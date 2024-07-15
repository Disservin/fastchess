#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace fast_chess::config {

struct Opening {
    std::string file;
    FormatType format = FormatType::NONE;

#ifdef USE_CUTE
    OrderType order = OrderType::SEQUENTIAL;
#else
    OrderType order = OrderType::RANDOM;
#endif

    int plies = -1;
    int start = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Opening, file, format, order, plies, start)

}  // namespace fast_chess::config
