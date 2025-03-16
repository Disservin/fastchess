#pragma once

#include <json.hpp>

#include <string>

namespace fastchess::config {

struct TbAdjudication {
    std::string syzygy_dirs;

    bool enabled = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TbAdjudication, syzygy_dirs, enabled)

}  // namespace fastchess::config
