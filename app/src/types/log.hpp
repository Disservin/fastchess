#pragma once

#include <string>

#include <core/logger/logger.hpp>

namespace fastchess::config {

struct Log {
    std::string file;
    Logger::Level level = Logger::Level::WARN;
    bool append_file    = true;
    bool compress       = false;
    bool engine_coms    = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Log, file, level, append_file, compress, engine_coms)

}  // namespace fastchess::config
