#pragma once

#include <string>

#include <util/helper.hpp>

#include <types/draw_adjudication.hpp>
#include <types/engine_config.hpp>
#include <types/enums.hpp>
#include <types/epd.hpp>
#include <types/max_moves_adjudication.hpp>
#include <types/opening.hpp>
#include <types/pgn.hpp>
#include <types/resign_adjudication.hpp>
#include <types/sprt.hpp>

namespace fast_chess::config {

struct Tournament {
    Opening opening = {};
    Pgn pgn         = {};
    Epd epd         = {};

    Sprt sprt = {};

    std::string config_name;

    DrawAdjudication draw         = {};
    ResignAdjudication resign     = {};
    MaxMovesAdjudication maxmoves = {};

    VariantType variant = VariantType::STANDARD;

#ifdef USE_CUTE
    // output format, fastchess or cutechess
    OutputType output    = OutputType::CUTECHESS;
    int autosaveinterval = 0;
    int ratinginterval   = 0;
#else
    // output format, fastchess or cutechess
    OutputType output    = OutputType::FASTCHESS;
    int autosaveinterval = 20;
    int ratinginterval   = 10;
#endif

    uint64_t seed = 951356066;

    int scoreinterval = 1;

    int games       = 2;
    int rounds      = 2;
    int concurrency = 1;
    int overhead    = 0;

    bool recover          = false;
    bool report_penta     = true;
    bool affinity         = false;
    bool randomseed       = false;
    bool realtime_logging = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Tournament, resign, draw, maxmoves, opening, pgn, epd, sprt,
                                                config_name, output, seed, variant, ratinginterval, scoreinterval,
                                                autosaveinterval, games, rounds, concurrency, overhead, recover,
                                                report_penta, affinity, randomseed, realtime_logging)

}  // namespace fast_chess::config
