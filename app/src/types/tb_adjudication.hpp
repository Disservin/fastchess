#pragma once

#include <json.hpp>

#include <string>

namespace fastchess::config {

struct TbAdjudication {
    enum ResultType { WIN_LOSS = 1, DRAW = 2, BOTH = WIN_LOSS | DRAW };

    std::string syzygy_dirs;
    int max_pieces           = 0;
    ResultType result_type   = ResultType::BOTH;
    bool ignore_50_move_rule = false;

    bool enabled = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TbAdjudication, syzygy_dirs, max_pieces, ignore_50_move_rule, enabled)

}  // namespace fastchess::config
