#include "engines/uci_engine.hpp"

#include <sstream>
#include <stdexcept>

#include "engines/engine_config.hpp"
#include "helper.hpp"
#include "logger.hpp"
#include "uci_engine.hpp"

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
    return isResponsive(60000);
}

void UciEngine::sendUci() { writeEngine("uci"); }
bool UciEngine::readUci() {
    readEngine("uciok");
    return timeout();
}

std::string UciEngine::buildPositionInput(const std::vector<std::string> &moves,
                                          const std::string &fen) const {
    std::string position = fen == "startpos" ? "position startpos" : ("position fen " + fen);

    if (!moves.empty()) {
        position += " moves";
        for (const auto &move : moves) {
            position += " " + move;
        }
    }

    return position;
}

std::string UciEngine::buildGoInput(Chess::Color stm, const TimeControl &tc,
                                    const TimeControl &tc_2) const {
    std::stringstream input;
    input << "go";

    if (config_.limit.nodes != 0) input << " nodes " << config_.limit.nodes;

    if (config_.limit.plies != 0) input << " depth " << config_.limit.plies;

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

    if (!readUci() || !isResponsive(60000)) {
        throw std::runtime_error("Warning: Something went wrong when pinging the engine.");
    }

    for (const auto &option : config_.options) {
        sendSetoption(option.first, option.second);
    }
}

std::vector<std::string> UciEngine::readEngine(std::string_view last_word,
                                               int64_t timeoutThreshold) {
    try {
        output_.clear();
        output_ = readProcess(last_word, timeoutThreshold);
        return output_;

    } catch (const std::exception &e) {
        Logger::cout("Raised Exception in readProcess\nWarning: Engine", config_.name,
                     "disconnects #");
        throw e;
    }
}

void UciEngine::writeEngine(const std::string &input) {
    try {
        writeProcess(input + "\n");
    } catch (const std::exception &e) {
        Logger::cout("Raised Exception in writeProcess\nWarning: Engine", config_.name,
                     "disconnects #");

        throw e;
    }
}

std::string UciEngine::bestmove() const {
    return findElement<std::string>(splitString(output_.back(), ' '), "bestmove").value();
}

std::vector<std::string> UciEngine::lastInfo() const {
    return splitString(output_[output_.size() - 2], ' ');
}

std::string UciEngine::lastScoreType() const {
    return findElement<std::string>(lastInfo(), "score").value_or("cp");
}

int UciEngine::lastScore() const {
    return findElement<int>(lastInfo(), lastScoreType()).value_or(0);
}

std::vector<std::string> UciEngine::output() const { return std::vector<std::string>(); }

}  // namespace fast_chess
