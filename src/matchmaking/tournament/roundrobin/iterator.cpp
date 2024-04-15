#include <matchmaking/tournament/roundrobin/iterator.hpp>

#include <optional>
#include <tuple>
#include <vector>

#include <matchmaking/book/opening_book.hpp>
#include <matchmaking/output/output.hpp>
#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>
#include <util/iterator.hpp>

namespace fast_chess {

std::optional<std::tuple<std::pair<EngineConfiguration, EngineConfiguration>, Opening, std::size_t>>
RoundRobinIterator::next() {
    if (current_round_ >= tournament_.rounds || current_game_ >= tournament_.games) {
        // No more matches
        return std::nullopt;
    }

    const auto opening = book_.fetch();
    const auto first   = engine_configs_[current_index_i_];
    const auto second  = engine_configs_[current_index_j_];

    std::pair<EngineConfiguration, EngineConfiguration> configs =
        std::pair{engine_configs_[current_index_i_], engine_configs_[current_index_j_]};

    const std::size_t game_id = current_round_ * tournament_.games + (current_game_ + 1);

    const std::pair<EngineConfiguration, EngineConfiguration> copy = configs;

    // swap players for the next game
    std::swap(configs.first, configs.second);

    current_game_++;

    if (current_game_ >= tournament_.games) {
        // Move to the next round and reset game counter
        current_game_ = 0;
        current_index_j_++;

        if (current_index_j_ >= engine_configs_.size()) {
            // Move to the next player for the outer loop
            current_index_i_++;
            current_index_j_ = current_index_i_ + 1;

            if (current_index_j_ >= engine_configs_.size()) {
                // Move to the next round
                current_index_i_ = 0;
                current_index_j_ = 1;
                current_round_++;
            }
        }
    }

    return std::make_tuple(copy, opening, game_id);
}

bool RoundRobinIterator::hasNext() const {
    return current_round_ < tournament_.rounds && current_game_ < tournament_.games;
}

void RoundRobinIterator::reset() {
    current_round_   = 0;
    current_index_i_ = 0;
    current_index_j_ = 1;
    current_game_    = 0;
}
}  // namespace fast_chess