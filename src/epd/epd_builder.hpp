#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>

#include <matchmaking/match/match.hpp>

#include <types/tournament_options.hpp>

namespace fast_chess {

class EpdBuilder {
   public:
    EpdBuilder(const MatchData &match, const options::Tournament &tournament_options,
               std::size_t round_id) {
        chess::Board board = chess::Board();
        board.set960(tournament_options.variant == VariantType::FRC);
        board.setFen(match.fen);

        for (const auto &move : match.moves) {
            const auto illegal = !move.legal;

            if (illegal) {
                break;
            }

            board.makeMove(chess::uci::uciToMove(board, move.move));
        }

        epd << board.getEpd() << "\n";
    }

    /// @brief Get the newly created epd
    /// @return
    [[nodiscard]] std::string get() const noexcept { return epd.str(); }

   private:
    std::stringstream epd;
};

}  // namespace fast_chess