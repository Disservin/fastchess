#include <book/opening_book.hpp>

#include <fstream>
#include <string>

#include <util/safe_getline.hpp>

namespace fast_chess::book {

OpeningBook::OpeningBook(const options::Tournament& tournament) {
    start_  = tournament.opening.start;
    games_  = tournament.games;
    rounds_ = tournament.rounds;
    order_  = tournament.opening.order;
    plies_  = tournament.opening.plies;
    offset_ = start_ - 1 + matchcount_ / games_;
    setup(tournament.opening.file, tournament.opening.format);
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    std::visit([](auto&& arg) { arg.clear(); }, book_);

    if (type == FormatType::PGN) {
        book_ = pgn::PgnReader(file, plies_).getOpenings();

        if (std::get<pgn_book>(book_).empty()) {
            throw std::runtime_error("No openings found in PGN file: " + file);
        }
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;

        while (util::safeGetline(openingFile, line)) {
            if (!line.empty()) std::get<epd_book>(book_).emplace_back(line);
            book_.shrink_to_fit()
        }

        openingFile.close();

        if (std::get<epd_book>(book_).empty()) {
            throw std::runtime_error("No openings found in EPD file: " + file);
        }
    }

    if (type != FormatType::NONE) {
        shuffle();
        rotate();
        truncate();
    }
}

[[nodiscard]] std::optional<std::size_t> OpeningBook::fetchId() noexcept {
    static uint64_t opening_index = 0;

    const auto idx       = opening_index++;
    const auto book_size = std::holds_alternative<epd_book>(book_)
                               ? std::get<epd_book>(book_).size()
                               : std::get<pgn_book>(book_).size();

    if (book_size == 0) {
        return std::nullopt;
    }

    return idx % book_size;
}

}  // namespace fast_chess::book
