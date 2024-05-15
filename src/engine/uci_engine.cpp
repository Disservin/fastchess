#include <engine/uci_engine.hpp>

#include <stdexcept>
#include <string>
#include <vector>

#include <util/helper.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess::engine {

bool UciEngine::isResponsive(std::chrono::milliseconds threshold) {
    if (!alive()) return false;

    writeEngine("isready");
    const auto res = readEngine("readyok", threshold);

    if (res != process::Status::OK) {
        Logger::log<Logger::Level::WARN>("Warning; Engine", config_.name, "is not responsive.");
    }

    return res == process::Status::OK;
}

bool UciEngine::ucinewgame() {
    writeEngine("ucinewgame");
    return isResponsive(initialize_time);
}

void UciEngine::uci() { writeEngine("uci"); }

bool UciEngine::uciok() { return readEngine("uciok") == process::Status::OK; }

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
    if (!ucinewgame() && !isResponsive(ping_time_)) {
        // restart the engine
        restart();
        uci();

        if (!uciok()) {
            throw std::runtime_error(config_.name + " failed to start.");
        }

        if (!ucinewgame() && !isResponsive(ping_time_)) {
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

process::Status UciEngine::readEngine(std::string_view last_word,
                                      std::chrono::milliseconds threshold) {
    return readProcess(output_, last_word, threshold);
}

void UciEngine::writeLog() const {
    for (const auto &line : output_) {
        Logger::readFromEngine(line.line, config_.name, line.std == process::Standard::ERR);
    }
}

std::string UciEngine::lastInfoLine() const {
    // iterate backwards over the output and save the first line
    // that contains "info", "score" and "multipv 1" if it contains multipv
    for (auto it = output_.rbegin(); it != output_.rend(); ++it) {
        if (it->line.find("info") != std::string::npos &&
            it->line.find(" score ") != std::string::npos &&
            (it->line.find(" multipv ") == std::string::npos ||
             it->line.find(" multipv 1") != std::string::npos)) {
            return it->line;
        }
    }

    return {};
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input + "\n");
    } catch (const std::exception &e) {
        std::cout << input << std::endl;
        Logger::log<Logger::Level::ERR>("Raised Exception in writeProcess\nWarning; Engine",
                                        config_.name, "disconnects");

        throw e;
    }
}

std::string UciEngine::bestmove() const {
    if (output_.empty()) {
        Logger::log<Logger::Level::WARN>("Warning; No output from engine.");
        return "aaaa";
    }

    const auto bm = str_utils::findElement<std::string>(
        str_utils::splitString(output_.back().line, ' '), "bestmove");

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

int UciEngine::lastScore() const {
    const auto score = lastScoreType();

    if (score == ScoreType::ERR) {
        Logger::log<Logger::Level::WARN>("Warning; Could not extract last uci score.");
        return 0;
    }

    return str_utils::findElement<int>(lastInfo(), lastScoreType() == ScoreType::CP ? "cp" : "mate")
        .value_or(0);
}

bool UciEngine::outputIncludesBestmove() const {
    for (const auto &line : output_) {
        if (line.line.find("bestmove") != std::string::npos) return true;
    }

    return false;
}

}  // namespace fast_chess::engine
