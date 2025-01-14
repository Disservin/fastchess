#pragma once

#include <memory>

#include <core/lazy.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fastchess::config {

inline std::unique_ptr<Tournament> TournamentConfig;
inline std::unique_ptr<std::vector<EngineConfiguration>> EngineConfigs;

}  // namespace fastchess::config
