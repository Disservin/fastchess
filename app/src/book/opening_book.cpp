#include <book/opening_book.hpp>

#include <fstream>
#include <optional>
#include <string>

#include <book/epd_reader.hpp>
#include <book/pgn_reader.hpp>
#include <config/config.hpp>
#include <util/logger/logger.hpp>

namespace fastchess::book {

void Openings::shuffle() {
    const auto shuffle = [](auto& vec) {
        for (std::size_t i = 0; i + 2 <= vec.size(); i++) {
            auto rand     = util::random::mersenne_rand();
            std::size_t j = i + (rand % (vec.size() - i));
            std::swap(vec[i], vec[j]);
        }
    };

    std::visit(shuffle, openings);
}

void Openings::rotate(std::size_t offset) {
    const auto rotate = [offset](auto& vec) {
        std::rotate(vec.begin(), vec.begin() + (offset % vec.size()), vec.end());
    };

    std::visit(rotate, openings);
}

void Openings::truncate(std::size_t rounds) {
    const auto truncate = [rounds](auto& vec) {
        if (vec.size() > rounds) {
            vec.resize(rounds);
        }
    };

    std::visit(truncate, openings);
}

void Openings::shrink() {
    const auto shrink = [](auto& vec) {
        using BookType = std::decay_t<decltype(vec)>;
        std::vector<typename BookType::value_type> tmp(vec.begin(), vec.end());
        vec.swap(tmp);
        vec.shrink_to_fit();
    };

    std::visit(shrink, openings);
}

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

    std::visit([](auto&& arg) { arg.clear(); }, openings_.openings);

    if (type == FormatType::PGN) {
        openings_ = PgnReader(file, plies_).getOpenings();
    } else if (type == FormatType::EPD) {
        openings_ = EpdReader(file).getOpenings();
    }

    if (type == FormatType::NONE) return;

    if (order_ == OrderType::RANDOM) {
        Logger::info("Indexing opening suite...");
        openings_.shuffle();
    }

    if (offset_ > 0) {
        Logger::trace("Offsetting the opening book by {} openings...", offset_);
        openings_.rotate(offset_);
    }

    openings_.truncate(rounds_);
    openings_.shrink();
}

Opening OpeningBook::operator[](std::optional<std::size_t> idx) const noexcept {
    if (!idx.has_value()) return {chess::constants::STARTPOS, {}, chess::Color::WHITE};

    if (openings_.isEpd()) {
        assert(idx.value() < openings_.epd().size());

        const auto fen = openings_.epd()[*idx];
        return {std::string(fen), {}, chess::Board(fen).sideToMove()};
    }

    assert(idx.value() < openings_.pgn().size());

    const auto [fen, moves, stm] = openings_.pgn()[*idx];

    return {fen, moves, stm};
}

[[nodiscard]] std::optional<std::size_t> OpeningBook::fetchId() noexcept {
    const auto idx       = opening_index_++;
    const auto book_size = openings_.isEpd() ? openings_.epd().size() : openings_.pgn().size();

    if (book_size == 0) return std::nullopt;
    return idx % book_size;
}

}  // namespace fastchess::book
