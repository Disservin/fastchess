#pragma once

#include <string>
#include <variant>
#include <vector>

#include <pgn/pgn_reader.hpp>
#include <types/enums.hpp>
#include <types/tournament_options.hpp>
#include <util/rand.hpp>

namespace fast_chess::book {

class OpeningBook {
   public:
    OpeningBook() = default;
    explicit OpeningBook(const options::Tournament& tournament);

    [[nodiscard]] pgn::Opening fetch() noexcept;

    void setInternalOffset(std::size_t offset) noexcept { matchcount_ = offset; }

   private:
    void setup(const std::string& file, FormatType type);

    std::size_t start_      = 0;
    std::size_t matchcount_ = 0;
    int games_;
    int rounds_;
    int plies_;
    OrderType order_;
    std::vector<std::string> epd_;
    std::vector<pgn::Opening> pgn_;
};

}  // namespace fast_chess::book
