#pragma once

#include <string>

#include <core/logger/logger.hpp>

namespace fastchess::config {

struct Log {
    std::string file;
    Logger::Level level = Logger::Level::WARN;
    bool compress       = false;
    bool realtime       = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Log, file, level, compress, realtime)

}  // namespace fastchess::config