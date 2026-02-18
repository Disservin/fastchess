#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct Epd {
    std::string file;
    bool append_file = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Epd, file, append_file)

}  // namespace fastchess::config
