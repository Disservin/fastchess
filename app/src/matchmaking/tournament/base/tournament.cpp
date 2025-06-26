#include <matchmaking/tournament/base/tournament.hpp>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <affinity/affinity_manager.hpp>
#include <core/filesystem/file_writer.hpp>
#include <core/globals/globals.hpp>
#include <core/logger/logger.hpp>
#include <core/memory/cache.hpp>
#include <core/threading/threadpool.hpp>
#include <engine/uci_engine.hpp>
#include <game/book/opening_book.hpp>
#include <game/epd/epd_builder.hpp>
#include <game/pgn/pgn_builder.hpp>
#include <matchmaking/match/match.hpp>
#include <matchmaking/output/output.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/scoreboard.hpp>
#include <matchmaking/tournament/schedule/scheduler.hpp>
#include <types/tournament.hpp>

namespace fastchess {

BaseTournament::BaseTournament(const stats_map &results) {
    const auto &config = *config::TournamentConfig;
    const auto total   = setResults(results);

    initial_matchcount_ = total;
    match_count_        = total;

    output_ = OutputFactory::create(config.output, config.report_penta);
    cores_  = std::make_unique<affinity::AffinityManager>(config.affinity, config.affinity_cpus, getMaxAffinity(*config::EngineConfigs));
    book_   = std::make_unique<book::OpeningBook>(config, initial_matchcount_);

    if (!config.pgn.file.empty())
        file_writer_pgn_ = std::make_unique<util::FileWriter>(config.pgn.file, config.pgn.crc);
    if (!config.epd.file.empty()) file_writer_epd_ = std::make_unique<util::FileWriter>(config.epd.file);

    pool_.resize(config.concurrency);
}

BaseTournament::~BaseTournament() {
    LOG_TRACE("~BaseTournament()");

    LOG_TRACE("Instructing engines to stop...");

    writeToOpenPipes();

    pool_.kill();

    if (config::TournamentConfig->output == OutputType::FASTCHESS) {
        if (tracker_.begin() != tracker_.end()) {
            Logger::print<Logger::Level::INFO>("");
        }

        for (const auto &[name, tracked] : tracker_) {
            Logger::print<Logger::Level::INFO>("Player: {}", name);
            Logger::print<Logger::Level::INFO>("  Timeouts: {}", tracked.timeouts);
            Logger::print<Logger::Level::INFO>("  Crashed: {}", tracked.disconnects);
        }

        if (tracker_.begin() != tracker_.end()) {
            Logger::print<Logger::Level::INFO>("");
        }
    }

    saveJson();
}

void BaseTournament::start() {
    LOG_TRACE("Starting tournament...");

    create();
}

void BaseTournament::create() {
    LOG_TRACE("Creating matches...");

    final_matchcount_ = generator_->total();

    for (int i = 0; i < pool_.getNumThreads(); i++) {
        startNext();
    }
}

void BaseTournament::saveJson() {
    LOG_TRACE("Saving results...");

    const auto &config = *config::TournamentConfig;

    nlohmann::ordered_json jsonfile = config;
    jsonfile["engines"]             = *config::EngineConfigs;
    jsonfile["stats"]               = getResults();

    std::ofstream file(config.config_name);
    file << std::setw(4) << jsonfile << std::endl;

    LOG_TRACE("Saved results to.");
}

void BaseTournament::playGame(const GamePair<EngineConfiguration, EngineConfiguration> &engine_configs,
                              const start_fn &start, const finish_fn &finish, const book::Opening &opening,
                              std::size_t round_id, std::size_t game_id) {

    const auto &config = *config::TournamentConfig;
    const auto rl      = config.log.realtime;
    const auto core    = util::ScopeGuard(cores_->consume());

    const auto white_name = engine_configs.white.name;
    const auto black_name = engine_configs.black.name;

    auto white_engine = engine_cache_.getEntry(white_name, engine_configs.white, rl);
    auto black_engine = engine_cache_.getEntry(black_name, engine_configs.black, rl);

    auto &w_engine_ref = *(white_engine->get().get());
    auto &b_engine_ref = *(black_engine->get().get());

    util::ScopeGuard lock1(w_engine_ref.getConfig().restart ? nullptr : &(*white_engine));
    util::ScopeGuard lock2(b_engine_ref.getConfig().restart ? nullptr : &(*black_engine));

    if (atomic::stop.load()) return;

    LOG_TRACE_THREAD("Game {} between {} and {} starting", game_id, white_name, black_name);

    start();

    auto match = Match(opening);
    match.start(w_engine_ref, b_engine_ref, core.get().cpus);

    LOG_TRACE_THREAD("Game {} between {} and {} finished", game_id, white_name, black_name);

    if (match.isStallOrDisconnect()) {
        LOG_WARN_THREAD("Game {} between {} and {} stalled / disconnected", game_id, white_name, black_name);

        if (!config.recover) {
            if (!atomic::stop.exchange(true)) {
                Logger::print<Logger::Level::WARN>(
                    "Game {} stalled / disconnected and no recover option set for engine, stopping tournament.",
                    game_id);

                atomic::abnormal_termination = true;
            }
            return;
        }

        // restart the engine when recover is enabled

        if (w_engine_ref.isready() != engine::process::Status::OK) restartEngine(white_engine->get());
        if (b_engine_ref.isready() != engine::process::Status::OK) restartEngine(black_engine->get());
    }

    const auto match_data = match.get();

    // If the game was interrupted(didn't completely finish)
    if (match_data.termination != MatchTermination::INTERRUPT && !atomic::stop) {
        if (!config.pgn.file.empty()) {
            const auto pgn_str = pgn::PgnBuilder(config.pgn, match_data, round_id + 1).get();
            file_writer_pgn_->write(pgn_str);
        }

        if (!config.epd.file.empty()) {
            file_writer_epd_->write(epd::EpdBuilder(config.variant, match_data).get());
        }

        const auto result = pgn::PgnBuilder::getResultFromMatch(match_data.players.white, match_data.players.black);
        LOG_TRACE_THREAD("Game {} finished with result {}", game_id, result);

        finish({match_data}, match_data.reason, {w_engine_ref, b_engine_ref});

        startNext();
    }

    // remove engines if restart is enabled, frees up memory
    if (w_engine_ref.getConfig().restart) engine_cache_.deleteFromCache(white_engine);
    if (b_engine_ref.getConfig().restart) engine_cache_.deleteFromCache(black_engine);

    const auto &loser = match_data.players.white.result == chess::GameResult::LOSE ? white_name : black_name;

    switch (match_data.termination) {
        case MatchTermination::TIMEOUT:
            tracker_.report_timeout(loser);
            break;
        case MatchTermination::DISCONNECT:
        case MatchTermination::STALL:
            tracker_.report_disconnect(loser);
            break;
        default:
            break;
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

void BaseTournament::restartEngine(std::unique_ptr<engine::UciEngine> &engine) {
    LOG_TRACE_THREAD("Restarting engine {}", engine->getConfig().name);
    auto config = engine->getConfig();
    auto rl     = engine->isRealtimeLogging();
    engine      = std::make_unique<engine::UciEngine>(config, rl);
}

std::size_t BaseTournament::setResults(const stats_map &results) {
    LOG_TRACE("Setting results...");

    scoreboard_.setResults(results);

    std::size_t total = 0;

    for (const auto &pair1 : scoreboard_.getResults()) {
        const auto &stats = pair1.second;

        total += stats.wins + stats.losses + stats.draws;
    }

    return total;
}

}  // namespace fastchess
