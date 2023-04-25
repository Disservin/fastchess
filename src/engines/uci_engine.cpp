#include "engines/uci_engine.hpp"

#include <sstream>
#include <stdexcept>

#include "engines/engine_config.hpp"
#include "logger.hpp"
#include "uci_engine.hpp"

namespace fast_chess {

EngineConfiguration UciEngine::getConfig() const { return config_; }

bool UciEngine::isResponsive(int64_t threshold) {
    if (!isAlive()) return false;

    bool timeout = false;
    writeEngine("isready");
    readEngine("readyok", timeout, threshold);
    return !timeout;
}

void UciEngine::sendUciNewGame() {
    writeEngine("ucinewgame");
    isResponsive(60000);
}

void UciEngine::sendUci() { writeEngine("uci"); }

std::vector<std::string> UciEngine::readUci() {
    bool timeout = false;
    return readEngine("uciok", timeout);
}

std::string UciEngine::buildGoInput(Chess::Color stm, const TimeControl &tc,
                                    const TimeControl &tc_2) const {
    std::stringstream input;
    input << "go";

    if (config_.nodes != 0) input << " nodes " << config_.nodes;

    if (config_.plies != 0) input << " depth " << config_.plies;

    if (tc.fixed_time != 0) {
        input << " movetime " << tc.fixed_time;
    }
    // We cannot use st and tc together
    else {
        auto white = stm == Chess::Color::WHITE ? tc : tc_2;
        auto black = stm == Chess::Color::WHITE ? tc_2 : tc;

        if (tc.time != 0) {
            input << " wtime " << white.time << " btime " << black.time;
        }

        if (tc.increment != 0) {
            input << " winc " << white.increment << " binc " << black.increment;
        }

        if (tc.moves != 0) input << " movestogo " << tc.moves;
    }
    return input.str();
}

void UciEngine::loadConfig(const EngineConfiguration &config) { this->config_ = config; }

void UciEngine::sendQuit() { writeEngine("quit"); }

void UciEngine::sendSetoption(const std::string &name, const std::string &value) {
    writeEngine("setoption name " + name + " value " + value);
}

void UciEngine::restartEngine() {
    killProcess();
    initProcess(config_.cmd);
}

void UciEngine::startEngine(const std::string &cmd) {
    initProcess(cmd);

    sendUci();
    readUci();

    if (!isResponsive(60000)) {
        throw std::runtime_error("Warning: Something went wrong when pinging the engine.");
    }

    for (const auto &option : config_.options) {
        sendSetoption(option.first, option.second);
    }
}

std::vector<std::string> UciEngine::readEngine(std::string_view last_word, bool &timeout,
                                               int64_t timeoutThreshold) {
    try {
        return readProcess(last_word, timeout, timeoutThreshold);
    } catch (const std::exception &e) {
        Logger::coutInfo("Raised Exception in readProcess\nWarning: Engine", config_.name,
                         "disconnects #");
        throw e;
    }
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input);
    } catch (const std::exception &e) {
        Logger::coutInfo("Raised Exception in writeProcess\nWarning: Engine", config_.name,
                         "disconnects #");

        throw e;
    }
}

}  // namespace fast_chess
