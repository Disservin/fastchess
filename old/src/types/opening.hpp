#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct Opening {
    std::string file;
    FormatType format = FormatType::NONE;
    OrderType order   = OrderType::SEQUENTIAL;
    int plies         = -1;
    int start         = 1;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Opening, file, format, order, plies, start)

}  // namespace fastchess::config
