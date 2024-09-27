#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <config/config.hpp>
#include <types/enums.hpp>
#include <types/tournament.hpp>
#include <util/rand.hpp>

#include <chess.hpp>

namespace fastchess::book {

struct Opening {
    Opening() = default;
    Opening(const std::string& fen_epd, const std::vector<chess::Move>& moves, chess::Color stm = chess::Color::WHITE)
        : fen_epd(fen_epd), moves(moves), stm(stm) {}

    std::string fen_epd            = chess::constants::STARTPOS;
    std::vector<chess::Move> moves = {};
    chess::Color stm               = chess::Color::WHITE;
};

struct Openings {
    using EPD = std::vector<std::string>;
    using PGN = std::vector<Opening>;

    Openings() = default;
    explicit Openings(EPD epd) : openings(std::move(epd)) {}
    explicit Openings(PGN pgn) : openings(std::move(pgn)) {}

    [[nodiscard]] bool isEpd() const noexcept { return std::holds_alternative<EPD>(openings); }
    [[nodiscard]] bool isPgn() const noexcept { return std::holds_alternative<PGN>(openings); }

    [[nodiscard]] const auto& epd() const noexcept { return std::get<EPD>(openings); }
    [[nodiscard]] const auto& pgn() const noexcept { return std::get<PGN>(openings); }

    // Fisher-Yates / Knuth shuffle
    void shuffle();
    // Rotates the book vector based on the starting offset
    void rotate(std::size_t offset);

    // Gets rid of unused openings within the book vector to reduce memory usage
    void truncate(std::size_t rounds);

    // Shrink book vector
    void shrink();

    std::variant<EPD, PGN> openings;
};

class OpeningBook {
    using EPD = std::vector<std::string>;
    using PGN = std::vector<Opening>;

   public:
    OpeningBook() = default;
    explicit OpeningBook(const config::Tournament& config, std::size_t initial_matchcount = 0);

    [[nodiscard]] std::optional<std::size_t> fetchId() noexcept;

    [[nodiscard]] Opening operator[](std::optional<std::size_t> idx) const noexcept;

   private:
    void setup(const std::string& file, FormatType type);

    std::size_t start_         = 0;
    std::size_t opening_index_ = 0;
    std::size_t offset_        = 0;
    std::size_t rounds_;
    std::size_t games_;
    int plies_;
    OrderType order_;

    Openings openings_;
};

}  // namespace fastchess::book
