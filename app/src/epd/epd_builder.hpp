#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <chess.hpp>

#include <config/config.hpp>
#include <matchmaking/match/match.hpp>
#include <types/tournament.hpp>

namespace fastchess::epd {

class EpdBuilder {
   public:
    EpdBuilder(const VariantType &variant, const MatchData &match) {
        chess::Board board = chess::Board();
        board.set960(variant == VariantType::FRC);
        board.setFen(match.fen);

        for (const auto &move : match.moves) {
            const auto illegal = !move.legal;

            if (illegal) break;

            board.makeMove<true>(chess::uci::uciToMove(board, move.move));
        }

        epd << board.getEpd() << "\n";
    }

    // Get the newly created epd
    [[nodiscard]] std::string get() const noexcept { return epd.str(); }

   private:
    std::stringstream epd;
};

}  // namespace fastchess::epd