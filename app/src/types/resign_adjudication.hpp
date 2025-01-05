#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct ResignAdjudication {
    int move_count = 1;
    int score      = 0;
    bool twosided  = false;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(ResignAdjudication, move_count, score, twosided, enabled)

}  // namespace fastchess::config