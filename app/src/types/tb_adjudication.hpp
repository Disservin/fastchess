#pragma once

#include <json.hpp>

#include <string>

namespace fastchess::config {

struct TbAdjudication {
    std::string syzygy_dirs;
    int max_pieces           = 0;
    bool ignore_50_move_rule = false;

    bool enabled = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TbAdjudication, syzygy_dirs, max_pieces, ignore_50_move_rule, enabled)

}  // namespace fastchess::config
