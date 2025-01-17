#pragma once

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <matchmaking/elo/elo_pentanomial.hpp>
#include <matchmaking/elo/elo_wdl.hpp>
#include <matchmaking/sprt/sprt.hpp>

namespace fastchess::cli {

// cli command to caluclate penta/wdl elo
// ll, ld, wl + dd, wd, ww
// --sprt 60 78 261 117 135 elo=0 elo1=5 [model=normalized|bayesian] [alpha=0.05] [beta=0.05]
void calculateSprt(const std::string& line) {
    Stats stats;

    auto getParam = [&line](const std::string& param) -> std::optional<std::string> {
        size_t pos = line.find(param + "=");
        if (pos == std::string::npos) return std::nullopt;

        pos += param.length() + 1;  // Skip "param="
        size_t end = line.find(' ', pos);
        if (end == std::string::npos) end = line.length();

        return std::string(line.substr(pos, end - pos));
    };

    // Parse the pentanomial statistics
    std::istringstream iss = std::istringstream(line);
    std::vector<int> numbers;
    std::string token;

    while (iss >> token) {
        if (std::isdigit(token[0])) {
            numbers.push_back(std::stoi(token));
        }
    }

    // Verify we have exactly 5 numbers for pentanomial stats
    if (numbers.size() >= 5) {
        stats.penta_LL = numbers[0];
        stats.penta_LD = numbers[1];
        stats.penta_WL = numbers[2];
        stats.penta_WD = numbers[3];
        stats.penta_WW = numbers[4];
    } else {
        throw std::runtime_error("Invalid number of pentanomial statistics");
    }

    // Parse optional parameters
    double alpha      = 0.05;          // default value
    double beta       = 0.05;          // default value
    double elo0       = 0.0;           // default value
    double elo1       = 5.0;           // default value
    std::string model = "normalized";  // default value

    if (auto a = getParam("alpha")) alpha = std::stod(*a);
    if (auto b = getParam("beta")) beta = std::stod(*b);
    if (auto e0 = getParam("elo")) elo0 = std::stod(*e0);
    if (auto e1 = getParam("elo1")) elo1 = std::stod(*e1);
    if (auto m = getParam("model")) model = *m;

    // Calculate ELO
    elo::EloPentanomial elo(stats);
    auto nelo = elo.nElo();

    // Calculate SPRT
    SPRT sprt(alpha, beta, elo0, elo1, model, true);
    auto llr = sprt.getLLR(stats, true);

    // Output results
    std::cout << "Pentanomial Statistics:\n";
    std::cout << "  LL: " << stats.penta_LL << "\n";
    std::cout << "  LD: " << stats.penta_LD << "\n";
    std::cout << "  WL+DD: " << stats.penta_WL << "\n";
    std::cout << "  WD: " << stats.penta_WD << "\n";
    std::cout << "  WW: " << stats.penta_WW << "\n\n";

    std::cout << "Parameters:\n";
    std::cout << "  Alpha: " << alpha << "\n";
    std::cout << "  Beta:  " << beta << "\n";
    std::cout << "  Elo0:  " << elo0 << "\n";
    std::cout << "  Elo1:  " << elo1 << "\n";
    std::cout << "  Model: " << model << "\n\n";

    std::cout << "Results:\n";
    std::cout << "  Normalized Elo: " << nelo << "\n";
    std::cout << "  LLR: " << llr << " " << sprt.getBounds() << "\n";

    std::cout << "\n";

    // Determine SPRT outcome
    auto res = sprt.getResult(llr);

    if (res == SPRT_H0) {
        std::cout << "SPRT Result: H1 was accepted\n";
    } else if (res == SPRT_H1) {
        std::cout << "SPRT Result: H0 was accepted\n";
    } else {
        std::cout << "SPRT Result: Continue testing\n";
    }
}

}  // namespace fastchess::cli