#pragma once

#include <util/iterator.hpp>

#include <optional>
#include <tuple>
#include <vector>

#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess {

class Opening;
class OpeningBook;
class IOutput;
struct EngineConfiguration;

class RoundRobinIterator
    : public Iterator<std::pair<EngineConfiguration, EngineConfiguration>, Opening, std::size_t> {
   public:
    RoundRobinIterator(const options::Tournament& to,
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
    const options::Tournament& tournament_;
    const std::vector<EngineConfiguration>& engine_configs_;
    OpeningBook& book_;
    const IOutput& output_;

    std::size_t current_round_;
    std::size_t current_index_i_;
    std::size_t current_index_j_;
    std::size_t current_game_;
    std::size_t total_;
};

}  // namespace fast_chess