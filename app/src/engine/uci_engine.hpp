#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <chess.hpp>

#ifdef _WIN64
#    include <engine/process/process_win.hpp>
#else
#    include <engine/process/process_posix.hpp>
#endif

#include <engine/option/option_factory.hpp>
#include <engine/option/options.hpp>
#include <types/engine_config.hpp>

namespace fastchess::engine {

enum class ScoreType { CP, MATE, ERR };

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
    [[nodiscard]] bool start();

    // Restarts the engine, if necessary and reapplies the options.
    bool refreshUci();

    [[nodiscard]] std::optional<std::string> idName();
    [[nodiscard]] std::optional<std::string> idAuthor();

    // Returns false in case of failure.
    [[nodiscard]] bool uci();
    // Returns false in case of failure.
    [[nodiscard]] bool uciok(std::chrono::milliseconds threshold = ping_time_);
    // Returns false in case of failure.
    [[nodiscard]] bool ucinewgame();

    // Sends "isready" to the engine
    [[nodiscard]] process::Status isready(std::chrono::milliseconds threshold = ping_time_);

    // Sends "position" to the engine and waits for a response.
    [[nodiscard]] bool position(const std::vector<std::string> &moves, const std::string &fen);

    [[nodiscard]] bool go(const TimeControl &our_tc, const TimeControl &enemy_tc, chess::Color stm);

    void quit();

    // Writes the input to the engine. Appends a newline to the input.
    // Returns false in case of failure.
    bool writeEngine(const std::string &input);

    // Waits for the engine to output the last_word or until the threshold_ms is reached.
    // May throw if the read fails.
    process::Status readEngine(std::string_view last_word, std::chrono::milliseconds threshold = ping_time_);

    process::Status readEngineLowLat(std::string_view last_word, std::chrono::milliseconds threshold = ping_time_) {
        return readEngine(last_word, threshold);
    }

    void setupReadEngine() { process_->setupRead(); }

    // Logs are not written in realtime to avoid slowing down the engine.
    // This function writes the logs to the logger.
    void writeLog() const;

    void setCpus(const std::vector<int> &cpus) { process_->setAffinity(cpus); }

    // Get the bestmove from the last output.
    [[nodiscard]] std::optional<std::string> bestmove() const;

    [[nodiscard]] std::string lastInfoLine() const;

    // Get the last info from the last output.
    [[nodiscard]] std::vector<std::string> lastInfo() const;

    // Get the last score type from the last output. cp or mate.
    [[nodiscard]] ScoreType lastScoreType() const;

    // Get the last score from the last output. Becareful, mate scores are not converted. So
    // the score might 1, while it's actually mate 1. Always check lastScoreType() first.
    [[nodiscard]] int lastScore() const;

    // returns false if the output doesnt include a bestmove
    [[nodiscard]] bool outputIncludesBestmove() const;

    [[nodiscard]] const std::vector<process::Line> &output() const noexcept { return output_; }
    [[nodiscard]] const EngineConfiguration &getConfig() const noexcept { return config_; }

    // @TODO: expose this to the user?
    static constexpr std::chrono::milliseconds startup_time_    = std::chrono::seconds(10);
    static constexpr std::chrono::milliseconds ucinewgame_time_ = std::chrono::seconds(60);
    static constexpr std::chrono::milliseconds ping_time_       = std::chrono::seconds(60);

    [[nodiscard]] const UCIOptions &uciOptions() const noexcept { return uci_options_; }
    UCIOptions &uciOptions() noexcept { return uci_options_; }

   private:
    void loadConfig(const EngineConfiguration &config);
    void sendSetoption(const std::string &name, const std::string &value);

    std::unique_ptr<process::IProcess> process_ = std::make_unique<process::Process>();

    UCIOptions uci_options_;
    std::vector<process::Line> output_;
    EngineConfiguration config_;

    // init on first use
    bool initialized_ = false;

    const bool realtime_logging_;
};
}  // namespace fastchess::engine
