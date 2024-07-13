#pragma once

#include <string>

#include <util/file_system.hpp>

namespace fast_chess::util {

inline std::string buildPath(const std::string &dir, const std::string &path) {
    std::string full_path = (dir == "." ? "" : dir) + path;

#ifndef NO_STD_FILESYSTEM
    // convert path to a filesystem path
    auto p    = std::filesystem::path(dir) / std::filesystem::path(path);
    full_path = p.string();
#endif

    return full_path;
}

}  // namespace fast_chess::util
