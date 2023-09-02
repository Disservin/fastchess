#include <engines/uci_engine.hpp>

#include <stdexcept>

#include <helper.hpp>
#include <util/logger.hpp>

namespace fast_chess {

EngineConfiguration UciEngine::getConfig() const { return config_; }

bool UciEngine::isResponsive(int64_t threshold) {
    if (!isAlive()) return false;

    writeEngine("isready");
    readEngine("readyok", threshold);
    return !timeout();
}

bool UciEngine::sendUciNewGame() {
    writeEngine("ucinewgame");
    return isResponsive(ping_time_);
}

void UciEngine::sendUci() { writeEngine("uci"); }

bool UciEngine::readUci() {
    readEngine("uciok");
    return !timeout();
}

void UciEngine::loadConfig(const EngineConfiguration &config) { config_ = config; }

void UciEngine::sendQuit() { writeEngine("quit"); }

void UciEngine::sendSetoption(const std::string &name, const std::string &value) {
    writeEngine("setoption name " + name + " value " + value);
}

void UciEngine::restartEngine() {
    killProcess();
    initProcess((config_.dir == "." ? "" : config_.dir) + config_.cmd, config_.name);
}

void UciEngine::startEngine() {
    initProcess((config_.dir == "." ? "" : config_.dir) + config_.cmd, config_.name);

    sendUci();

    if (!readUci() && !isResponsive(60000)) {
        throw std::runtime_error("Warning; Something went wrong when pinging the engine.");
    }

    for (const auto &option : config_.options) {
        sendSetoption(option.first, option.second);
    }

    if (config_.variant == VariantType::FRC) {
        sendSetoption("UCI_Chess960", "true");
    }

    if (!sendUciNewGame()) {
        throw std::runtime_error(config_.name + " failed to start.");
    }
}

void UciEngine::readEngine(std::string_view last_word, int64_t threshold_ms) {
    try {
        readProcess(output_, last_word, threshold_ms);
    } catch (const std::exception &e) {
        Logger::cout("Raised Exception in readProcess\nWarning; Engine", config_.name,
                     "disconnects");
        throw e;
    }
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input + "\n");
    } catch (const std::exception &e) {
        Logger::cout("Raised Exception in writeProcess\nWarning; Engine", config_.name,
                     "disconnects");

        throw e;
    }
}

std::string UciEngine::bestmove() const {
    const auto bm = str_utils::findElement<std::string>(str_utils::splitString(output_.back(), ' '),
                                                        "bestmove");

    if (!bm.has_value()) {
        Logger::cout("Warning; Could not extract bestmove.");
        return "aaaa";
    }

    return bm.value();
}

std::vector<std::string> UciEngine::lastInfo() const {
    if (output_.size() < 2) {
        // silence for now
        // Logger::cout("Warning; Could not extract last uci info line.");
        return {};
    }
    return str_utils::splitString(output_[output_.size() - 2], ' ');
}

std::string UciEngine::lastScoreType() const {
    return str_utils::findElement<std::string>(lastInfo(), "score").value_or("cp");
}

int UciEngine::lastScore() const {
    return str_utils::findElement<int>(lastInfo(), lastScoreType()).value_or(0);
}

std::vector<std::string> UciEngine::output() const { return output_; }

bool UciEngine::timedout() const { return timeout(); }
}  // namespace fast_chess
