#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace fast_chess::config {

struct Opening {
    std::string file;
    FormatType format = FormatType::NONE;
    OrderType order   = OrderType::RANDOM;
    int plies         = -1;
    int start         = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Opening, file, format, order, plies, start)

}  // namespace fast_chess::config