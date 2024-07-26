#pragma once

#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace mercury::config {

void sanitize(config::Tournament&);

void sanitize(std::vector<EngineConfiguration>&);

}  // namespace mercury::config
