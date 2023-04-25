#pragma once

#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "engines/engine_config.hpp"
#include "logger.hpp"

namespace fast_chess {

namespace CMD {

struct OpeningOptions {
    std::string file;
    // TODO use enums for this
    std::string format;
    std::string order;
    int plies = 0;
    int start = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(OpeningOptions, file, format, order, plies, start)

struct PgnOptions {
    std::string file;
    std::string notation = "san";
    bool track_nodes = false;
    bool track_seldepth = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(PgnOptions, file, notation, track_nodes,
                                                track_seldepth)

struct SprtOptions {
    double alpha = 0.0;
    double beta = 0.0;
    double elo0 = 0.0;
    double elo1 = 0.0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(SprtOptions, alpha, beta, elo0, elo1)

struct DrawAdjudication {
    int move_number = 0;
    int move_count = 0;
    int score = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(DrawAdjudication, move_number, move_count, score,
                                                enabled)

struct ResignAdjudication {
    int move_count = 0;
    int score = 0;

    bool enabled = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(ResignAdjudication, move_count, score, enabled)

struct GameManagerOptions {
    ResignAdjudication resign = {};
    DrawAdjudication draw = {};

    OpeningOptions opening = {};
    PgnOptions pgn = {};

    SprtOptions sprt = {};

    std::string event_name = "?";
    std::string site = "?";

    /// @brief output format, fastchess or cutechess
    std::string output = "fastchess";

    uint32_t seed = 951356066;

    int ratinginterval = 10;

    int games = 2;
    int rounds = 0;

    int concurrency = 1;

    int overhead = 0;

    bool recover = false;

    bool report_penta = true;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(GameManagerOptions, event_name, site, seed,
                                                ratinginterval, games, rounds, concurrency,
                                                overhead, recover, report_penta, resign, draw,
                                                opening, pgn, sprt)

class Options {
   public:
    Options() = default;

    Options(int argc, char const *argv[]);

    std::vector<EngineConfiguration> getEngineConfigs() const;
    GameManagerOptions getGameOptions() const;

   private:
    bool isEngineSettableOption(const std::string &stringFormat) const;

    TimeControl parseTc(const std::string &tcString) const;

    void parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                              const std::string &value) const;

    // Generic function to parse a standalone value after a dash command.
    template <typename T>
    void parseValue(int &i, int argc, const char *argv[], T &optionValue) {
        i++;
        if (i < argc && argv[i][0] != '-') {
            if constexpr (std::is_same_v<T, int>)
                optionValue = std::stoi(argv[i]);
            else if constexpr (std::is_same_v<T, uint32_t>)
                optionValue = std::stoul(argv[i]);
            else if constexpr (std::is_same_v<T, float>)
                optionValue = std::stof(argv[i]);
            else if constexpr (std::is_same_v<T, double>)
                optionValue = std::stod(argv[i]);
            else if constexpr (std::is_same_v<T, bool>)
                optionValue = std::string(argv[i]) == "true";
            else
                optionValue = argv[i];
        }
    }

    // parse multiple keys with values after a dash command
    void parseDashOptions(int &i, int argc, char const *argv[],
                          std::function<void(std::string, std::string)> func);

    void printVersion(int &i) const;

    void coutMissingCommand(std::string_view name, std::string_view key,
                            std::string_view value) const;

    // Holds all the relevant settings for the handling of the games
    GameManagerOptions game_options_;

    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs_;
};
}  // namespace CMD

}  // namespace fast_chess
