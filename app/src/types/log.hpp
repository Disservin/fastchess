#pragma once

#include <string>

#include <util/logger/logger.hpp>

namespace fastchess::config {

struct Log {
    std::string file;
    Logger::Level level = Logger::Level::WARN;
    bool compress       = false;
    bool realtime       = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Log, file, level, compress, realtime)

}  // namespace fastchess::config