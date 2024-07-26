#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace mercury::config {

struct ResignAdjudication {
    int move_count = 1;
    int score      = 0;
    bool twosided  = false;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(ResignAdjudication, move_count, score, twosided, enabled)

}  // namespace mercury::config