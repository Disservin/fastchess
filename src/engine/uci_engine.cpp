#include <engine/uci_engine.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include <util/file_system.hpp>
#include <util/helper.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess::engine {

bool UciEngine::isResponsive(std::chrono::milliseconds threshold) {
    try {
        if (!alive()) return false;

        Logger::log<Logger::Level::TRACE, true>("Pinging engine", config_.name);

        writeEngine("isready");

        std::vector<process::Line> output;
        const auto res = readProcess(output, "readyok", threshold);

        for (const auto &line : output) {
            Logger::readFromEngine(line.line, line.time, config_.name, line.std == process::Standard::ERR);
        }

        if (res != process::Status::OK) {
            Logger::log<Logger::Level::TRACE, true>("Engine", config_.name, "didn't respond to isready.");
            Logger::log<Logger::Level::WARN, true>("Warning; Engine", config_.name, "is not responsive.");
        }

        Logger::log<Logger::Level::TRACE, true>("Engine", config_.name,
                                                res == process::Status::OK ? "is responsive." : "is not responsive.");

        return res == process::Status::OK;

    } catch (const std::exception &e) {
        Logger::log<Logger::Level::TRACE, true>("Raised Exception in isResponsive", e.what());

        return false;
    }
}

bool UciEngine::ucinewgame() {
    Logger::log<Logger::Level::TRACE, true>("Sending ucinewgame to engine", config_.name);

    try {
        writeEngine("ucinewgame");
        const auto res = isResponsive(initialize_time);
        return res;
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::TRACE, true>("Raised Exception in ucinewgame", e.what());
        return false;
    }
}

void UciEngine::uci() {
    Logger::log<Logger::Level::TRACE, true>("Sending uci to engine", config_.name);
    writeEngine("uci");
}

bool UciEngine::uciok() {
    Logger::log<Logger::Level::TRACE, true>("Waiting for uciok from engine", config_.name);

    const auto res = readEngine("uciok") == process::Status::OK;

    Logger::log<Logger::Level::TRACE, true>("Engine", config_.name,
                                            res ? "responded to uciok." : "didn't respond to uciok.");

    return res;
}

void UciEngine::loadConfig(const EngineConfiguration &config) { config_ = config; }

void UciEngine::quit() {
    Logger::log<Logger::Level::TRACE, true>("Sending quit to engine", config_.name);
    try {
        writeEngine("quit");
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::TRACE, true>("Raised Exception when quitting engine", e.what());
    }
}

void UciEngine::sendSetoption(const std::string &name, const std::string &value) {
    Logger::log<Logger::Level::TRACE, true>("Sending setoption to engine", config_.name, name, value);
    writeEngine("setoption name " + name + " value " + value);
}

void UciEngine::start() {
    if (initialized_) return;

    std::string path = (config_.dir == "." ? "" : config_.dir) + config_.cmd;

#ifndef NO_STD_FILESYSTEM
    // convert path to a filesystem path
    auto p = std::filesystem::path(config_.dir) / std::filesystem::path(config_.cmd);
    path   = p.string();
#endif

    Logger::log<Logger::Level::TRACE, true>("Starting engine", config_.name, "at", path);

    init(path, config_.args, config_.name);
    uci();

    if (!uciok()) {
        throw std::runtime_error(config_.name + " failed to start.");
    }

    initialized_ = true;
}

void UciEngine::refreshUci() {
    Logger::log<Logger::Level::TRACE, true>("Refreshing engine", config_.name);
    start();

    if (!ucinewgame() && !isResponsive(ping_time_)) {
        // restart the engine
        Logger::log<Logger::Level::TRACE, true>("Engine", config_.name, "failed to refresh. Restarting engine.");
        restart();
        uci();

        if (!uciok()) {
            Logger::log<Logger::Level::TRACE, true>("Engine", config_.name, "failed to start.");
            throw std::runtime_error(config_.name + " failed to start.");
        }

        if (!ucinewgame() && !isResponsive(ping_time_)) {
            Logger::log<Logger::Level::TRACE, true>("Engine", config_.name,
                                                    "responded to uci but not to ucinewgame/isready.");
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
        Logger::log<Logger::Level::TRACE, true>("Engine", config_.name, "didn't respond to ucinewgame.");
        throw std::runtime_error(config_.name + " failed to start.");
    }
}

process::Status UciEngine::readEngine(std::string_view last_word, std::chrono::milliseconds threshold) {
    return readProcess(output_, last_word, threshold);
}

void UciEngine::writeLog() const {
    for (const auto &line : output_) {
        Logger::readFromEngine(line.line, line.time, config_.name, line.std == process::Standard::ERR);
    }
}

std::string UciEngine::lastInfoLine() const {
    // iterate backwards over the output and save the first line
    // that contains "info", "score" and "multipv 1" if it contains multipv
    for (auto it = output_.rbegin(); it != output_.rend(); ++it) {
        if (it->line.find("info") != std::string::npos && it->line.find(" score ") != std::string::npos &&
            (it->line.find(" multipv ") == std::string::npos || it->line.find(" multipv 1") != std::string::npos)) {
            return it->line;
        }
    }

    return {};
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input + "\n");
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::TRACE, true>("Raised Exception in writeProcess", e.what());

        throw e;
    }
}

std::optional<std::string> UciEngine::bestmove() const {
    if (output_.empty()) {
        Logger::log<Logger::Level::WARN, true>("Warning; No output from engine.");
        return std::nullopt;
    }

    const auto bm = str_utils::findElement<std::string>(str_utils::splitString(output_.back().line, ' '), "bestmove");

    if (!bm.has_value()) {
        Logger::log<Logger::Level::WARN, true>("Warning; Could not extract bestmove.");
        return std::nullopt;
    }

    return bm.value();
}

std::vector<std::string> UciEngine::lastInfo() const {
    const auto last_info = lastInfoLine();
    if (last_info.empty()) {
        Logger::log<Logger::Level::WARN, true>("Warning; Could not extract last uci info line.");
        return {};
    }

    return str_utils::splitString(last_info, ' ');
}

ScoreType UciEngine::lastScoreType() const {
    auto score = str_utils::findElement<std::string>(lastInfo(), "score").value_or("ERR");

    return score == "ERR" ? ScoreType::ERR : score == "cp" ? ScoreType::CP : ScoreType::MATE;
}

int UciEngine::lastScore() const {
    const auto score = lastScoreType();

    if (score == ScoreType::ERR) {
        Logger::log<Logger::Level::WARN, true>("Warning; Could not extract last uci score.");
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

}  // namespace fast_chess::engine
