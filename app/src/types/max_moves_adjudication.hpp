#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace fastchess::config {

struct MaxMovesAdjudication {
    int move_count = 1;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(MaxMovesAdjudication, move_count, enabled)

}  // namespace fastchess::config