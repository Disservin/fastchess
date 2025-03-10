#pragma once

#include <string>

#include <core/helper.hpp>
#include <core/rand.hpp>

#include <types/draw_adjudication.hpp>
#include <types/engine_config.hpp>
#include <types/enums.hpp>
#include <types/epd.hpp>
#include <types/log.hpp>
#include <types/max_moves_adjudication.hpp>
#include <types/opening.hpp>
#include <types/pgn.hpp>
#include <types/resign_adjudication.hpp>
#include <types/sprt.hpp>

namespace fastchess::config {

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
    int games            = 1;
    int rounds           = 1;
    bool report_penta    = false;
#else
    // output format, fastchess or cutechess
    OutputType output    = OutputType::FASTCHESS;
    int autosaveinterval = 20;
    int ratinginterval   = 10;
    std::size_t games    = 2;
    std::size_t rounds   = 2;
    bool report_penta    = true;
#endif

    uint64_t seed = util::random::random_uint64();

    int scoreinterval = 1;

    int concurrency        = 1;
    uint32_t wait          = 0;
    bool force_concurrency = false;

    bool noswap       = false;
    bool reverse      = false;
    bool recover      = false;
    bool affinity     = false;
    bool show_latency = false;
    bool test_env     = false;

    Log log = {};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Tournament, resign, draw, maxmoves, opening, pgn, epd, sprt, config_name, output,
                                   seed, variant, ratinginterval, scoreinterval, wait, autosaveinterval, games, rounds,
                                   concurrency, force_concurrency, recover, noswap, reverse, report_penta, affinity,
                                   show_latency, log)

}  // namespace fastchess::config
