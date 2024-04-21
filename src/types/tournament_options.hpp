#pragma once

#include <string>
#include <random>

#include <types/engine_config.hpp>
#include <types/enums.hpp>

namespace fast_chess::options {

struct Opening {
    std::string file;
    FormatType format = FormatType::NONE;
    OrderType order   = OrderType::RANDOM;
    int plies         = 0;
    int start         = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Opening, file, format, order, plies, start)

struct Pgn {
    std::string file;
    NotationType notation = NotationType::SAN;
    bool track_nodes      = false;
    bool track_seldepth   = false;
    bool track_nps        = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Pgn, file, notation, track_nodes, track_seldepth,
                                                track_nps)

struct Sprt {
    double alpha         = 0.0;
    double beta          = 0.0;
    double elo0          = 0.0;
    double elo1          = 0.0;
    bool logistic_bounds = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Sprt, alpha, beta, elo0, elo1, logistic_bounds)

struct DrawAdjudication {
    int move_number = 0;
    int move_count  = 1;
    int score       = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(DrawAdjudication, move_number, move_count, score,
                                                enabled)

struct ResignAdjudication {
    int move_count = 1;
    int score      = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(ResignAdjudication, move_count, score, enabled)

struct Tournament {
    Opening opening = {};
    Pgn pgn         = {};

    Sprt sprt = {};

    std::string event_name = "Fast Chess";
    std::string site       = "?";

    DrawAdjudication draw     = {};
    ResignAdjudication resign = {};

    VariantType variant = VariantType::STANDARD;

#ifdef USE_CUTE
    /// @brief output format, fastchess or cutechess
    OutputType output = OutputType::CUTECHESS;
#else
    /// @brief output format, fastchess or cutechess
    OutputType output = OutputType::FASTCHESS;
#endif
    std::random_device rd;  // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); // Standard mersenne_twister_engine seeded with rd()

    // Define the range for the random number
    std::uniform_int_distribution<> distrib(0, 4294967294);

    // Generate a random number
    uint32_t seed = distrib(gen);

    int ratinginterval = 10;

    int games       = 2;
    int rounds      = 2;
    int concurrency = 1;
    int overhead    = 0;

    bool recover      = false;
    bool report_penta = true;
    bool affinity     = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Tournament, resign, draw, opening, pgn, sprt,
                                                event_name, site, output, seed, variant,
                                                ratinginterval, games, rounds, concurrency,
                                                overhead, recover, report_penta)

}  // namespace fast_chess::options
