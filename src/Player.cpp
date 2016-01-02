#include "Texture.hpp"
#include "Player.hpp"
#include "Game.hpp"
#include "GlobalProperties.hpp"
#include <glm/gtc/matrix_transform.hpp> 
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

namespace Diggler {

Player::TexInfo ***Player::TexInfos = nullptr;
Player::Renderer Player::R = {0};

const char* Player::getTeamNameLowercase(Player::Team t) {
	switch (t) {
		case Team::Blue:
			return "blue";
		case Team::Red:
			return "red";
		default:
			return "";
	}
}

const char* Player::getToolNameLowercase(Player::Tools t) {
	switch (t) {
		case Tools::ConstructionGun:
		case Tools::DeconstructionGun:
			return "construction";
		case Tools::Detonator:
			return "detonator";
		case Tools::Pickaxe:
			return "pickaxe";
		case Tools::ProspectingRadar:
			return "radar";
		default:
			return "";
	}
}

int Player::getMaxOre(Class c) {
	switch (c) {
		case Class::Engineer:
			return 350;
		case Class::Miner:
		case Class::Prospector:
		case Class::Sapper:
			return 200;
	}
	return 0;
}

int Player::getMaxWeight(Class c) {
	switch (c) {
		case Class::Miner:
			return 8;
		case Class::Engineer:
		case Class::Prospector:
		case Class::Sapper:
			return 4;
	}
	return 0;
}

Player::Player(Game *G) : team(Team::Red),
	playerclass(Class::Prospector), tool(Tools::Pickaxe), G(G),
	position(0), velocity(0), accel(0), angle(0), toolUseTime(0),
	isAlive(true), ore(0), loot(0) {
	if (GlobalProperties::IsClient) {
		if (TexInfos == nullptr) {
			R.prog = G->PM->getProgram(PM_3D | PM_TEXTURED | PM_FOG | PM_DISCARD);
			R.att_coord = R.prog->att("coord");
			R.att_texcoord = R.prog->att("texcoord");
			R.uni_mvp = R.prog->uni("mvp");
			R.uni_unicolor = R.prog->uni("unicolor");
			R.uni_fogStart = R.prog->uni("fogStart");
			R.uni_fogEnd = R.prog->uni("fogEnd");

			TexInfos = new TexInfo**[Team::LAST];
			for (uint8 t=0; t < (uint8)Team::LAST; t++) {
				TexInfos[t] = new TexInfo*[Tools::LAST];
				for (uint8 tool=0; tool < (uint8)Tools::LAST; tool++) {
					std::string aa = std::string("tex_sprite_") + getTeamNameLowercase(team) + '_' + getToolNameLowercase(this->tool) + ".png";
					TexInfos[t][tool] = new TexInfo;
					TexInfos[t][tool]->tex = new Texture(
						getAssetPath("sprites", std::string("tex_sprite_") + getTeamNameLowercase((Team)t) + '_' + getToolNameLowercase((Tools)tool) + ".png"),
						Texture::PixelFormat::RGBA);
					static const float
						w = 24.f, h = 32.f,
						sheetW = 128.f, sheetH = 128.f,
						wt = w/sheetW, ht = h/sheetH, ratio = h/w,
						sz = .9f, szH = (sz*2)*ratio;
					for (int side=0; side < 4; ++side) {
						float ya = 1-(side*ht), yb = 1-((side+1)*ht);
						for (int action=0; action < 4; ++action) {
							float xa = action*wt, xb = (action+1)*wt;
							float coords[6*5] = {
								-sz, .0f, 0.0f,  xa, ya,
								 sz, .0f, 0.0f,  xb, ya,
								 sz, szH, 0.0f,  xb, yb,

								-sz, szH, 0.0f,  xa, yb,
								-sz, .0f, 0.0f,  xa, ya,
								 sz, szH, 0.0f,  xb, yb,
							};
							TexInfos[t][tool]->side[side].vbos[action] = new VBO;
							TexInfos[t][tool]->side[side].vbos[action]->setData(coords, 6*5);
						}
					}
				}
			}
		}
	}
}

using std::swap;
Player::Player(Player &&p) {
	*this = std::move(p);
}

Player& Player::operator=(Player &&p) {
	swap(team, p.team);
	swap(playerclass, p.playerclass);
	swap(tool, p.tool);
	swap(direction, p.direction);
	swap(G, p.G);
	swap(position, p.position);
	swap(velocity, p.velocity);
	swap(accel, p.accel);
	swap(name, p.name);
	swap(id, p.id);
	swap(P, p.P);
	swap(isAlive, p.isAlive);
	return *this;
}

Player::~Player() {
}

void Player::setPosVel(const glm::vec3 &pos, const glm::vec3 &vel, const glm::vec3 &acc) {
	lastPosition = m_predictPos;
	position = m_predictPos = pos;
	velocity = vel;
	accel = acc;
	m_lastPosTime = G->Time;
}

void Player::update(const float &delta) {
	velocity += accel * delta;
	m_predictPos = glm::mix(lastPosition, position, std::min((G->Time-m_lastPosTime)*G->PlayerPosUpdateFreq, 1.0));
}

static inline int getSide(float angle) {
	if (angle >= 2*M_PI-M_PI/4 || angle < M_PI/4)
		return 1; // Back
	if (angle >= M_PI/4 && angle < M_PI-M_PI/4)
		return 2; // Left
	if (angle >= M_PI-M_PI/4 && angle >= M_PI+M_PI/4)
		return 0; // Right
	return 3; // Front
}

void Player::render(const glm::mat4 &transform) const {
	R.prog->bind();
	TexInfos[(uint8)team][(uint8)tool]->tex->bind();
	int action = 1;
	if (velocity.x < -.5f || velocity.x > .5f ||
		velocity.z < -.5f || velocity.z > .5f)
		action = ((int)(G->Time*3))%2*2;
	if (G->Time-1 < toolUseTime)
		action = 3;
	TexInfos[(uint8)team][(uint8)tool]->side[
		getSide(rmod(atan2(position.x-G->LP->position.x, position.z-G->LP->position.z)-angle, M_PI*2))]
		.vbos[action]->bind();
	glEnableVertexAttribArray(R.att_texcoord);
	glEnableVertexAttribArray(R.att_coord);
	glUniform1f(R.uni_fogStart, G->RP->fogStart);
	glUniform1f(R.uni_fogEnd, G->RP->fogEnd);
	glm::vec3 &lpPos = G->LP->position;
	float angle = atan2(lpPos.x-m_predictPos.x, lpPos.z-m_predictPos.z);
	static const glm::vec3 vecY(0.0, 1.0, 0.0);
	glUniformMatrix4fv(R.uni_mvp, 1, GL_FALSE, glm::value_ptr(
		glm::translate(transform, m_predictPos) * glm::rotate(angle, vecY)));
	glVertexAttribPointer(R.att_coord, 3, GL_FLOAT, GL_FALSE, 5*sizeof(float), 0);
	glVertexAttribPointer(R.att_texcoord, 2, GL_FLOAT, GL_FALSE, 5*sizeof(float), (GLvoid*)(3*sizeof(float)));
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(R.att_coord);
	glDisableVertexAttribArray(R.att_texcoord);
}

void Player::setDead(bool dead, DeathReason dr, bool send) {
	isAlive = !dead;
	deathReason = dr;
}

}