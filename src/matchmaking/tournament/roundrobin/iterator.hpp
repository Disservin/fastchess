#pragma once

#include <util/iterator.hpp>

#include <optional>
#include <tuple>
#include <vector>

#include <types/tournament_options.hpp>

namespace fast_chess {

class Opening;
class OpeningBook;
class IOutput;
struct EngineConfiguration;

class RoundRobinIterator
    : public Iterator<std::pair<EngineConfiguration, EngineConfiguration>, Opening, std::size_t> {
   public:
    RoundRobinIterator(const cmd::TournamentOptions& to,
                       const std::vector<EngineConfiguration>& engine_configs, OpeningBook& book,
                       const IOutput& output)
        : tournament_(to),
          engine_configs_(engine_configs),
          book_(book),
          output_(output),
          current_round_(0),
          current_index_i_(0),
          current_index_j_(1),
          current_game_(0) {}

    std::optional<
        std::tuple<std::pair<EngineConfiguration, EngineConfiguration>, Opening, std::size_t>>
    next();

    bool hasNext() const;

    void reset();

   private:
    const cmd::TournamentOptions& tournament_;
    const std::vector<EngineConfiguration>& engine_configs_;
    OpeningBook& book_;
    const IOutput& output_;

    std::size_t current_round_;
    std::size_t current_index_i_;
    std::size_t current_index_j_;
    std::size_t current_game_;
    std::size_t total_;
};
// class RoundRobinIterator
//     : public Iterator<
//           std::optional<std::tuple<fast_chess::pair_config, fast_chess::Opening, std::size_t>>> {
//    public:
//     RoundRobinIterator(const cmd::TournamentOptions& to,
//                        const std::vector<EngineConfiguration>& engine_configs, OpeningBook& book,
//                        const IOutput& output)
//         : tournament_(to), engine_configs_(engine_configs), book_(book), output_(output) {}

//     std::optional<std::tuple<fast_chess::pair_config, fast_chess::Opening, std::size_t>> next()
//         override {
//         total_ = (engine_configs_.size() * (engine_configs_.size() - 1) / 2) * tournament_.rounds
//         *
//                  tournament_.games;

//         const auto create_match = [this](std::size_t i, std::size_t j, std::size_t round_id) {
//             // both players get the same opening
//             const auto opening = book_.fetch();
//             const auto first   = engine_configs_[i];
//             const auto second  = engine_configs_[j];

//             auto configs = std::pair{engine_configs_[i], engine_configs_[j]};

//             for (int g = 0; g < tournament_.games; g++) {
//                 const std::size_t game_id = round_id * tournament_.games + (g + 1);

//                 const auto copy = configs;

//                 // swap players
//                 std::swap(configs.first, configs.second);

//                 return std::make_tuple(copy, opening, round_id);
//             }
//         };

//         for (std::size_t i = 0; i < engine_configs_.size(); i++) {
//             for (std::size_t j = i + 1; j < engine_configs_.size(); j++) {
//                 for (int k = 0; k < tournament_.rounds; k++) {
//                     create_match(i, j, k);
//                 }
//             }
//         }
//     }

//     bool hasNext() const override {}

//     void reset() override {}

//    private:
//     const cmd::TournamentOptions& tournament_;
//     const std::vector<EngineConfiguration>& engine_configs_;
//     OpeningBook& book_;
//     const IOutput& output_;

//     std::size_t total_;
// };

}  // namespace fast_chess