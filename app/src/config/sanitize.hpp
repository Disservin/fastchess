#pragma once

#include <config/types.hpp>
#include <types/engine_config.hpp>

namespace fast_chess::config {

void sanitize(config::TournamentType&);

void sanitize(std::vector<EngineConfiguration>&);

}  // namespace fast_chess::config
