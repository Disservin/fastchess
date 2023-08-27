#pragma once

#include <string>

#include <engines/engine_config.hpp>
#include <enums.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>

namespace fast_chess::cmd {

struct OpeningOptions {
    std::string file;
    FormatType format = FormatType::NONE;
    OrderType order   = OrderType::RANDOM;
    int plies         = 0;
    int start         = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(OpeningOptions, file, format, order, plies, start)

struct PgnOptions {
    std::string file;
    NotationType notation = NotationType::SAN;
    bool track_nodes      = false;
    bool track_seldepth   = false;
    bool track_nps        = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(PgnOptions, file, notation, track_nodes,
                                                track_seldepth, track_nps)

struct SprtOptions {
    double alpha = 0.0;
    double beta  = 0.0;
    double elo0  = 0.0;
    double elo1  = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(SprtOptions, alpha, beta, elo0, elo1)

struct DrawAdjudication {
    std::size_t move_number = 0;
    std::size_t move_count  = 0;
    int score               = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(DrawAdjudication, move_number, move_count, score,
                                                enabled)

struct ResignAdjudication {
    std::size_t move_count = 0;
    int score              = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(ResignAdjudication, move_count, score, enabled)

struct TournamentOptions {
    ResignAdjudication resign = {};
    DrawAdjudication draw     = {};

    OpeningOptions opening = {};
    PgnOptions pgn         = {};

    SprtOptions sprt = {};

    std::string event_name = "Fast Chess";
    std::string site       = "?";

    VariantType variant = VariantType::STANDARD;

#ifdef USE_CUTE
    /// @brief output format, fastchess or cutechess
    OutputType output = OutputType::CUTECHESS;
#else
    /// @brief output format, fastchess or cutechess
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
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(TournamentOptions, resign, draw, opening, pgn, sprt,
                                                event_name, site, output, seed, variant,
                                                ratinginterval, games, rounds, concurrency,
                                                overhead, recover, report_penta)

}  // namespace fast_chess::cmd
