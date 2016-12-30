#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <vector>
#include <string>
#include <iostream>

#include "platform/BuildInfo.hpp"
#include "platform/FastRand.hpp"
#include "platform/FourCC.hpp"
#include "platform/fs.hpp"
#include "platform/Math.hpp"
#include "platform/Types.hpp"

namespace Diggler {

namespace proc {
  /// @returns The executable's absolute path
  std::string getExecutablePath();

  /// @returns The executable's absolute path directory, including the end slash (/)
  std::string getExecutableDirectory();
}

static constexpr const char* const_strrchr(char const * s, int c) {
  return *s == static_cast<char>(c) && (!*s || !const_strrchr(s + 1, c))? s
    : !*s ? nullptr
    : const_strrchr(s + 1, c);
}

static constexpr const char *const_filename() {
#ifdef __FILENAME__
  return __FILENAME__;
#else
  return const_strrchr(__FILE__, '/') ? const_strrchr(__FILE__, '/') + 1 : __FILE__;
#endif
}

/// @returns The system's error output stream
std::ostream& getErrorStreamRaw();
#ifdef IN_IDE_PARSER
std::ostream& getErrorStream();
#else
#define getErrorStream() getErrorStreamRaw() << const_filename() << ':' << __LINE__ << ' '
#endif

/// @returns The system's debug output stream
std::ostream& getDebugStreamRaw();
#ifdef IN_IDE_PARSER
std::ostream& getDebugStream();
#else
#define getDebugStream() getDebugStreamRaw() << const_filename() << ':' << __LINE__ << ' '
#endif

/// @returns The system's output stream
std::ostream& getOutputStreamRaw();
#ifdef IN_IDE_PARSER
std::ostream& getOutputStream();
#else
#define getOutputStream() getOutputStreamRaw() << const_filename() << ':' << __LINE__ << ' '
#endif

extern const char *UserdataDirsName;

std::string getConfigDirectory();
std::string getCacheDirectory();


/// @returns The absolute assets directory path
std::string getAssetsDirectory(const std::string &type);

/// @returns The absolute assets directory path
std::string getAssetsDirectory();

/// @returns The absolute asset path
std::string getAssetPath(const std::string &name);

/// @returns The absolute asset path
std::string getAssetPath(const std::string &type, const std::string &name);

}

#endif
