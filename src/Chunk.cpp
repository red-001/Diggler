#include "Chunk.hpp"
#include "Platform.hpp"
#include "GlobalProperties.hpp"
#include "Game.hpp"
#include "content/Registry.hpp"
#include <cstring>
#include <cstddef>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <lzfx.h>
#include <fasthash.h>

#if CHUNK_INMEM_COMPRESS
	#include <cstdlib>
#endif

#define CXY (CX*CY)
#define I(x,y,z) (x+y*CX+z*CXY)

#define SHOW_CHUNK_UPDATES 1

namespace Diggler {

Chunk::Renderer Chunk::R = {0};
const Texture *Chunk::TextureAtlas = nullptr;

struct GLCoord {
	uint8 x, y, z, w;
	uint16 tx, ty;
	float r, g, b;
};

constexpr float Chunk::CullSphereRadius;
constexpr float Chunk::MidX, Chunk::MidY, Chunk::MidZ;

void Chunk::Data::clear() {
	memset(this, 0, AllocaSize);
}

Chunk::ChangeHelper::ChangeHelper(Chunk &C) :
	C(C), enabled(true) {
	m_changes.reserve(8);
}

void Chunk::ChangeHelper::add(int x, int y, int z) {
	if (!enabled)
		return;
	m_changes.emplace_back(x, y, z);
}

void Chunk::ChangeHelper::flush(Net::OutMessage &msg) {
	msg.writeU8(m_changes.size());
	for (glm::ivec3 &c : m_changes) {
		msg.writeU8(c.x);
		msg.writeU8(c.y);
		msg.writeU8(c.z);
		msg.writeU16(C.data->id[I(c.x, c.y, c.z)]);
		msg.writeU16(C.data->data[I(c.x, c.y, c.z)]);
		msg.writeU16(C.data->light[I(c.x, c.y, c.z)]);
	}
	m_changes.clear();
}

int Chunk::ChangeHelper::count() const {
	return m_changes.size();
}

bool Chunk::ChangeHelper::empty() const {
	return m_changes.empty();
}

void Chunk::ChangeHelper::discard() {
	m_changes.clear();
}

Chunk::Chunk(Game *G, WorldRef W, int X, int Y, int Z) :
	wcx(X), wcy(Y), wcz(Z),
	G(G), W(W), vbo(nullptr), data(nullptr), data2(nullptr),
	state(State::Unavailable),
	CH(*this) {
	dirty = true;
	data = new Data;
	data->clear();
	
	if (GlobalProperties::IsClient) {
		vbo = new VBO;
		ibo = new VBO;
		if (R.prog == nullptr) {
			loadShader();
			TextureAtlas = G->CR->getAtlas();
		}
	}
	if (GlobalProperties::IsServer) {
		data2 = new Data;
		data2->clear();
	}
#if CHUNK_INMEM_COMPRESS
	imcUnusedSince = 0;
	imcData = nullptr;
#endif
	calcMemUsage();
}

void Chunk::loadShader() {
	bool w = G->RP->wavingLiquids;
	ProgramManager::FlagsT flags = PM_3D | PM_TEXTURED | PM_COLORED | PM_FOG;
	if (w)
		flags |= PM_WAVE;
	R.prog = G->PM->getProgram(flags);
	R.att_coord = R.prog->att("coord");
	R.att_color = R.prog->att("color");
	R.att_texcoord = R.prog->att("texcoord");
	R.att_wave = w ? R.prog->att("wave") : -1;
	R.uni_mvp = R.prog->uni("mvp");
	R.uni_unicolor = R.prog->uni("unicolor");
	R.uni_fogStart = R.prog->uni("fogStart");
	R.uni_fogEnd = R.prog->uni("fogEnd");
	R.uni_time = w ? R.prog->uni("time") : -1;
}

void Chunk::calcMemUsage() {
	blkMem = 0;
#if CHUNK_INMEM_COMPRESS
	if (imcData) {
		blkMem = imcSize;
		return;
	}
#endif
	if (data)
		blkMem += AllocaSize;
	if (data2)
		blkMem += AllocaSize;
}

#if CHUNK_INMEM_COMPRESS
void Chunk::imcCompress() {
	if (mut.try_lock()) {
		uint isize = AllocaSize, osize = isize;
		imcData = std::malloc(osize);
		lzfx_compress(data, isize, imcData, &osize);
		imcData = std::realloc(imcData, osize);
		imcSize = osize;
		delete data;
		data = nullptr;
		calcMemUsage();
		mut.unlock();
	}
}

void Chunk::imcUncompress() {
	if (!imcData) {
		imcUnusedSince = G->TimeMs;
		return;
	}
	mut.lock();
	uint isize = imcSize, osize = AllocaSize;
	imcUnusedSince = G->TimeMs;
	data = new Data;
	lzfx_decompress(imcData, isize, data, &osize);
	std::free(imcData);
	imcData = nullptr;
	calcMemUsage();
	mut.unlock();
}
#endif

void Chunk::onRenderPropertiesChanged() {
	loadShader();
}

Chunk::~Chunk() {
	delete data; delete data2;
	if (GlobalProperties::IsClient) {
		delete vbo; delete ibo;
	}
#if CHUNK_INMEM_COMPRESS
	std::free(imcData);
#endif
	// getDebugStream() << W->id << '.' << wcx << ',' << wcy << ',' << wcz << " destruct" << std::endl;
}

void Chunk::notifyChange(int x, int y, int z) {
	if (state != State::Ready)
		return;

	CH.add(x, y, z);
	if (GlobalProperties::IsClient) {
		int u = x==CX-1?1:(x==0)?-1:0,
			v = y==CY-1?1:(y==0)?-1:0,
			w = z==CZ-1?1:(z==0)?-1:0;
		ChunkRef nc;
		if (u && (nc = W->getChunk(wcx+u, wcy, wcz)))
			nc->markAsDirty();
		if (v && (nc = W->getChunk(wcx, wcy+v, wcz)))
			nc->markAsDirty();
		if (w && (nc = W->getChunk(wcx, wcy, wcz+w)))
			nc->markAsDirty();
	}
}

void Chunk::setBlock(int x, int y, int z, BlockId id, BlockData data, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && G)
		return (void)W->setBlock(wcx * CX + x, wcy * CY + y, wcz * CZ + z, id, data, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : this->data;
	buf->id[I(x,y,z)] = id;
	buf->data[I(x,y,z)] = data;
	notifyChange(x, y, z);
}

void Chunk::setBlockId(int x, int y, int z, BlockId id, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && W)
		return (void)W->setBlockId(wcx * CX + x, wcy * CY + y, wcz * CZ + z, id, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : data;
	buf->id[I(x,y,z)] = id;
	notifyChange(x, y, z);
}

void Chunk::setBlockData(int x, int y, int z, BlockData data, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && W)
		return (void)W->setBlockData(wcx * CX + x, wcy * CY + y, wcz * CZ + z, data, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : this->data;
	buf->data[I(x,y,z)] = data;
	notifyChange(x, y, z);
}
 
BlockId Chunk::getBlockId(int x, int y, int z, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && W)
		return W->getBlockId(wcx * CX + x, wcy * CY + y, wcz * CZ + z, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : data;
	return buf->id[I(x,y,z)];
}

BlockData Chunk::getBlockData(int x, int y, int z, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && W)
		return W->getBlockData(wcx * CX + x, wcy * CY + y, wcz * CZ + z, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : data;
	BlockData d = buf->data[I(x,y,z)];
	if (d & BlockMetadataBit) {
		// TODO Implement data in metadata
		return 0;
	}
	return d;
}

bool Chunk::blockHasMetadata(int x, int y, int z, bool buf2) {
	if ((x < 0 || y < 0 || z < 0 || x >= CX || y >= CY || z >= CZ) && W)
		return W->blockHasMetadata(wcx * CX + x, wcy * CY + y, wcz * CZ + z, buf2);
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	Data *buf = buf2 ? data2 : data;
	return buf->data[I(x,y,z)] & BlockMetadataBit;
}

void Chunk::markAsDirty() {
	dirty = true;
}

void Chunk::updateServerPrepare() {
	memcpy(data2, data, AllocaSize);
}

void Chunk::updateServer() {
	mut.lock();
	for (int x=0; x < CX; x++)
		for (int y=0; y < CY; y++)
			for (int z=0; z < CZ; z++) {
				// TODO
#if 0
				if (data->id[I(x,y,z)] == BlockTypeLava) {
					BlockType under = get(x, y-1, z);
					if (under == BlockType::Air) {
						set2(x, y-1, z, BlockTypeLava);
					} else if (under != BlockTypeLava) {
						if (get(x+1, y, z) == BlockType::Air)
							set2(x+1, y, z, BlockTypeLava);
						if (get(x-1, y, z) == BlockType::Air)
							set2(x-1, y, z, BlockTypeLava);
						if (get(x, y, z+1) == BlockType::Air)
							set2(x, y, z+1, BlockTypeLava);
						if (get(x, y, z-1) == BlockType::Air)
							set2(x, y, z-1, BlockTypeLava);
					}
				}
#endif
			}
	mut.unlock();
}

void Chunk::updateServerSwap() {
	std::swap(data, data2);
}

struct RGB { float r, g, b; };
void Chunk::updateClient() {
#if CHUNK_INMEM_COMPRESS
	imcUncompress();
#endif
	mut.lock();
	ContentRegistry &CR = *G->CR;
	GLCoord		vertex[CX * CY * CZ * 6 /* faces */ * 4 /* vertices */ / 2 /* face removing (HSR) makes a lower vert max */];
	GLushort	idxOpaque[CX * CY * CZ * 6 /* faces */ * 6 /* indices */ / 2 /* HSR */],
				idxTransp[CX*CY*CZ*6*6/2];
	int v = 0, io = 0, it = 0;

	bool hasWaves = G->RP->wavingLiquids;
	BlockId bt, bu /*BlockUp*/, bn /*BlockNear*/;
	bool mayDisp;
	const AtlasCreator::Coord *tc;
	for(int8 x = 0; x < CX; x++) {
		for(int8 y = 0; y < CY; y++) {
			for(int8 z = 0; z < CZ; z++) {
				bt = data->id[I(x,y,z)];

				// Empty block?
				if (bt == Content::BlockAirId ||
					bt == Content::BlockIgnoreId)
					continue;

#if 0
				BlockType
					/* -X face*/
					bNNZ = get(x-1, y-1, z),
					bNPZ = get(x-1, y+1, z),
					bNZN = get(x-1, y, z-1),
					bNZP = get(x-1, y, z+1),
					/* +X face*/
					bPNZ = get(x+1, y-1, z),
					bPPZ = get(x+1, y+1, z),
					bPZN = get(x+1, y, z-1),
					bPZP = get(x+1, y, z+1),
					/* Top & bottom */
					bZPN = get(x, y+1, z-1),
					bZPP = get(x, y+1, z+1),
					bZNN = get(x, y-1, z-1),
					bZNP = get(x, y+1, z+1);
					
					RGB bl = {.6f, .6f, .6f}, br = {.6f, .6f, .6f},
						tl = {.6f, .6f, .6f}, tr = {.6f, .6f, .6f};
					if (bNZN == BlockTypeLava || bNNZ == BlockTypeLava) { bl.r = 1.6f; bl.g = 1.2f; }
					if (bNNZ == BlockTypeLava || bNZP == BlockTypeLava) { br.r = 1.6f; br.g = 1.2f; }
					if (bNZP == BlockTypeLava || bNPZ == BlockTypeLava) { tr.r = 1.6f; tr.g = 1.2f; }
					if (bNPZ == BlockTypeLava || bNZN == BlockTypeLava) { tl.r = 1.6f; tl.g = 1.2f; }
					vertex[v++] = {x,     y,     z,     tc->x, tc->v, bl.r, bl.g, bl.b};
					vertex[v++] = {x,     y,     z + 1, tc->u, tc->v, br.r, br.g, br.b};
					vertex[v++] = {x,     y + 1, z,     tc->x, tc->y, tl.r, tl.g, tl.b};
					vertex[v++] = {x,     y + 1, z + 1, tc->u, tc->y, tr.r, tr.g, tr.b};
#endif
				// FIXME: dirtydirty workaround
				const BlockId BlockTypeLava = 998;
				const BlockId BlockTypeShock = 999;

				uint8 w;
				if (hasWaves) {
					w = (bt == BlockTypeLava && getBlockId(x, y+1, z) != BlockTypeLava) ? 8 : 0;
					bu = getBlockId(x, y+1, z);
					mayDisp = (bt != BlockTypeLava) || (bt == BlockTypeLava && bu == BlockTypeLava);
				} else {
					w = 0;
					mayDisp = false;
				}

				GLushort *index; int i; bool transp = ContentRegistry::isTransparent(bt);
				if (transp) {
					index = idxTransp;
					i = it;
				} else {
					index = idxOpaque;
					i = io;
				}

				// View from negative x
				bn = getBlockId(x - 1, y, z);
				if ((mayDisp && bn == BlockTypeLava && getBlockId(x-1, y+1, z) != BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					tc = CR.gTC(bt, FaceDirection::XDec);
					vertex[v++] = {x,     y,     z,     0, tc->x, tc->v, .6f, .6f, .6f};
					vertex[v++] = {x,     y,     z + 1, 0, tc->u, tc->v, .6f, .6f, .6f};
					vertex[v++] = {x,     y + 1, z,     w, tc->x, tc->y, .6f, .6f, .6f};
					vertex[v++] = {x,     y + 1, z + 1, w, tc->u, tc->y, .6f, .6f, .6f};
				}

				// View from positive x
				bn = getBlockId(x + 1, y, z);
				if ((mayDisp && bn == BlockTypeLava && getBlockId(x+1, y+1, z) != BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					tc = CR.gTC(bt, FaceDirection::XInc);
					vertex[v++] = {x + 1, y,     z,     0, tc->u, tc->v, .6f, .6f, .6f};
					vertex[v++] = {x + 1, y + 1, z,     w, tc->u, tc->y, .6f, .6f, .6f};
					vertex[v++] = {x + 1, y,     z + 1, 0, tc->x, tc->v, .6f, .6f, .6f};
					vertex[v++] = {x + 1, y + 1, z + 1, w, tc->x, tc->y, .6f, .6f, .6f};
				}

				// Negative Y
				bn = getBlockId(x, y - 1, z);
				if ((hasWaves && bn == BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					float shade = (data->id[I(x,y,z)] == BlockTypeShock) ? 1.5f : .2f;;
					tc = CR.gTC(bt, FaceDirection::YDec);
					vertex[v++] = {x,     y,     z, 0, tc->u, tc->v, shade, shade, shade};
					vertex[v++] = {x + 1, y,     z, 0, tc->u, tc->y, shade, shade, shade};
					vertex[v++] = {x,     y, z + 1, 0, tc->x, tc->v, shade, shade, shade};
					vertex[v++] = {x + 1, y, z + 1, 0, tc->x, tc->y, shade, shade, shade};
				}

				// Positive Y
				bn = getBlockId(x, y + 1, z);
				if ((hasWaves && bt == BlockTypeLava && bu != BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					tc = CR.gTC(bt, FaceDirection::YInc);
					vertex[v++] = {x,     y + 1,     z, w, tc->x, tc->v, .8f, .8f, .8f};
					vertex[v++] = {x,     y + 1, z + 1, w, tc->u, tc->v, .8f, .8f, .8f};
					vertex[v++] = {x + 1, y + 1,     z, w, tc->x, tc->y, .8f, .8f, .8f};
					vertex[v++] = {x + 1, y + 1, z + 1, w, tc->u, tc->y, .8f, .8f, .8f};
				}

				// Negative Z
				bn = getBlockId(x, y, z - 1);
				if ((mayDisp && bn == BlockTypeLava && getBlockId(x, y+1, z-1) != BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					tc = CR.gTC(bt, FaceDirection::ZDec);
					vertex[v++] = {x,     y,     z, 0, tc->u, tc->v, .4f, .4f, .4f};
					vertex[v++] = {x,     y + 1, z, w, tc->u, tc->y, .4f, .4f, .4f};
					vertex[v++] = {x + 1, y,     z, 0, tc->x, tc->v, .4f, .4f, .4f};
					vertex[v++] = {x + 1, y + 1, z, w, tc->x, tc->y, .4f, .4f, .4f};
				}

				// Positive Z
				bn = getBlockId(x, y, z + 1);
				if ((mayDisp && bn == BlockTypeLava && getBlockId(x, y+1, z+1) != BlockTypeLava)
					|| ContentRegistry::isFaceVisible(bt, bn)) {
					index[i++] = v; index[i++] = v+1; index[i++] = v+2;
					index[i++] = v+2; index[i++] = v+1; index[i++] = v+3;
					tc = CR.gTC(bt, FaceDirection::ZInc);
					vertex[v++] = {x,     y,     z + 1, 0, tc->x, tc->v, .4f, .4f, .4f};
					vertex[v++] = {x + 1, y,     z + 1, 0, tc->u, tc->v, .4f, .4f, .4f};
					vertex[v++] = {x,     y + 1, z + 1, w, tc->x, tc->y, .4f, .4f, .4f};
					vertex[v++] = {x + 1, y + 1, z + 1, w, tc->u, tc->y, .4f, .4f, .4f};
				}

				if (transp) {
					it = i;
				} else {
					io = i;
				}
			}
		}
	}

	vertices = v;
	vbo->setData(vertex, v);
	ibo->setSize((io+it) * sizeof(*idxOpaque));
	ibo->setSubData(idxOpaque, 0, io);
	ibo->setSubData(idxTransp, io, it);
	indicesOpq = io;
	indicesTpt = it;
	dirty = false;
	mut.unlock();
}

void Chunk::render(const glm::mat4 &transform) {
#if SHOW_CHUNK_UPDATES
	glUniform4f(R.uni_unicolor, 1.f, dirty ? 0.f : 1.f, dirty ? 0.f : 1.f, 1.f);
#endif
	if (dirty)
		updateClient();
	if (!indicesOpq)
		return;

	glUniformMatrix4fv(R.uni_mvp, 1, GL_FALSE, glm::value_ptr(transform));
	vbo->bind();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo->id);
	glVertexAttribPointer(R.att_coord, 3, GL_BYTE, GL_FALSE, sizeof(GLCoord), 0);
	glVertexAttribPointer(R.att_wave, 1, GL_BYTE, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, w));
	glVertexAttribPointer(R.att_texcoord, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, tx));
	glVertexAttribPointer(R.att_color, 3, GL_FLOAT, GL_FALSE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, r));
	glDrawElements(GL_TRIANGLES, indicesOpq, GL_UNSIGNED_SHORT, nullptr);
}

void Chunk::renderTransparent(const glm::mat4 &transform) {
	if (!indicesTpt)
		return;

	// Here we really need to pass the matrix again since the call is made in a second render pass
	glUniformMatrix4fv(R.uni_mvp, 1, GL_FALSE, glm::value_ptr(transform));
	vbo->bind();
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo->id);
	glVertexAttribPointer(R.att_coord, 3, GL_BYTE, GL_FALSE, sizeof(GLCoord), 0);
	glVertexAttribPointer(R.att_wave, 1, GL_BYTE, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, w));
	glVertexAttribPointer(R.att_texcoord, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, tx));
	glVertexAttribPointer(R.att_color, 3, GL_FLOAT, GL_FALSE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, r));
	glDrawElements(GL_TRIANGLES, indicesTpt, GL_UNSIGNED_SHORT, (GLvoid*)(indicesOpq*sizeof(GLshort)));
}

void Chunk::write(OutStream &os) const {
	const uint dataSize = Chunk::AllocaSize;
	uint compressedSize;
	byte *compressed = new byte[dataSize];

	const void *chunkData = data;
	compressedSize = dataSize;
	int rz = lzfx_compress(chunkData, dataSize, compressed, &compressedSize);
	if (rz < 0) {
		getErrorStream() << "Failed compressing Chunk[" << wcx << ',' << wcy <<
			' ' << wcz << ']' << std::endl;
	} else {
		os.writeU16(compressedSize);
		os.writeData(compressed, compressedSize);
	}
	os.writeU32(fasthash32(chunkData, dataSize, 0xFA0C778C));

	delete[] compressed;
}

void Chunk::read(InStream &is) {
	uint compressedSize = is.readU16();
	const uint targetDataSize = Chunk::AllocaSize;
	byte *compressedData = new byte[compressedSize];
	is.readData(compressedData, compressedSize);

	uint outLen = targetDataSize;
	int rz = lzfx_decompress(compressedData, compressedSize, data, &outLen);
	if (rz < 0 || outLen != targetDataSize) {
		if (rz < 0) {
			getErrorStream() << "Chunk[" << wcx << ',' << wcy << ' ' << wcz <<
				"] LZFX decompression failed" << std::endl;
		} else {
			getErrorStream() << "Chunk[" << wcx << ',' << wcy << ' ' << wcz <<
				"] has bad size " << outLen << '/' << targetDataSize << std::endl;
		}
	}
	if (is.readU32() != fasthash32(data, outLen, 0xFA0C778C)) {
		getErrorStream() << "Chunk[" << wcx << ',' << wcy << ' ' << wcz <<
				"] decompression gave bad chunk content" << std::endl;
	}

	delete[] compressedData;
	markAsDirty();

	{ ChunkRef nc;
		nc = W->getChunk(wcx+1, wcy, wcz); if (nc) nc->markAsDirty();
		nc = W->getChunk(wcx-1, wcy, wcz); if (nc) nc->markAsDirty();
		nc = W->getChunk(wcx, wcy+1, wcz); if (nc) nc->markAsDirty();
		nc = W->getChunk(wcx, wcy-1, wcz); if (nc) nc->markAsDirty();
		nc = W->getChunk(wcx, wcy, wcz+1); if (nc) nc->markAsDirty();
		nc = W->getChunk(wcx, wcy, wcz-1); if (nc) nc->markAsDirty();
	}
}

void Chunk::recv(Net::InMessage &M) {
	return read(M);
}

void Chunk::send(Net::OutMessage &M) const {
	return write(M);
}

}