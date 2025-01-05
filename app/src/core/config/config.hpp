#pragma once

#include <core/lazy.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fastchess::config {

inline util::Lazy<Tournament> TournamentConfig;
inline util::Lazy<std::vector<EngineConfiguration>> EngineConfigs;

}  // namespace fastchess::config
