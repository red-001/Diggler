#ifndef DIGGLER_RENDER_GL_DELEGATE_GL_HPP
#define DIGGLER_RENDER_GL_DELEGATE_GL_HPP

#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "../../platform/Types.hpp"
#include "../../util/unique_function.hpp"
#include "OpenGL.hpp"

namespace Diggler {
namespace Render {
namespace gl {

class DelegateGL {
private:
  static DelegateGL instance;
  std::vector<Util::unique_function<void()>> operations;
  std::mutex operationsMutex;

public:
  static std::thread::id GLThreadId;

private:
  template<typename Func> static void push(Func &&func) {
    std::lock_guard<std::mutex> lock(instance.operationsMutex);
    instance.operations.emplace_back(std::move(func));
  }

public:
  static void texImage2D(GLuint texture, GLint level, GLint internalformat, GLsizei width,
    GLsizei height, GLenum format, GLenum type, std::unique_ptr<const uint8[]> &&data);

  static void texSubImage2D(GLuint texture, GLint level, GLint xoffset, GLint yoffset,
    GLsizei width, GLsizei height, GLenum format, GLenum type,
    std::unique_ptr<const uint8[]> &&data);

  static void execute();
};

}
}
}

#endif /* DIGGLER_RENDER_GL_DELEGATE_GL_HPP */
