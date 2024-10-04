#pragma once

#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fastchess::cli {

void sanitize(config::Tournament&);

void sanitize(std::vector<EngineConfiguration>&);

}  // namespace fastchess::cli
