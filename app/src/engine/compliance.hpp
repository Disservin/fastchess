#pragma once

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#include <engine/uci_engine.hpp>

namespace fastchess::engine {

bool compliant(int argc, char const *argv[]) {
    int step = 0;

    EngineConfiguration config;
    config.cmd  = argv[2];
    config.args = argc > 3 ? std::string(argv[3]) : "";

    auto executeStep = [&step](const std::string &description, const std::function<bool()> &action) {
        step++;

        std::cout << "Step " << step << ": " << description << "..." << std::flush;

        if (!action()) {
            std::cerr << "\r\033[1;31m Failed\033[0m Step " << step << ": " << description << std::endl;
            return false;
        }

        std::cout << "\r" << "\033[1;32m Passed\033[0m Step " << step << ": " << description << std::endl;

        return true;
    };

    UciEngine uci_engine(config, false);

    std::vector<std::pair<std::string, std::function<bool()>>> steps = {
        {"Start the engine", [&uci_engine] { return uci_engine.start(); }},
        {"Check if engine is ready", [&uci_engine] { return uci_engine.isready() == process::Status::OK; }},
        {"Check id name", [&uci_engine] { return uci_engine.idName().has_value(); }},
        {"Check id author", [&uci_engine] { return uci_engine.idAuthor().has_value(); }},
        {"Send ucinewgame", [&uci_engine] { return uci_engine.ucinewgame(); }},
        {"Set position to startpos", [&uci_engine] { return uci_engine.writeEngine("position startpos"); }},
        {"Check if engine is ready after startpos",
         [&uci_engine] { return uci_engine.isready() == process::Status::OK; }},
        {"Set position to fen",
         [&uci_engine] {
             return uci_engine.writeEngine(
                 "position fen 3r2k1/p5n1/1pq1p2p/2p3p1/2P1P1n1/1P1P2pP/PN1Q2K1/5R2 w - - 0 27");
         }},

        {"Check if engine is ready after fen", [&uci_engine] { return uci_engine.isready() == process::Status::OK; }},
        {"Send go wtime 100", [&uci_engine] { return uci_engine.writeEngine("go wtime 100"); }},
        {"Read bestmove", [&uci_engine] { return uci_engine.readEngine("bestmove") == process::Status::OK; }},
        {"Check if engine prints an info line", [&uci_engine] { return !uci_engine.lastInfoLine().empty(); }},
        {"Verify info line contains score",
         [&uci_engine] { return str_utils::contains(uci_engine.lastInfoLine(), "score"); }},
        {"Set position to black to move",
         [&uci_engine] {
             return uci_engine.writeEngine("position fen rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
         }},
        {"Send go btime 100", [&uci_engine] { return uci_engine.writeEngine("go btime 100"); }},
        {"Read bestmove after go btime 100",
         [&uci_engine] { return uci_engine.readEngine("bestmove") == process::Status::OK; }},
        {"Check if engine prints an info line after go btime 100",
         [&uci_engine] { return !uci_engine.lastInfoLine().empty(); }},
        {"Check if engine prints an info line with the score after go btime 100",
         [&uci_engine] { return str_utils::contains(uci_engine.lastInfoLine(), "score"); }},
        {"Send go wtime 100 winc 100 btime 100 binc 100",
         [&uci_engine] { return uci_engine.writeEngine("go wtime 100 winc 100 btime 100 binc 100"); }},
        {"Read bestmove after go wtime 100 winc 100 btime 100 binc 100",
         [&uci_engine] { return uci_engine.readEngine("bestmove") == process::Status::OK; }},
        {"Check if engine prints an info line after go wtime 100 winc 100",
         [&uci_engine] { return !uci_engine.lastInfoLine().empty(); }},
        {"Check if engine prints an info line with the score after go wtime 100 winc 100",
         [&uci_engine] { return str_utils::contains(uci_engine.lastInfoLine(), "score"); }},
        {"Send go btime 100 binc 100 wtime 100 winc 100",
         [&uci_engine] { return uci_engine.writeEngine("go btime 100 binc 100 wtime 100 winc 100"); }},
        {"Read bestmove after go btime 100 binc 100 wtime 100 winc 100",
         [&uci_engine] { return uci_engine.readEngine("bestmove") == process::Status::OK; }},
        {"Check if engine prints an info line after go btime 100 binc 100",
         [&uci_engine] { return !uci_engine.lastInfoLine().empty(); }},
        {"Check if engine prints an info line with the score after go btime 100 binc 100",
         [&uci_engine] { return str_utils::contains(uci_engine.lastInfoLine(), "score"); }},
        {"Check if engine prints an info line after go btime 100 binc 100",
         [&uci_engine] { return str_utils::contains(uci_engine.lastInfoLine(), "score"); }},
        // Simulate a game
        {"Send ucinewgame", [&uci_engine] { return uci_engine.ucinewgame(); }},
        {"Set position to startpos", [&uci_engine] { return uci_engine.writeEngine("position startpos"); }},
        {"Send go wtime 100", [&uci_engine] { return uci_engine.writeEngine("go wtime 100 btime 100"); }},
        {"Read bestmove after go wtime 100 btime 100",
         [&uci_engine] {
             return uci_engine.readEngine("bestmove") == process::Status::OK && uci_engine.bestmove() != std::nullopt;
         }},
        {"Set position to startpos moves e2e4 e7e5",
         [&uci_engine] { return uci_engine.writeEngine("position startpos moves e2e4 e7e5"); }},
        {"Send go wtime 100 btime 100", [&uci_engine] { return uci_engine.writeEngine("go wtime 100 btime 100"); }},
        {"Read bestmove after position startpos moves e2e4 e7e5", [&uci_engine] {
             return uci_engine.readEngine("bestmove") == process::Status::OK && uci_engine.bestmove() != std::nullopt;
         }}};

    for (const auto &[description, action] : steps) {
        if (!executeStep(description, action)) {
            return false;
        }
    }

    std::cout << "Engine passed all compliance checks." << std::endl;

    return true;
}

}  // namespace fastchess::engine
