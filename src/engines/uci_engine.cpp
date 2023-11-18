#include <engines/uci_engine.hpp>

#include <stdexcept>

#include <helper.hpp>
#include <util/logger.hpp>

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
}

void UciEngine::refreshUci() {
    uci();

    if (!uciok() && !isResponsive(std::chrono::milliseconds(60000))) {
        // restart the engine
        restart();
        uci();

        if (!uciok() && !isResponsive(std::chrono::milliseconds(60000))) {
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
    try {
        return read(output_, last_word, threshold);
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::ERR>("Raised Exception in read\nWarning; Engine", config_.name,
                                        "disconnects");
        throw e;
    }
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        write(input + "\n");
    } catch (const std::exception &e) {
        Logger::log<Logger::Level::ERR>("Raised Exception in write\nWarning; Engine", config_.name,
                                        "disconnects");

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
    if (output_.size() < 2) {
        Logger::log<Logger::Level::WARN>("Warning; Could not extract last uci info line.");
        return {};
    }

    return str_utils::splitString(output_[output_.size() - 2], ' ');
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
}  // namespace fast_chess
