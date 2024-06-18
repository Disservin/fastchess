#include <book/opening_book.hpp>

#include <fstream>
#include <optional>
#include <string>

#include <util/safe_getline.hpp>

namespace fast_chess::book {

OpeningBook::OpeningBook(const options::Tournament& tournament, std::size_t initial_matchcount) {
    start_      = tournament.opening.start;
    games_      = tournament.games;
    order_      = tournament.opening.order;
    plies_      = tournament.opening.plies;
    matchcount_ = initial_matchcount;
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
        opening_file_.open(file);

        std::string line;
        std::streampos start_pos = 0;
        while (util::safeGetline(opening_file_, line)) {
            if (line.empty()) continue;
            std::streampos end_pos = start_pos + std::streampos(line.size() + 1);
            std::get<epd_book>(book_).emplace_back(std::make_pair(start_pos, end_pos));
            std::cout << "start_pos: " << start_pos << " end_pos: " << end_pos << std::endl;
            start_pos = end_pos;
        }
    }

    if (order_ == OrderType::RANDOM && type != FormatType::NONE) shuffle();
}

[[nodiscard]] std::optional<std::size_t> OpeningBook::fetchId() noexcept {
    // - 1 because start starts at 1 in the opening options
    const auto idx = start_ - 1 + opening_index_++ + matchcount_ / games_;
    const auto book_size =
        std::holds_alternative<epd_book>(book_) ? std::get<epd_book>(book_).size() : std::get<pgn_book>(book_).size();

    if (book_size == 0) {
        return std::nullopt;
    }

    return idx % book_size;
}

[[nodiscard]] std::string OpeningBook::readLineFromEpdBook(std::streampos start, std::streampos end) {
    opening_file_.clear();
    opening_file_.seekg(start);

    std::streamsize size = end - start;
    std::string line(size, ' ');

    opening_file_.read(&line[0], size);

    line.erase(line.find_last_not_of(" \n\r\t") + 1);  // Trim the line from the right

    return line;
}

}  // namespace fast_chess::book
