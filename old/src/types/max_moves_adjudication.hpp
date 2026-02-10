#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct MaxMovesAdjudication {
    int move_count = 1;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MaxMovesAdjudication, move_count, enabled)

}  // namespace fastchess::config