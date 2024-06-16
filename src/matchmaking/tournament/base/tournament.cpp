#include <matchmaking/tournament/base/tournament.hpp>

#include <affinity/affinity_manager.hpp>
#include <book/opening_book.hpp>
#include <engine/uci_engine.hpp>
#include <epd/epd_builder.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/output/output.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>
#include <pgn/pgn_builder.hpp>
#include <types/tournament_options.hpp>
#include <util/cache.hpp>
#include <util/file_writer.hpp>
#include <util/logger/logger.hpp>
#include <util/threadpool.hpp>

namespace fast_chess {

BaseTournament::BaseTournament(const options::Tournament &config,
                               const std::vector<EngineConfiguration> &engine_configs)
    : book_(config) {
    tournament_options_ = config;
    engine_configs_     = engine_configs;
    output_             = OutputFactory::create(config);
    cores_              = std::make_unique<affinity::AffinityManager>(config.affinity, getMaxAffinity(engine_configs));

    if (!config.pgn.file.empty()) file_writer_pgn = std::make_unique<util::FileWriter>(config.pgn.file);
    if (!config.epd.file.empty()) file_writer_epd = std::make_unique<util::FileWriter>(config.epd.file);

    pool_.resize(config.concurrency);
}

void BaseTournament::start() {
    Logger::log<Logger::Level::TRACE>("Starting tournament...");

    create();
}

void BaseTournament::saveJson() {
    nlohmann::ordered_json jsonfile = tournament_options_;
    jsonfile["engines"]             = engine_configs_;
    jsonfile["stats"]               = getResults();

    Logger::log<Logger::Level::TRACE>("Saving results...");

    std::ofstream file(tournament_options_.config_name.empty() ? "config.json" : tournament_options_.config_name);
    file << std::setw(4) << jsonfile << std::endl;

    Logger::log<Logger::Level::INFO>("Saved results.");
}

void BaseTournament::stop() {
    Logger::log<Logger::Level::TRACE>("Stopped!");
    atomic::stop = true;
    Logger::log<Logger::Level::TRACE>("Stopping threads...");
    pool_.kill();
}

void BaseTournament::playGame(const std::pair<EngineConfiguration, EngineConfiguration> &configs, start_callback start,
                              finished_callback finish, const pgn::Opening &opening, std::size_t game_id) {
    if (atomic::stop) return;

    const auto core = util::ScopeGuard(cores_->consume());

    auto engine_one = util::ScopeGuard(engine_cache_.getEntry(configs.first.name, configs.first));
    auto engine_two = util::ScopeGuard(engine_cache_.getEntry(configs.second.name, configs.second));

    Logger::log<Logger::Level::TRACE>("Playing game ", game_id + 1, " between ", configs.first.name, " and ",
                                      configs.second.name);

    start();

    auto match = Match(tournament_options_, opening);

    try {
        match.start(engine_one.get().get(), engine_two.get().get(), core.get().cpus);
    } catch (const std::exception &e) {
        atomic::stop = true;
        Logger::log<Logger::Level::ERR>("Exception RoundRobin:::playGame " + std::string(e.what()));

        return;
    }

    if (atomic::stop) return;

    const auto match_data = match.get();

    // If the game was interrupted(didn't completely finish)
    if (match_data.termination != MatchTermination::INTERRUPT) {
        if (!tournament_options_.pgn.file.empty())
            file_writer_pgn->write(pgn::PgnBuilder(match_data, tournament_options_, game_id + 1).get());
        if (!tournament_options_.epd.file.empty())
            file_writer_epd->write(epd::EpdBuilder(match_data, tournament_options_).get());
    }

    finish({match_data}, match_data.reason);
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
