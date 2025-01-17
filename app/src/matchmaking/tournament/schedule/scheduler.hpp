#pragma once

#include <cstdint>
#include <optional>

namespace fastchess {

class Scheduler {
   public:
    struct Pairing {
        std::size_t round_id;
        std::size_t pairing_id;
        std::size_t game_id;
        std::optional<std::size_t> opening_id;
        std::size_t player1;
        std::size_t player2;
    };

    virtual std::optional<Pairing> next() = 0;

    virtual std::size_t total() const = 0;
};

}  // namespace fastchess
