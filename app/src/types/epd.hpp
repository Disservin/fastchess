#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace mercury::config {

struct Epd {
    std::string file;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Epd, file)

}  // namespace mercury::config