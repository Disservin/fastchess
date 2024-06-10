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
    setup(tournament.opening.file, tournament.opening.format);
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    if (type == FormatType::PGN) {
        pgn_ = pgn::PgnReader(file, plies_).getOpenings();
        
        if (order_ == OrderType::RANDOM) shuffle(type);
        
        if (pgn.size() > games_ * rounds_) {
            pgn_.erase(pgn_.begin() + games_ * rounds_, pgn_.end());
        }

        if (pgn_.empty()) {
            throw std::runtime_error("No openings found in PGN file: " + file);
        }
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;

        while (util::safeGetline(openingFile, line)) {
            if (!line.empty()) epd_.emplace_back(line);
        }

        openingFile.close();

        if (order_ == OrderType::RANDOM) shuffle(type);
        
        if (epd_.size() > games_ * rounds_) {
            epd_.erase(epd_.begin() + games_ * rounds_, epd_.end());
        }

        if (epd_.empty()) {
            throw std::runtime_error("No openings found in EPD file: " + file);
        }
    }
}

pgn::Opening OpeningBook::fetch() noexcept {
    static uint64_t opening_index = 0;

    // - 1 because start starts at 1 in the opening options
    const auto idx       = start_ - 1 + opening_index++ + matchcount_ / games_;
    const auto book_size = std::max(epd_.size(), pgn_.size());

    if (book_size == 0) {
        return {chess::constants::STARTPOS, {}};
    }

    if (!epd_.empty()) {
        const auto fen = epd_[idx % book_size];
        return {fen, {}, chess::Board(fen).sideToMove()};
    } else if (!pgn_.empty()) {
        return pgn_[idx % book_size];
    }

    return {chess::constants::STARTPOS, {}};
}

}  // namespace fast_chess::book
