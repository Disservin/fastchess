#include <engines/uci_engine.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include <util/helper.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess {

bool UciEngine::isResponsive(std::chrono::milliseconds threshold) {
    if (!alive()) return false;

    writeEngine("isready");
    const auto res = readEngine("readyok", threshold);
    return res == Process::Status::OK;
}

bool UciEngine::ucinewgame() {
    writeEngine("ucinewgame");
    return isResponsive(ping_time_);
}

void UciEngine::uci() { writeEngine("uci"); }

bool UciEngine::uciok() { return readEngine("uciok") == Process::Status::OK; }

void UciEngine::loadConfig(const EngineConfiguration &config) { config_ = config; }

void UciEngine::quit() { writeEngine("quit"); }

void UciEngine::sendSetoption(const std::string &name, const std::string &value) {
    writeEngine("setoption name " + name + " value " + value);
}

void UciEngine::start() {
    Logger::log<Logger::Level::TRACE>("Starting engine", config_.name);
    init((config_.dir == "." ? "" : config_.dir) + config_.cmd, config_.args, config_.name);
    uci();

    if (!uciok()) {
        throw std::runtime_error(config_.name + " failed to start.");
    }
}

void UciEngine::refreshUci() {
    if (!ucinewgame() && !isResponsive(std::chrono::milliseconds(60000))) {
        // restart the engine
        restart();
        uci();

        if (!uciok()) {
            throw std::runtime_error(config_.name + " failed to start.");
        }

        if (!ucinewgame() && !isResponsive(std::chrono::milliseconds(60000))) {
            throw std::runtime_error("Warning; Something went wrong when pinging the engine.");
        }
    }

    for (const auto &option : config_.options) {
        sendSetoption(option.first, option.second);
    }

    if (config_.variant == VariantType::FRC) {
        sendSetoption("UCI_Chess960", "true");
    }

    if (!ucinewgame()) {
        throw std::runtime_error(config_.name + " failed to start.");
    }
}

Process::Status UciEngine::readEngine(std::string_view last_word,
                                      std::chrono::milliseconds threshold) {
    last_info_.clear();

    try {
        return readProcess(output_, last_word, threshold);
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::ERR>("Raised Exception in readProcess\nWarning; Engine",
                                        config_.name, "disconnects");
        throw e;
    }
}

std::string UciEngine::lastInfoLine() const {
    // iterate backwards over the output and save the first line
    // that contains "info depth" and score
    for (auto it = output_.rbegin(); it != output_.rend(); ++it) {
        if (it->find("info depth") != std::string::npos &&
            it->find(" score ") != std::string::npos) {
            return *it;
        }
    }

    return {};
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input + "\n");
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::ERR>("Raised Exception in writeProcess\nWarning; Engine",
                                        config_.name, "disconnects");

        throw e;
    }
}

std::string UciEngine::bestmove() const {
    const auto bm = str_utils::findElement<std::string>(str_utils::splitString(output_.back(), ' '),
                                                        "bestmove");

    if (!bm.has_value()) {
        Logger::log<Logger::Level::WARN>("Warning; Could not extract bestmove.");
        return "aaaa";
    }

    return bm.value();
}

std::vector<std::string> UciEngine::lastInfo() const {
    const auto last_info = lastInfoLine();
    if (last_info.empty()) {
        Logger::log<Logger::Level::WARN>("Warning; Could not extract last uci info line.");
        return {};
    }

    return str_utils::splitString(last_info, ' ');
}

ScoreType UciEngine::lastScoreType() const {
    auto score = str_utils::findElement<std::string>(lastInfo(), "score").value_or("ERR");

    return score == "ERR" ? ScoreType::ERR : score == "cp" ? ScoreType::CP : ScoreType::MATE;
}

std::chrono::milliseconds UciEngine::lastTime() const {
    const auto time = str_utils::findElement<int>(lastInfo(), "time").value_or(0);

    return std::chrono::milliseconds(time);
}

int UciEngine::lastScore() const {
    const auto score = lastScoreType();

    if (score == ScoreType::ERR) {
        Logger::log<Logger::Level::WARN>("Warning; Could not extract last uci score.");
        return 0;
    }

    return str_utils::findElement<int>(lastInfo(), lastScoreType() == ScoreType::CP ? "cp" : "mate")
        .value_or(0);
}
}  // namespace fast_chess
