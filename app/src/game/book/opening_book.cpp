#include <game/book/opening_book.hpp>

#include <optional>
#include <string>

#include <chess.hpp>

#include <core/config/config.hpp>
#include <core/logger/logger.hpp>
#include <game/book/epd_reader.hpp>
#include <game/book/pgn_reader.hpp>

namespace fastchess::book {

OpeningBook::OpeningBook(const config::Tournament& config, std::size_t initial_matchcount) {
    start_  = config.opening.start;
    games_  = config.games;
    rounds_ = config.rounds;
    order_  = config.opening.order;
    plies_  = config.opening.plies;

    // - 1 because start starts at 1 in the opening options
    offset_ = start_ - 1 + initial_matchcount / games_;
    setup(config.opening.file, config.opening.format);
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    if (type == FormatType::PGN) {
        openings_.emplace<PgnReader>(file, plies_);
    } else if (type == FormatType::EPD) {
        openings_.emplace<EpdReader>(file);
    }

    if (type == FormatType::NONE) return;

    if (order_ == OrderType::RANDOM) {
        Logger::print("Indexing opening suite...");
        std::visit([](auto& arg) { Openings::shuffle(arg.get()); }, openings_);
    }

    if (offset_ > 0) {
        LOG_INFO("Offsetting the opening book by {} openings...", offset_);
        std::visit([offset = offset_](auto& arg) { Openings::rotate(arg.get(), offset); }, openings_);
    }

    std::visit([rounds = rounds_](auto& arg) { Openings::truncate(arg.get(), rounds); }, openings_);
    std::visit([](auto& arg) { Openings::shrink(arg.get()); }, openings_);
}

Opening OpeningBook::operator[](std::optional<std::size_t> idx) const noexcept {
    if (!idx.has_value()) return {chess::constants::STARTPOS, {}};

    assert(idx.value() < std::visit([](const auto& arg) { return arg.get().size(); }, openings_));

    if (std::holds_alternative<EpdReader>(openings_)) {
        const auto& reader = std::get<EpdReader>(openings_);
        const auto fen     = reader.get()[idx.value()];

        return {std::string(fen.c_str()), {}};
    }

    const auto& reader = std::get<PgnReader>(openings_);
    return reader.get()[idx.value()];
}

[[nodiscard]] std::optional<std::size_t> OpeningBook::fetchId() noexcept {
    const auto idx       = opening_index_++;
    const auto book_size = std::visit([](const auto& arg) { return arg.get().size(); }, openings_);

    if (book_size == 0) return std::nullopt;
    return idx % book_size;
}

}  // namespace fastchess::book
