#include <matchmaking/tournament/base/tournament.hpp>

#include <affinity/affinity_manager.hpp>
#include <core/filesystem/file_writer.hpp>
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
#include <types/tournament.hpp>

namespace fastchess {

namespace atomic {
extern std::atomic_bool stop;
}  // namespace atomic

BaseTournament::BaseTournament(const stats_map &results) {
    const auto &config     = *config::TournamentConfig;
    const auto total       = setResults(results);
    const auto num_players = config::EngineConfigs->size();

    initial_matchcount_ = total;
    match_count_        = total;

    output_    = OutputFactory::create(config.output, config.report_penta);
    cores_     = std::make_unique<affinity::AffinityManager>(config.affinity, getMaxAffinity(*config::EngineConfigs));
    book_      = std::make_unique<book::OpeningBook>(config, initial_matchcount_);
    generator_ = std::make_unique<MatchGenerator>(book_.get(), num_players, config.rounds, config.games, total);

    if (!config.pgn.file.empty()) file_writer_pgn_ = std::make_unique<util::FileWriter>(config.pgn.file);
    if (!config.epd.file.empty()) file_writer_epd_ = std::make_unique<util::FileWriter>(config.epd.file);

    pool_.resize(config.concurrency);
}

BaseTournament::~BaseTournament() {
    Logger::trace("~BaseTournament()");
    saveJson();
    Logger::trace("Instructing engines to stop...");
    writeToOpenPipes();
}

void BaseTournament::start() {
    Logger::trace("Starting tournament...");

    create();
}

void BaseTournament::saveJson() {
    Logger::trace("Saving results...");

    const auto &config = *config::TournamentConfig;

    nlohmann::ordered_json jsonfile = config;
    jsonfile["engines"]             = *config::EngineConfigs;
    jsonfile["stats"]               = getResults();

    auto filename = config.config_name.empty() ? "config.json" : config.config_name;

    std::ofstream file(filename);
    file << std::setw(4) << jsonfile << std::endl;

    Logger::trace("Saved results.");
}

void BaseTournament::playGame(const GamePair<EngineConfiguration, EngineConfiguration> &engine_configs,
                              start_callback start, finished_callback finish, const book::Opening &opening,
                              std::size_t round_id, std::size_t game_id) {
    if (atomic::stop) return;

    const auto &config = *config::TournamentConfig;
    const auto rl      = config.log.realtime;
    const auto core    = util::ScopeGuard(cores_->consume());

    const auto white_name = engine_configs.white.name;
    const auto black_name = engine_configs.black.name;

    auto &white_engine = engine_cache_.getEntry(white_name, engine_configs.white, rl);
    auto &black_engine = engine_cache_.getEntry(black_name, engine_configs.black, rl);

    util::ScopeGuard lock1(white_engine);
    util::ScopeGuard lock2(black_engine);

    if (white_engine.get()->getConfig().restart) restartEngine(white_engine.get());
    if (black_engine.get()->getConfig().restart) restartEngine(black_engine.get());

    Logger::trace<true>("Game {} between {} and {} starting", game_id, white_name, black_name);

    start();

    auto match = Match(opening);
    match.start(*white_engine.get(), *black_engine.get(), core.get().cpus);

    Logger::trace<true>("Game {} between {} and {} finished", game_id, white_name, black_name);

    if (match.isStallOrDisconnect()) {
        Logger::trace<true>("Game {} between {} and {} stalled / disconnected", game_id, white_name, black_name);

        if (!config.recover) {
            atomic::stop = true;
            return;
        }

        // restart the engine when recover is enabled

        Logger::trace<true>("Restarting engine...");

        if (white_engine.get()->isready() != engine::process::Status::OK) restartEngine(white_engine.get());
        if (black_engine.get()->isready() != engine::process::Status::OK) restartEngine(black_engine.get());
    }

    const auto match_data = match.get();

    // If the game was interrupted(didn't completely finish)
    if (match_data.termination != MatchTermination::INTERRUPT && !atomic::stop) {
        if (!config.pgn.file.empty()) {
            file_writer_pgn_->write(pgn::PgnBuilder(config.pgn, match_data, round_id + 1).get());
        }

        if (!config.epd.file.empty()) {
            file_writer_epd_->write(epd::EpdBuilder(config.variant, match_data).get());
        }

        const auto result = pgn::PgnBuilder::getResultFromMatch(match_data.players.white, match_data.players.black);
        Logger::trace<true>("Game {} finished with result {}", game_id, result);

        finish({match_data}, match_data.reason, {*white_engine.get(), *black_engine.get()});

        startNext();
    }

    const auto &loser = match_data.players.white.result == chess::GameResult::LOSE ? white_name : black_name;

    switch (match_data.termination) {
        case MatchTermination::TIMEOUT:
            tracker_.get(loser).timeouts++;
            break;
        case MatchTermination::DISCONNECT:
        case MatchTermination::STALL:
            tracker_.get(loser).disconnects++;
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
    Logger::trace<true>("Restarting engine {}", engine->getConfig().name);
    engine = std::make_unique<engine::UciEngine>(engine->getConfig(), engine->isRealtimeLogging());
}

std::size_t BaseTournament::setResults(const stats_map &results) {
    Logger::trace("Setting results...");

    scoreboard_.setResults(results);

    std::size_t total = 0;

    for (const auto &pair1 : scoreboard_.getResults()) {
        const auto &stats = pair1.second;

        total += stats.wins + stats.losses + stats.draws;
    }

    return total;
}

}  // namespace fastchess
