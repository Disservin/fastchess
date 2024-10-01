#pragma once

#include <string>

#include <engine/uci_engine.hpp>

namespace fastchess::engine {

/**
 * UCI compliance check.
 *
 * 1. The engine must respond to the "uci" command with "uciok".
 * 2. The engine must respond to the "isready" command with "readyok".
 * 3. The engine must handle the "ucinewgame" command, and be ready after it.
 * 4. The engine must handle the "position startpos" command and be ready after it.
 * 5. The engine must handle the "position fen" command and be ready after it.
 * 6. The engine must handle the "go wtime 100" command.
 * 7. The engine must handle the "go btime 100" command.
 * 8. The engine must handle the "go wtime 100 winc 100 btime 100 binc 100" command.
 * 9. The engine must handle the "go btime 100 binc 100 wtime 100 winc 100" command.
 * 10. The engine must respond to the go commands with bestmove, and print an info line before the bestmove.
 *
 *
 */
bool compliant(const std::string &path) {
    EngineConfiguration config;

    config.cmd = path;

    if (config.cmd.empty()) {
        std::cerr << "Path to engine is empty" << std::endl;
        return false;
    }

    UciEngine uci_engine = UciEngine(config, false);

    if (!uci_engine.start()) {
        std::cerr << "Engine did not start." << std::endl;
        return false;
    }

    if (!uci_engine.uci()) {
        std::cerr << "Writing uci command failed." << std::endl;
        return false;
    }

    if (!uci_engine.uciok()) {
        std::cerr << "Engine did not respond with uciok." << std::endl;
        return false;
    }

    if (uci_engine.isready() != process::Status::OK) {
        std::cerr << "Engine did not respond with readyok." << std::endl;
        return false;
    }

    if (!uci_engine.ucinewgame()) {
        std::cerr << "isready after ucinewgame failed." << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("position startpos")) {
        std::cerr << "Writing position startpos failed." << std::endl;
        return false;
    }

    if (uci_engine.isready() != process::Status::OK) {
        std::cerr << "Engine did not respond with readyok after position startpos." << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("position fen 3r2k1/p5n1/1pq1p2p/2p3p1/2P1P1n1/1P1P2pP/PN1Q2K1/5R2 w - - 0 27")) {
        std::cerr << "Writing position fen failed." << std::endl;
        return false;
    }

    if (uci_engine.isready() != process::Status::OK) {
        std::cerr << "Engine did not respond with readyok after position fen." << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("go wtime 100")) {
        std::cerr << "Writing go wtime 100 failed." << std::endl;
        return false;
    }

    if (uci_engine.readEngine("bestmove") != process::Status::OK) {
        std::cerr << "Engine did not respond with bestmove after go wtime 100." << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("go btime 100")) {
        std::cerr << "Writing go btime 100 failed." << std::endl;
        return false;
    }

    if (uci_engine.readEngine("bestmove") != process::Status::OK) {
        std::cerr << "Engine did not respond with bestmove after go btime 100." << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("go wtime 100 winc 100 btime 100 binc 100")) {
        std::cerr << "Writing go wtime 100 winc 100 btime 100 binc 100 failed." << std::endl;
        return false;
    }

    if (uci_engine.readEngine("bestmove") != process::Status::OK) {
        std::cerr << "Engine did not respond with bestmove after go wtime 100 winc 100 btime 100 binc 100."
                  << std::endl;
        return false;
    }

    if (!uci_engine.writeEngine("go btime 100 binc 100 wtime 100 winc 100")) {
        std::cerr << "Writing go btime 100 binc 100 wtime 100 winc 100 failed." << std::endl;
        return false;
    }

    if (uci_engine.readEngine("bestmove") != process::Status::OK) {
        std::cerr << "Engine did not respond with bestmove after go btime 100 binc 100 wtime 100 winc 100."
                  << std::endl;
        return false;
    }

    std::cout << "Engine passed all compliance checks." << std::endl;

    return true;
}
}  // namespace fastchess::engine