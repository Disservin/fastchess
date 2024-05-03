#pragma once

#include <string>

#include <types/engine_config.hpp>
#include <types/enums.hpp>

namespace fast_chess::options {

struct Opening {
    std::string file;
    FormatType format = FormatType::NONE;
    OrderType order   = OrderType::RANDOM;
    int plies         = -1;
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

struct Epd {
    std::string file;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Epd, file)

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

struct MaxMovesAdjudication {
    int move_count  = 1;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(MaxMovesAdjudication, move_count)

struct Tournament {
    Opening opening = {};
    Pgn pgn         = {};
    Epd epd         = {};

    Sprt sprt = {};

    std::string event_name = "Fast-Chess Tournament";
    std::string site       = "?";

    DrawAdjudication draw         = {};
    ResignAdjudication resign     = {};
    MaxMovesAdjudication maxmoves = {};

    VariantType variant = VariantType::STANDARD;

#ifdef USE_CUTE
    // output format, fastchess or cutechess
    OutputType output = OutputType::CUTECHESS;
#else
    // output format, fastchess or cutechess
    OutputType output = OutputType::FASTCHESS;
#endif

    uint32_t seed = 951356066;

    int ratinginterval = 10;

    int games       = 2;
    int rounds      = 2;
    int concurrency = 1;
    int overhead    = 0;

    bool recover      = false;
    bool report_penta = true;
    bool affinity     = false;
    bool randomseed   = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Tournament, resign, draw, maxmoves, opening, pgn, epd, 
                                                sprt, event_name, site, output, seed, variant,
                                                ratinginterval, games, rounds, concurrency,
                                                overhead, recover, report_penta, affinity, randomseed)

}  // namespace fast_chess::options
