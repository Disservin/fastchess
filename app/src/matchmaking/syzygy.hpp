#pragma once

#include <string_view>

#include <chess.hpp>

namespace fastchess {

// Initialize the syzygy tablebases.
// syzygyDirs should be a ;-separated list of directories containing the tablebases.
// Returns the largest number of pieces in the tablebases, or 0 upon failure.
[[nodiscard]] int initSyzygy(std::string_view syzygyDirs);

// Release the syzygy tablebases.
void tearDownSyzygy();

// Check whether the board is in a state for which we can probe WDL tablebases.
// Note that even if this function returns true, a probe for this position might still fail if the
// tablebases are incomplete.
[[nodiscard]] bool canProbeSyzgyWdl(const chess::Board& board);

// Probe the WDL tablebases for the given board.
// If the probe is successful, one of GameResult::WIN, GameResult::DRAW, GameResult::LOSE is
// returned, representing the game result from the perspective of the side to move.
// Otherwise, GameResult::NONE is returned.
[[nodiscard]] chess::GameResult probeSyzygyWdl(const chess::Board& board);

}  // namespace fastchess
