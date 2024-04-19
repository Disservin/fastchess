#include <matchmaking/book/opening_book.hpp>

#include <fstream>
#include <string>

#include <util/safe_getline.hpp>

namespace fast_chess {

OpeningBook::OpeningBook(const options::Opening& opening) {
    start_ = opening.start;
    order_ = opening.order;
    setup(opening.file, opening.format);
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    if (type == FormatType::PGN) {
        book_ = PgnReader(file).getOpenings();

        if (std::get<pgn_book>(book_).empty()) {
            throw std::runtime_error("No openings found in PGN file: " + file);
        }
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;
        std::vector<std::string> epd;

        while (safeGetline(openingFile, line)) {
            epd.emplace_back(line);
        }

        openingFile.close();

        book_ = epd;

        if (std::get<epd_book>(book_).empty()) {
            throw std::runtime_error("No openings found in EPD file: " + file);
        }
    }
    if (order_ == OrderType::RANDOM){shuffle();}
}

Opening OpeningBook::fetch() noexcept {
    static uint64_t opening_index = 0;
    const auto idx                = start_ + opening_index++;
    const auto book_size          = std::holds_alternative<epd_book>(book_)
                                        ? std::get<epd_book>(book_).size()
                                        : std::get<pgn_book>(book_).size();

    if (book_size == 0) {
        return {chess::constants::STARTPOS, {}};
    }

    if (std::holds_alternative<epd_book>(book_)) {
        const auto fen = std::get<epd_book>(book_)[idx % std::get<epd_book>(book_).size()];
        return {fen, {}, chess::Board(fen).sideToMove()};
    } else if (std::holds_alternative<pgn_book>(book_)) {
        return std::get<pgn_book>(book_)[idx % std::get<pgn_book>(book_).size()];
    }

    return {chess::constants::STARTPOS, {}};
}

}  // namespace fast_chess
