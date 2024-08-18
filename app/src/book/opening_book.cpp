#include <book/opening_book.hpp>

#include <fstream>
#include <optional>
#include <string>

#include <config/config.hpp>
#include <util/logger/logger.hpp>
#include <util/safe_getline.hpp>

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

    std::visit([](auto&& arg) { arg.clear(); }, book_);

    if (type == FormatType::PGN) {
        book_ = pgn::PgnReader(file, plies_).getOpenings();

        if (std::get<pgn_book>(book_).empty()) throw std::runtime_error("No openings found in PGN file: " + file);
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;
        while (util::safeGetline(openingFile, line))
            if (!line.empty()) std::get<epd_book>(book_).emplace_back(line);

        openingFile.close();

        if (std::get<epd_book>(book_).empty()) throw std::runtime_error("No openings found in EPD file: " + file);
    }

    if (type == FormatType::NONE) return;

    if (order_ == OrderType::RANDOM) {
        Logger::info("Indexing opening suite...");
        shuffle();
    }

    if (offset_ > 0) {
        Logger::trace("Offsetting the opening book by {} openings...", offset_);
        rotate(offset_);
    }

    truncate(rounds_);
    shrink();
}

[[nodiscard]] std::optional<std::size_t> OpeningBook::fetchId() noexcept {
    const auto idx = opening_index_++;
    const auto book_size =
        std::holds_alternative<epd_book>(book_) ? std::get<epd_book>(book_).size() : std::get<pgn_book>(book_).size();

    if (book_size == 0) return std::nullopt;
    return idx % book_size;
}

}  // namespace fastchess::book
