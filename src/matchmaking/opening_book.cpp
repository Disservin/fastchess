#include "opening_book.h"

#include <fstream>
#include <string>

namespace fast_chess {

OpeningBook::OpeningBook(const std::string& file, FormatType type, std::size_t start) {
    start_ = start;
    setup(file, type);
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    if (type == FormatType::PGN) {
        book_ = PgnReader(file).getPgns();

        if (std::get<pgn_book>(book_).empty()) {
            throw std::runtime_error("No openings found in PGN file: " + file);
        }
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;
        std::vector<std::string> epd;

        while (std::getline(openingFile, line)) {
            epd.emplace_back(line);
        }

        openingFile.close();

        book_ = epd;

        if (std::get<epd_book>(book_).empty()) {
            throw std::runtime_error("No openings found in EPD file: " + file);
        }
    }
}

Opening OpeningBook::fetch() noexcept {
    static uint64_t opening_index = 0;

    const auto idx = start_ + opening_index++;

    if (std::holds_alternative<epd_book>(book_)) {
        return {std::get<epd_book>(book_)[idx % std::get<epd_book>(book_).size()], {}};
    } else if (std::holds_alternative<pgn_book>(book_)) {
        return std::get<pgn_book>(book_)[idx % std::get<pgn_book>(book_).size()];
    }

    return {chess::constants::STARTPOS, {}};
}

}  // namespace fast_chess