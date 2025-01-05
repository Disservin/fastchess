#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct DrawAdjudication {
    uint32_t move_number = 0;
    int move_count       = 1;
    int score            = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(DrawAdjudication, move_number, move_count, score, enabled)

}  // namespace fastchess::config