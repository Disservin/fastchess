#include <matchmaking/tournament/base/tournament.hpp>

#include <affinity/affinity_manager.hpp>
#include <book/opening_book.hpp>
#include <config/types.hpp>
#include <engine/uci_engine.hpp>
#include <epd/epd_builder.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/output/output.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>
#include <pgn/pgn_builder.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/logger/logger.hpp>
#include <util/threadpool.hpp>

namespace fast_chess {

BaseTournament::BaseTournament(const stats_map &results) {
    output_ = OutputFactory::create();
    cores_  = std::make_unique<affinity::AffinityManager>(config::Tournament.get().affinity,
                                                          getMaxAffinity(config::EngineConfigs.get()));

    if (!config::Tournament.get().pgn.file.empty())
        file_writer_pgn = std::make_unique<util::FileWriter>(config::Tournament.get().pgn.file);
    if (!config::Tournament.get().epd.file.empty())
        file_writer_epd = std::make_unique<util::FileWriter>(config::Tournament.get().epd.file);

    pool_.resize(config::Tournament.get().concurrency);

    setResults(results);

    book_ = std::make_unique<book::OpeningBook>(config::Tournament.get(), initial_matchcount_);
}

void BaseTournament::start() {
    Logger::trace("Starting tournament...");

    create();
}

void BaseTournament::saveJson() {
    nlohmann::ordered_json jsonfile = config::Tournament.get();
    jsonfile["engines"]             = config::EngineConfigs.get();
    jsonfile["stats"]               = getResults();

    Logger::trace("Saving results...");

    std::ofstream file(config::Tournament.get().config_name.empty() ? "config.json"
                                                                    : config::Tournament.get().config_name);
    file << std::setw(4) << jsonfile << std::endl;

    Logger::info("Saved results.");
}

void BaseTournament::stop() {
    Logger::trace("Stopped!");
    atomic::stop = true;
    Logger::trace("Stopping threads...");
    pool_.kill();
}

void BaseTournament::playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs, start_callback start,
                              finished_callback finish, const pgn::Opening &opening, std::size_t game_id) {
    if (atomic::stop) return;

    const auto core = util::ScopeGuard(cores_->consume());

    auto engine_one = util::ScopeGuard(
        engine_cache_.getEntry(configs.first.name, configs.first, config::Tournament.get().realtime_logging));
    auto engine_two = util::ScopeGuard(
        engine_cache_.getEntry(configs.second.name, configs.second, config::Tournament.get().realtime_logging));

    Logger::trace("Playing game {} between {} and {}", game_id + 1, configs.first.name, configs.second.name);

    start();

    auto match = Match(opening);
    match.start(engine_one.get().get(), engine_two.get().get(), core.get().cpus);

    if (match.isCrashOrDisconnect()) {
        // restart the engine when recover is enabled
        if (config::Tournament.get().recover) {
            Logger::trace("Restarting engine...");
            if (!engine_one.get().get().isready()) {
                Logger::trace("Restarting engine {}", configs.first.name);
                engine_one.get().get().refreshUci();
            }

            if (!engine_two.get().get().isready()) {
                Logger::trace("Restarting engine {}", configs.second.name);
                engine_two.get().get().refreshUci();
            }
        } else {
            atomic::stop = true;
        }
    }

    const auto match_data = match.get();

    // If the game was interrupted(didn't completely finish)
    if (match_data.termination != MatchTermination::INTERRUPT) {
        if (!config::Tournament.get().pgn.file.empty())
            file_writer_pgn->write(pgn::PgnBuilder(config::Tournament.get(), match_data, game_id + 1).get());
        if (!config::Tournament.get().epd.file.empty())
            file_writer_epd->write(epd::EpdBuilder(config::Tournament.get(), match_data).get());

        finish({match_data}, match_data.reason, {engine_one.get().get(), engine_two.get().get()});
    }
}

int BaseTournament::getMaxAffinity(const std::vector<EngineConfiguration> &configs) const noexcept {
    constexpr auto transform = [](const auto &val) { return std::stoi(val); };
    const auto first_threads = configs[0].getOption<int>("Threads", transform).value_or(1);

    for (const auto &config : configs) {
        const auto threads = config.getOption<int>("Threads", transform).value_or(1);

        // thread count in all configs has to be the same for affinity to work,
        // otherwise we set it to 0 and affinity is disabled
        // if (threads != first_threads) {
        //     return 0;
        // }

        // only enable for 1 thread currently
        if (threads != 1) {
            return 0;
        }
    }

    return first_threads;
}

}  // namespace fast_chess
