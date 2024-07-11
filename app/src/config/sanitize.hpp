#pragma once

#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess::config {

void sanitize(options::Tournament&);

void sanitize(std::vector<EngineConfiguration>&);

}  // namespace fast_chess::config
