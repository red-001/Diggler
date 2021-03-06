#include "Program.hpp"

#include <thread>

#include "../../Platform.hpp"
#include "../../util/Log.hpp"

namespace Diggler {
namespace Render {
namespace gl {

using Util::Log;
using namespace Util::Logging::LogLevels;

static const char *TAG = "Program";

Program::Program(Shader* vsh, Shader* fsh) :
  vsh(vsh),
  fsh(fsh),
  id(0),
  linked(GL_FALSE) {
  
}

Program::Program(const std::string& vshPath, const std::string& fshPath)
  : id(0), fshPath(fshPath), vshPath(vshPath) {
  vsh = new Shader(Shader::Type::VERTEX);
  fsh = new Shader(Shader::Type::FRAGMENT);
  mustDestroy = true;
}

void Program::setPreludeLines(const std::vector<std::string> &lines) {
  vsh->setPreludeLines(lines);
  fsh->setPreludeLines(lines);
}


bool Program::link() {
  fsh->compileFromFile(fshPath);
  vsh->compileFromFile(vshPath);
  id = glCreateProgram();
  glAttachShader(id, vsh->getId());
  glAttachShader(id, fsh->getId());
  glLinkProgram(id);
  GLint linkStatus;
  glGetProgramiv(id, GL_LINK_STATUS, &linkStatus);
  linked = GLboolean(linkStatus);
  if (!linked) {
    Log(Error, TAG) << id << ':' << getError();
    return false;
  }

  // Default values
  GLint loc = glGetUniformLocation(id, "unicolor");
  if (loc != -1) {
    GLint prevId; glGetIntegerv(GL_CURRENT_PROGRAM, &prevId);
    glUseProgram(id);
    glUniform4f(loc, 1.f, 1.f, 1.f, 1.f);
    glUseProgram(prevId);
  }
  
  loc = glGetUniformLocation(id, "bloomThreshold");
  if (loc != -1) {
    GLint prevId; glGetIntegerv(GL_CURRENT_PROGRAM, &prevId);
    glUseProgram(id);
    glUniform1f(loc, .4f);
    glUseProgram(prevId);
  }

  // FIXME: remove this, fog needs to be dynamic
  loc = glGetUniformLocation(id, "fogStart");
  if (loc != -1) {
    GLint prevId; glGetIntegerv(GL_CURRENT_PROGRAM, &prevId);
    glUseProgram(id);
    glUniform1f(loc, 16.f);
    glUniform1f(glGetUniformLocation(id, "fogEnd"), 32.f);
    glUseProgram(prevId);
  }

  return true;
}

std::string Program::getError() const {
  GLint log_length = 0;
  glGetProgramiv(id, GL_INFO_LOG_LENGTH, &log_length);
  char* log = (char*)malloc(log_length);
  glGetProgramInfoLog(id, log_length, NULL, log);
  std::string ret(log);
  free(log);
  return ret;
}

GLuint Program::getId() const {
  return id;
}

GLint Program::att(const std::string &name) const {
  if (!linked) {
    Log(Error, TAG) << id << ": Not linked, failed attrib " << name;
    return 0;
  }
  GLint loc = glGetAttribLocation(id, name.c_str());
  if (loc == -1) {
#if DEBUG
    Log(Error, TAG) << vshPath << ": Couldn't bind attrib " << name;
#else
    Log(Error, TAG) << id << ":Couldn't bind attrib " << name;
#endif
    return 0;
  }
  return loc;
}

GLint Program::uni(const std::string &name) const {
  if (!linked) {
    Log(Error, TAG) << id << ": Not linked, failed uniform " << name;
    return 0;
  }
  GLint loc = glGetUniformLocation(id, name.c_str());
  if (loc == -1) {
#if DEBUG
    Log(Error, TAG) << vshPath << ": Couldn't bind uniform " << name;
#else
    Log(Error, TAG) << id << ":C ouldn't bind uniform " << name;
#endif
    return 0;
  }
  return loc;
}

void Program::bind() const {
  glUseProgram(id);
}

GLuint Program::getFShId() const {
  return fsh->getId();
}

GLuint Program::getVShId() const {
  return vsh->getId();
}

Program::~Program() {
  if (mustDestroy) {
    delete vsh; delete fsh;
  }
}

}
}
}
