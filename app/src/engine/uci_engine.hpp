#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <chess.hpp>
#include <expected.hpp>

#ifdef _WIN64
#    include <engine/process/process_win.hpp>
#else
#    include <engine/process/process_posix.hpp>
#endif

#include <core/config/config.hpp>
#include <engine/option/option_factory.hpp>
#include <engine/option/options.hpp>
#include <types/engine_config.hpp>

namespace fastchess::engine {

using ms = std::chrono::milliseconds;

enum class ScoreType { CP, MATE, ERR };

struct Score {
    ScoreType type;
    int64_t value;
};

class UciEngine {
   public:
    explicit UciEngine(const EngineConfiguration &config, bool realtime_logging);

    UciEngine(const UciEngine &)            = delete;
    UciEngine(UciEngine &&)                 = delete;
    UciEngine &operator=(const UciEngine &) = delete;
    UciEngine &operator=(UciEngine &&)      = delete;

    ~UciEngine() { quit(); }

    // Starts the engine, does nothing after the first call.
    // Returns false if the engine is not alive.
    [[nodiscard]] tl::expected<bool, std::string> start(const std::optional<std::vector<int>> &cpus);

    // Restarts the engine, if necessary and reapplies the options.
    bool refreshUci();

    [[nodiscard]] std::optional<std::string> idName();
    [[nodiscard]] std::optional<std::string> idAuthor();

    // Returns false in case of failure.
    [[nodiscard]] bool uci();
    // Returns false in case of failure.
    [[nodiscard]] bool uciok(std::optional<ms> threshold = std::nullopt);
    // Returns false in case of failure.
    [[nodiscard]] bool ucinewgame();

    // Sends "isready" to the engine
    [[nodiscard]] process::Status isready(std::optional<ms> threshold = std::nullopt);

    // Sends "position" to the engine and waits for a response.
    [[nodiscard]] bool position(const std::vector<std::string> &moves, const std::string &fen);

    [[nodiscard]] bool go(const TimeControl &our_tc, const TimeControl &enemy_tc, chess::Color stm);

    void quit();

    // Writes the input to the engine. Appends a newline to the input.
    // Returns false in case of failure.
    bool writeEngine(const std::string &input);

    // Waits for the engine to output the last_word or until the threshold_ms is reached.
    // May throw if the read fails.
    process::Status readEngine(std::string_view last_word, std::optional<ms> threshold = std::nullopt);

    process::Status readEngineLowLat(std::string_view last_word, std::optional<ms> threshold = std::nullopt) {
        return readEngine(last_word, threshold);
    }

    void setupReadEngine() { process_.setupRead(); }

    // Logs are not written in realtime to avoid slowing down the engine.
    // This function writes the logs to the logger.
    void writeLog() const;

    void setCpus(const std::vector<int> &cpus) {
        if (cpus.empty()) return;

// Apple does not support setting the affinity of a pid
#ifdef __APPLE__
        return;
#endif

        if (process_.setAffinity(cpus)) return;

        // turn cpus vector into a string for error logging
        std::string cpu_str;
        for (const auto &cpu : cpus) {
            if (!cpu_str.empty()) cpu_str += ", ";
            cpu_str += std::to_string(cpu);
        }

        Logger::print<Logger::Level::WARN>(
            "Warning; Failed to set CPU affinity for the engine process to {}. Please restart.", cpu_str);
    }

    // Get the bestmove from the last output.
    [[nodiscard]] std::optional<std::string> bestmove() const;

    [[nodiscard]] std::string lastInfoLine() const;

    // Get the last info from the last output.
    [[nodiscard]] std::vector<std::string> lastInfo() const;

    [[nodiscard]] ms lastTime() const;

    // Get the last score from the last output.
    [[nodiscard]] Score lastScore() const;

    // returns false if the output doesnt include a bestmove
    [[nodiscard]] bool outputIncludesBestmove() const;

    [[nodiscard]] const std::vector<process::Line> &output() const noexcept { return output_; }
    [[nodiscard]] const EngineConfiguration &getConfig() const noexcept { return config_; }

    // @TODO: expose this to the user?

    [[nodiscard]] const UCIOptions &uciOptions() const noexcept { return uci_options_; }
    UCIOptions &uciOptions() noexcept { return uci_options_; }

    [[nodiscard]] bool isRealtimeLogging() const noexcept { return realtime_logging_; }

    [[nodiscard]] const std::vector<process::Line> &lastOutput() const noexcept { return output_; }

    [[nodiscard]] static ms getPingTime() { return config::TournamentConfig->ping_time; }
    [[nodiscard]] static ms getUciNewGameTime() { return config::TournamentConfig->ucinewgame_time; }
    [[nodiscard]] static ms getStartupTime() { return config::TournamentConfig->startup_time; }

   private:
    void loadConfig(const EngineConfiguration &config);
    void sendSetoption(const std::string &name, const std::string &value);

    process::Process process_ = {};
    UCIOptions uci_options_   = {};
    EngineConfiguration config_;

    std::vector<process::Line> output_;

    // init on first use
    bool initialized_ = false;

    const bool realtime_logging_;
};
}  // namespace fastchess::engine
