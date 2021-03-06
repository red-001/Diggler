#include "Chatbox.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Game.hpp"
#include "GameWindow.hpp"
#include "render/gl/ProgramManager.hpp"
#include "ui/Manager.hpp"

namespace Diggler {

const Render::gl::Program *Chatbox::RenderProgram = nullptr;
GLint Chatbox::RenderProgram_coord = -1;
GLint Chatbox::RenderProgram_color = -1;
GLint Chatbox::RenderProgram_mvp = -1;

Chatbox::Chatbox(Game *G) : m_isChatting(false), G(G),
  m_posX(0), m_posY(0) {
  m_chatText = G->UIM->addManual<UI::Text>("", 2, 2);
  m_chatText->setPos(0, 0);
  if (RenderProgram == nullptr) {
    RenderProgram = G->PM->getProgram("2d", "color0");
    RenderProgram_coord = RenderProgram->att("coord");
    RenderProgram_color = RenderProgram->att("color");
    RenderProgram_mvp = RenderProgram->uni("mvp");
  }
  Vertex verts[6] = {
    {0.f, 0.f, 0.f, 0.f, 0.f, .5f},
    {100.f, 0.f, 0.f, 0.f, 0.f, .5f},
    {0.f, 100.f, 0.f, 0.f, 0.f, .5f},
    {100.f, 100.f, 0.f, 0.f, 0.f, .5f},
    {100.f, 0.f, 0.f, 0.f, 0.f, .5f},
    {0.f, 100.f, 0.f, 0.f, 0.f, .5f}
  };
  m_vbo.setData(verts, 6);
  { Render::gl::VAO::Config cfg = m_vao.configure();
    cfg.vertexAttrib(m_vbo, RenderProgram_coord, 2, GL_FLOAT, sizeof(Vertex), 0);
    cfg.vertexAttrib(m_vbo, RenderProgram_color, 4, GL_FLOAT, sizeof(Vertex), offsetof(Vertex, r));
    cfg.commit();
  }
}

Chatbox::~Chatbox() {
}

void Chatbox::setPosition(int x, int y) {
  m_posX = x;
  m_posY = y;
}

bool Chatbox::isChatting() const {
  return m_isChatting;
}

void Chatbox::setIsChatting(bool value) {
  m_chatString.clear();
  m_chatText->setText(m_chatString + '_');
  m_isChatting = value;
}

void Chatbox::addChatEntry(const std::string &text) {
  m_chatEntries.emplace_back();
  ChatEntry &entry = m_chatEntries.back();
  entry.date = system_clock::now();
  entry.text = G->UIM->addManual<UI::Text>(text);
  entry.height = entry.text->getSize().y;
}

Chatbox::ChatEntry::~ChatEntry() {
}

void Chatbox::handleChar(char32 unichar) {
  //getDebugStream() << unichar << std::endl;
  if (unichar >= ' ' && unichar <= '~') { // ASCII range
    // TODO: Update when libstdc++ supports locale codecvt facets
    //std::codecvt_utf8<char32_t> convert32;
    m_chatString.append(1, (char)unichar);
    m_chatText->setText(m_chatString + '_');
  }
}

void Chatbox::handleKey(int key, int scancode, int action, int mods) {
  (void)scancode; (void)mods;
  if (action == GLFW_PRESS && key == GLFW_KEY_BACKSPACE) {
    if (m_chatString.size() > 0) {
      m_chatString.erase(m_chatString.end()-1);
      m_chatText->setText(m_chatString + '_');
    }
  }
}

void Chatbox::render() {
  if (m_isChatting) {
    RenderProgram->bind();
    m_vao.bind();
    glUniformMatrix4fv(RenderProgram_mvp, 1, GL_FALSE, glm::value_ptr(*G->UIM->PM));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    m_vao.unbind();

    m_chatText->render();
  }
  
  int totalHeight = 0;
  for (const ChatEntry &entry : m_chatEntries)
    totalHeight += entry.height;
  totalHeight *= 2;
  glm::mat4 msgMatrix = glm::scale(glm::translate(*G->UIM->PM, glm::vec3(m_posX, m_posY+totalHeight, 0)), glm::vec3(2));
  for (const ChatEntry &entry : m_chatEntries) {
    msgMatrix = glm::translate(msgMatrix, glm::vec3(0, -entry.height, 0));
    entry.text->render(msgMatrix);
  }
}

}
