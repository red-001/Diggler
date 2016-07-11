#include "WorldRenderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../content/Registry.hpp"
#include "../../Chunk.hpp"
#include "../../Game.hpp"
#include "../../World.hpp"

namespace Diggler {
namespace Render {
namespace gl {

using GLCoord = Chunk::Vertex;

GLWorldRenderer::GLWorldRenderer(Game *G) :
  G(G) {
  loadShader();
  TextureAtlas = G->CR->getAtlas();
}

GLWorldRenderer::~GLWorldRenderer() {
}

void GLWorldRenderer::loadShader() {
  bool w = G->RP->wavingLiquids;
  ProgramManager::FlagsT flags = PM_3D | PM_TEXTURED | PM_COLORED | PM_FOG;
  if (w)
    flags |= PM_WAVE;
  prog = G->PM->getProgram(flags);
  att_coord = prog->att("coord");
  att_color = prog->att("color");
  att_texcoord = prog->att("texcoord");
  att_wave = w ? prog->att("wave") : -1;
  uni_mvp = prog->uni("mvp");
  uni_unicolor = prog->uni("unicolor");
  uni_fogStart = prog->uni("fogStart");
  uni_fogEnd = prog->uni("fogEnd");
  uni_time = w ? prog->uni("time") : -1;
}

void GLWorldRenderer::registerChunk(Chunk *c) {
  if (c == nullptr) {
    return;
  }
  setRendererData(c, reinterpret_cast<uintptr_t>(new ChunkEntry));
}

void GLWorldRenderer::updateChunk(Chunk *c, Chunk::Vertex *vertices, uint vertCount,
  uint16 *indicesOpq, uint idxOpqCount, uint16 *indicesTpt, uint idxTptCount) {
  ChunkEntry &ce = *reinterpret_cast<ChunkEntry*>(getRendererData(c));
  ce.vbo.setDataKeepSize(vertices, vertCount, GL_DYNAMIC_DRAW);
  ce.ibo.resizeGrow(sizeof(uint16) * (idxOpqCount + idxTptCount));
  ce.ibo.setSubData(indicesOpq, 0, idxOpqCount);
  ce.ibo.setSubData(indicesTpt, idxOpqCount, idxTptCount);
  ce.indicesOpq = idxOpqCount;
  ce.indicesTpt = idxTptCount;
}

void GLWorldRenderer::unregisterChunk(Chunk *c) {
  if (c == nullptr) {
    return;
  }
  delete reinterpret_cast<ChunkEntry*>(getRendererData(c));
}

void GLWorldRenderer::render(RenderParams &rp) {
  if (prog == nullptr)
    return;
  //lastVertCount = 0;
  prog->bind();
  glUniform1f(uni_fogStart, G->RP->fogStart);
  glUniform1f(uni_fogEnd, G->RP->fogEnd);
  glUniform1f(uni_time, G->Time);
  glEnableVertexAttribArray(att_coord);
  glEnableVertexAttribArray(att_texcoord);
  glEnableVertexAttribArray(att_color);
  glEnableVertexAttribArray(att_wave);
  TextureAtlas->bind();

  const static glm::vec3 cShift(Chunk::MidX, Chunk::MidY, Chunk::MidZ);
  glm::mat4 chunkTransform;
  ChunkRef c;
  for (auto pair : *rp.world) {
    const glm::ivec3 &pos = pair.first;
    ChunkWeakRef &cwr = pair.second;
    if ((c = cwr.lock())) {
#if CHUNK_INMEM_COMPRESS
      if (!c->imcData && (G->TimeMs - c->imcUnusedSince) > CHUNK_INMEM_COMPRESS_DELAY)
        c->imcCompress();
#endif
      ChunkEntry &ce = *reinterpret_cast<ChunkEntry*>(getRendererData(c.get()));
      glm::vec3 translate(pos.x * CX, pos.y * CY, pos.z * CZ);
      if (rp.frustum.sphereInFrustum(translate + cShift, Chunk::CullSphereRadius)) {
        chunkTransform = glm::translate(rp.transform, translate);
#if SHOW_CHUNK_UPDATES
        glUniform4f(uni_unicolor, 1.f, dirty ? 0.f : 1.f, dirty ? 0.f : 1.f, 1.f);
#endif
        if (c->isDirty())
          c->updateClient();
        if (!ce.indicesOpq)
          continue;

        glUniformMatrix4fv(uni_mvp, 1, GL_FALSE, glm::value_ptr(chunkTransform));
        ce.vbo.bind();
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ce.ibo);
        glVertexAttribPointer(att_coord, 3, GL_BYTE, GL_FALSE, sizeof(GLCoord), 0);
        glVertexAttribPointer(att_wave, 1, GL_BYTE, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, w));
        glVertexAttribPointer(att_texcoord, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, tx));
        glVertexAttribPointer(att_color, 3, GL_FLOAT, GL_FALSE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, r));
        glDrawElements(GL_TRIANGLES, ce.indicesOpq, GL_UNSIGNED_SHORT, nullptr);
        //lastVertCount += cc->vertices;
      }
    }
  }

  glDisableVertexAttribArray(att_wave);
  glDisableVertexAttribArray(att_color);
  glDisableVertexAttribArray(att_texcoord);
  glDisableVertexAttribArray(att_coord);
}

/*void GLWorldRenderer::renderTransparent(RenderParams &rp) {
  if (!indicesTpt)
    return;

  // Here we really need to pass the matrix again since the call is made in a second render pass
  glUniformMatrix4fv(uni_mvp, 1, GL_FALSE, glm::value_ptr(transform));
  vbo->bind();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo->id());
  glVertexAttribPointer(att_coord, 3, GL_BYTE, GL_FALSE, sizeof(GLCoord), 0);
  glVertexAttribPointer(att_wave, 1, GL_BYTE, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, w));
  glVertexAttribPointer(att_texcoord, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, tx));
  glVertexAttribPointer(att_color, 3, GL_FLOAT, GL_FALSE, sizeof(GLCoord), (GLvoid*)offsetof(GLCoord, r));
  glDrawElements(GL_TRIANGLES, indicesTpt, GL_UNSIGNED_SHORT, (GLvoid*)(indicesOpq*sizeof(GLshort)));
}*/

}
}
}