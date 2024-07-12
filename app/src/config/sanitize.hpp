#pragma once

#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fast_chess::config {

void sanitize(config::Tournament&);

void sanitize(std::vector<EngineConfiguration>&);

}  // namespace fast_chess::config
