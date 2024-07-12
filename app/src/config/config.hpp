#pragma once

#include <types/engine_config.hpp>
#include <types/tournament.hpp>
#include <util/lazy.hpp>

namespace fast_chess::config {

inline Lazy<TournamentType> Tournament;
inline Lazy<std::vector<EngineConfiguration>> EngineConfigs;

}  // namespace fast_chess::config
