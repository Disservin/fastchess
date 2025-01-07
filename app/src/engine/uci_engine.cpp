#include <engine/uci_engine.hpp>

#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <core/config/config.hpp>
#include <core/filesystem/file_system.hpp>
#include <core/helper.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::engine {

// A counting semaphore to limit the number of threads
class CountingSemaphore {
   public:
    CountingSemaphore(int max_) : max__(max_), current_(0) {}

    void acquire() {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this] { return current_ < max__; });
        ++current_;
    }

    void release() {
        std::lock_guard<std::mutex> lock(mtx_);
        --current_;
        cond_.notify_one();
    }

   private:
    std::mutex mtx_;
    std::condition_variable cond_;
    int max__;
    int current_;
};

// RAII class to acquire and release the semaphore
class AcquireSemaphore {
   public:
    explicit AcquireSemaphore(CountingSemaphore &semaphore) : semaphore_(semaphore) { semaphore_.acquire(); }

    ~AcquireSemaphore() { semaphore_.release(); }

   private:
    CountingSemaphore &semaphore_;
};

namespace {
CountingSemaphore semaphore(16);

}  // namespace

UciEngine::UciEngine(const EngineConfiguration &config, bool realtime_logging) : realtime_logging_(realtime_logging) {
    loadConfig(config);
    output_.reserve(100);

    process_.setRealtimeLogging(realtime_logging_);
}

process::Status UciEngine::isready(std::chrono::milliseconds threshold) {
    const auto is_alive = process_.alive();
    if (is_alive != process::Status::OK) return is_alive;

    Logger::trace<true>("Pinging engine {}", config_.name);

    writeEngine("isready");

    setupReadEngine();

    std::vector<process::Line> output;
    const auto res = process_.readOutput(output, "readyok", threshold);

    // print output in case we are using delayed logging
    if (!realtime_logging_) {
        for (const auto &line : output) {
            Logger::readFromEngine(line.line, line.time, config_.name, line.std == process::Standard::ERR);
        }
    }

    if (res != process::Status::OK) {
        Logger::trace<true>("Engine {} didn't respond to isready.", config_.name);
        Logger::warn<true>("Warning; Engine {} is not responsive.", config_.name);

        return res;
    }

    Logger::trace<true>("Engine {} is {}", config_.name, "responsive.");

    return res;
}

bool UciEngine::position(const std::vector<std::string> &moves, const std::string &fen) {
    auto position = fmt::format("position {}", fen == "startpos" ? "startpos" : ("fen " + fen));

    if (!moves.empty()) {
        position += " moves";
        for (const auto &move : moves) {
            position += " " + move;
        }
    }

    return writeEngine(position);
}

bool UciEngine::go(const TimeControl &our_tc, const TimeControl &enemy_tc, chess::Color stm) {
    std::stringstream input;
    input << "go";

    if (config_.limit.nodes) {
        input << " nodes " << config_.limit.nodes;
    }

    if (config_.limit.plies) {
        input << " depth " << config_.limit.plies;
    }

    // We cannot use st and tc together
    if (our_tc.isFixedTime()) {
        input << " movetime " << our_tc.getFixedTime();
        return writeEngine(input.str());
    }

    const auto &white = stm == chess::Color::WHITE ? our_tc : enemy_tc;
    const auto &black = stm == chess::Color::WHITE ? enemy_tc : our_tc;

    if (our_tc.isTimed() || our_tc.isIncrement()) {
        if (white.isTimed() || white.isIncrement()) {
            input << " wtime " << white.getTimeLeft();
        }

        if (black.isTimed() || black.isIncrement()) {
            input << " btime " << black.getTimeLeft();
        }
    }

    if (our_tc.isIncrement()) {
        if (white.isIncrement()) {
            input << " winc " << white.getIncrement();
        }

        if (black.isIncrement()) {
            input << " binc " << black.getIncrement();
        }
    }

    if (our_tc.isMoves()) {
        input << " movestogo " << our_tc.getMovesLeft();
    }

    return writeEngine(input.str());
}

bool UciEngine::ucinewgame() {
    Logger::trace<true>("Sending ucinewgame to engine {}", config_.name);
    auto res = writeEngine("ucinewgame");

    if (!res) {
        Logger::trace<true>("Failed to send ucinewgame to engine {}", config_.name);
        return false;
    }

    return isready(ucinewgame_time_) == process::Status::OK;
}

std::optional<std::string> UciEngine::idName() {
    if (!uci()) {
        Logger::warn<true>("Warning; Engine {} didn't respond to uci.", config_.name);
        return std::nullopt;
    }

    if (!uciok()) {
        Logger::warn<true>("Warning; Engine {} didn't respond to uci.", config_.name);
        return std::nullopt;
    }

    // get id name
    for (const auto &line : output_) {
        if (line.line.find("id name") != std::string::npos) {
            // everything after id name
            return line.line.substr(line.line.find("id name") + 8);
        }
    }

    return std::nullopt;
}

std::optional<std::string> UciEngine::idAuthor() {
    if (!uci()) {
        Logger::warn<true>("Warning; Engine {} didn't respond to uci.", config_.name);
        return std::nullopt;
    }

    if (!uciok()) {
        Logger::warn<true>("Warning; Engine {} didn't respond to uci.", config_.name);
        return std::nullopt;
    }

    // get id author
    for (const auto &line : output_) {
        if (line.line.find("id author") != std::string::npos) {
            // everything after id author
            return line.line.substr(line.line.find("id author") + 10);
        }
    }

    return std::nullopt;
}

bool UciEngine::uci() {
    Logger::trace<true>("Sending uci to engine {}", config_.name);
    const auto res = writeEngine("uci");

    if (!res) {
        Logger::trace<true>("Failed to send uci to engine {}", config_.name);
        return false;
    }

    return res;
}

bool UciEngine::uciok(std::chrono::milliseconds threshold) {
    Logger::trace<true>("Waiting for uciok from engine {}", config_.name);

    const auto res = readEngine("uciok", threshold) == process::Status::OK;

    for (const auto &line : output_) {
        if (!realtime_logging_) {
            Logger::readFromEngine(line.line, line.time, config_.name, line.std == process::Standard::ERR);
        }

        auto option = UCIOptionFactory::parseUCIOptionLine(line.line);

        if (option != nullptr) {
            uci_options_.addOption(std::move(option));
        }
    }

    if (!res) Logger::trace<true>("Engine {} did not respond to uciok in time.", config_.name);

    return res;
}

void UciEngine::loadConfig(const EngineConfiguration &config) { config_ = config; }

void UciEngine::quit() {
    if (!initialized_) return;
    Logger::trace<true>("Sending quit to engine {}", config_.name);
    writeEngine("quit");
}

void UciEngine::sendSetoption(const std::string &name, const std::string &value) {
    auto option = uci_options_.getOption(name);

    if (!option.has_value()) {
        Logger::info<true>("Warning; {} doesn't have option {}", config_.name, name);
        return;
    }

    if (!option.value()->isValid(value)) {
        Logger::info<true>("Warning; Invalid value for option {}; {}", name, value);
        return;
    }

    Logger::trace<true>("Sending setoption to engine {} {} {}", config_.name, name, value);

    if (option.value()->getType() == UCIOption::Type::Button) {
        if (value != "true") {
            return;
        }

        if (!writeEngine(fmt::format("setoption name {}", name))) {
            Logger::trace<true>("Failed to send setoption to engine {} {}", config_.name, name);
            return;
        }

        option.value()->setValue(value);
    }

    if (!writeEngine(fmt::format("setoption name {} value {}", name, value))) {
        Logger::trace<true>("Failed to send setoption to engine {} {} {}", config_.name, name, value);
        return;
    }

    option.value()->setValue(value);
}

bool UciEngine::start() {
    if (initialized_) return true;

    AcquireSemaphore semaphore_acquire(semaphore);

    const auto path = config_.cmd;
    Logger::trace<true>("Starting engine {} at {}", config_.name, path);

    // Creates the engine process and sets the pipes
    if (process_.init(config_.dir, path, config_.args, config_.name) != process::Status::OK) {
        Logger::warn<true>("Warning; Cannot start engine {};", config_.name);
        Logger::warn<true>("Cannot execute command: {}", path);

        return false;
    }

    // Wait for the engine to start
    if (!uci()) {
        Logger::warn<true>("Couldnt write uci to engine.", config_.name);

        return false;
    }

    if (!uciok(startup_time_)) {
        Logger::warn<true>("Engine {} didn't respond to uci with uciok after startup.", config_.name);

        return false;
    }

    initialized_ = true;

    return true;
}

bool UciEngine::refreshUci() {
    Logger::trace<true>("Refreshing engine {}", config_.name);

    if (!ucinewgame()) {
        Logger::trace<true>("Engine {} failed to refresh.", config_.name);
        return false;
    }

    for (const auto &option : config_.options) {
        sendSetoption(option.first, option.second);
    }

    if (config_.variant == VariantType::FRC) {
        sendSetoption("UCI_Chess960", "true");
    }

    if (!ucinewgame()) {
        Logger::trace<true>("Engine {} didn't respond to ucinewgame.", config_.name);
        return false;
    }

    return true;
}

process::Status UciEngine::readEngine(std::string_view last_word, std::chrono::milliseconds threshold) {
    setupReadEngine();
    return process_.readOutput(output_, last_word, threshold);
}

void UciEngine::writeLog() const {
    for (const auto &line : output_) {
        Logger::readFromEngine(line.line, line.time, config_.name, line.std == process::Standard::ERR);
    }
}

std::string UciEngine::lastInfoLine() const {
    // iterate backwards over the output and save the info line
    for (auto it = output_.rbegin(); it != output_.rend(); ++it) {
        // skip lowerbound and upperbound
        if (it->line.find("lowerbound") != std::string::npos || it->line.find("upperbound") != std::string::npos) {
            continue;
        }

        // check that the line contains "info", "score", "cp" | "mate" and "multipv 1" if it contains multipv
        if (it->line.find("info") != std::string::npos && it->line.find(" score ") != std::string::npos &&
            (it->line.find(" multipv ") == std::string::npos || it->line.find(" multipv 1") != std::string::npos)) {
            return it->line;
        }
    }

    return {};
}

bool UciEngine::writeEngine(const std::string &input) {
    Logger::writeToEngine(input, util::time::datetime_precise(), config_.name);
    return process_.writeInput(input + "\n") == process::Status::OK;
}

std::optional<std::string> UciEngine::bestmove() const {
    if (output_.empty()) {
        Logger::warn<true>("Warning; No output from {}", config_.name);
        return std::nullopt;
    }

    const auto bm = str_utils::findElement<std::string>(str_utils::splitString(output_.back().line, ' '), "bestmove");

    if (!bm.has_value()) {
        Logger::warn<true>("Warning; No bestmove found in the last line from {}", config_.name);

        return std::nullopt;
    }

    return bm.value();
}

std::vector<std::string> UciEngine::lastInfo() const {
    const auto last_info = lastInfoLine();

    if (last_info.empty()) {
        Logger::warn<true>("Warning; Last info string with score not found from {}", config_.name);
        return {};
    }

    return str_utils::splitString(last_info, ' ');
}

ScoreType UciEngine::lastScoreType() const {
    auto score = str_utils::findElement<std::string>(lastInfo(), "score").value_or("ERR");

    if (score == "ERR") return ScoreType::ERR;
    if (score == "cp") return ScoreType::CP;
    if (score == "mate") return ScoreType::MATE;

    return ScoreType::ERR;
}

int UciEngine::lastScore() const {
    const auto score = lastScoreType();

    if (score == ScoreType::ERR) {
        return 0;
    }

    return str_utils::findElement<int>(lastInfo(), lastScoreType() == ScoreType::CP ? "cp" : "mate").value_or(0);
}

bool UciEngine::outputIncludesBestmove() const {
    for (const auto &line : output_) {
        if (line.line.find("bestmove") != std::string::npos) return true;
    }

    return false;
}

}  // namespace fastchess::engine
