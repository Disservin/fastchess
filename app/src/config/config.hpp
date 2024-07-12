#pragma once

#include <types/engine_config.hpp>
#include <types/tournament.hpp>
#include <util/lazy.hpp>

namespace fast_chess::config {

inline util::Lazy<Tournament> TournamentConfig;
inline util::Lazy<std::vector<EngineConfiguration>> EngineConfigs;

}  // namespace fast_chess::config
