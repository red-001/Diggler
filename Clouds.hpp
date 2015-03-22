#ifndef CLOUDS_HPP
#define CLOUDS_HPP
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "VBO.hpp"

namespace Diggler {

class Game;
class Program;
class Texture;

class Clouds {
private:
	static struct Renderer {
		const Program *prog;
		GLint att_coord, att_texcoord, uni_mvp, uni_texshift;
	} R;
	Texture **m_tex;
	VBO m_vbo;
	int m_layers;
	Game *G;
	struct Coord { uint8 x, y, z, u, v; };

public:
	Clouds(Game *G, int w, int h, int layers);
	void render(const glm::mat4 &transform);
	~Clouds();
};

}

#endif