#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>
#include <util/logger.hpp>

namespace fast_chess::cli {

/// @brief Holds the data of the OptionParser
struct ArgumentData {
    // Holds all the relevant settings for the handling of the games
    options::Tournament tournament_options;
    /*previous olded values before config*/
    options::Tournament old_tournament_options;

    stats_map stats;

    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    std::vector<EngineConfiguration> old_configs;
};

class OptionsParser {
    using parseFunc = std::function<void(int &, int, char const *[], ArgumentData &)>;

   public:
    OptionsParser(int argc, char const *argv[]);

    static void throwMissing(std::string_view name, std::string_view key, std::string_view value) {
        throw std::runtime_error("Unrecognized " + std::string(name) +
                                 " option: " + std::string(key) + " with value " +
                                 std::string(value) + " parsing failed.");
    }

    static void printVersion() {
        std::unordered_map<std::string, std::string> months({{"Jan", "01"},
                                                             {"Feb", "02"},
                                                             {"Mar", "03"},
                                                             {"Apr", "04"},
                                                             {"May", "05"},
                                                             {"Jun", "06"},
                                                             {"Jul", "07"},
                                                             {"Aug", "08"},
                                                             {"Sep", "09"},
                                                             {"Oct", "10"},
                                                             {"Nov", "11"},
                                                             {"Dec", "12"}});

        std::string month, day, year;
        std::stringstream ss, date(__DATE__);  // {month} {date} {year}

        ss << "fast-chess alpha-0.6.3-";
#ifdef GIT_DATE
        ss << GIT_DATE;
#else
        date >> month >> day >> year;
        if (day.length() == 1) day = "0" + day;
        ss << year.substr(2) << months[month] << day;
#endif

#ifdef GIT_SHA
        ss << "-" << GIT_SHA;
#endif

        ss << "\n";

        std::cout << ss.str() << std::flush;
        std::exit(0);
    }

    static void printHelp() {
        // clang-format off
        std::cout << "Fast-Chess is a command-line tool designed for creating chess engine tournaments.\n"
                  << "Homepage: https://github.com/Disservin/fast-chess\n\n";
        std::cout << "Options:\n";

        std::cout << "  -engine OPTIONS                        View the Readme for the usage of OPTIONS.\n"
                  << "  -each OPTIONS                          OPTIONS to apply for both engines.\n"
                  << "  -pgnout notation=NOTATION file=FILE    The played games will be exported into FILE with pgn format. NOTATION can\n"
                  << "          nodes=true seldepth=true         be \"san\" (default), \"lan\" and \"uci\". Extra pgn comments can be turned off with\n"
                  << "          nps=true                         nodes/seldepth/nps = false.\n"
                  << "  -openings file=FILE format=FORMAT      Use openings from FILE with FORMAT=epd/pgn. \"pgn\" format is experimental.\n"
                  << "            order=ORDER plies=PLIES        ORDER=random/sequential sets the order in which the openings will be played starting\n"
                  << "            start=START                    with an offset of START.\n"
                  << "  -sprt elo0=ELO0 elo1=ELO1              Starts a SPRT test between the engines with the given parameters.\n"
                  << "        alpha=ALPHA beta=BETA                       \n"
                  << "  -draw movenumber=NUMBER                After NUMBER of moves into a game both engine's evaluation is below SCORE for COUNT\n"
                  << "        movecount=COUNT score=SCORE        consecutive moves the game is declared draw.\n"
                  << "  -resign movecount=COUNT score=SCORE    If an engine's evaluation is over SCORE for COUNT moves, it resigns.\n"
                  << "  -log file=NAME level=LEVEL             Debug information will be logged into NAME. LEVEL can be \"ALL\", \"trace\", \"warn\", \"info\", \"err\" or \"fatal\".\n"
                  << "                                           The default LEVEL is \"warn\", it will log warnings, info, errors and fatal.\n"
                  << "  -config file=NAME discard=true         Read fast-chess settings from a config json file. Discard prevents the current one from being\n"
                  << "                                           overwritten.\n"
                  << "  -report penta=PENTA                    PENTA should be a boolean value, to signal which report output should be used.\n"
                  << "  -output format=FORMAT                  FORMAT can be \"cutechess\" or \"fastchess\" (default) change this if you have scripts\n"
                  << "                                           that only parse cutechess like output. This is experimental as of now.\n"
                  << "  -concurrency N                         Number of games to run simultaneously.\n"
                  << "  -event VALUE                           Set the event header tag in the pgn.\n"
                  << "  -site VALUE                            Set the site header tag in the pgn.\n"
                  << "  -games                                 This should be set to 1 or 2, each round will play n games with,\n"
                  << "                                           setting this higher than 2 does not really make sense.\n"
                  << "  -rounds N                              Sets the number of rounds to be played to N.\n"
                  << "  -ratinginterval N                      Print rating updates every N. When Penta reporting is activated this counts game pairs.\n"
                  << "  -srand SEED                            Sets the random seed to SEED.\n"
                  << "  -version                               Prints version and exits. Same as -v.\n"
                  << "  -help                                  Prints this help and exit.\n"
                  << "  -recover                               Don't crash when an engine plays an illegal move or crashes. Give a warning and countinue\n"
                  << "                                           with the tournament.\n"
                  << "  -repeat                                This has the same effect as -games 2 and is the default.\n"
                  << "  -quick                                 This is a shortcut for\n"
                  << "                                           -engine cmd=ENGINE1 -engine cmd=ENGINE2 -each tc=10+0.1 -rounds 25000 -repeat -concurrency max - 2\n"
                  << "                                           -openings file=BOOK format=epd order=random -draw movecount=8 score=8 movenumber=30\n"
                  << "  -variant VALUE                         VALUE is either `standard` (default) or `fischerandom`.\n"
                  << "  -no-affinity                           Disables the affinity setting for the engines.\n"
                  << std::flush;
        // clang-format on

        std::exit(0);
    }

    [[nodiscard]] std::vector<EngineConfiguration> getEngineConfigs() const {
        return argument_data_.configs;
    }

    [[nodiscard]] options::Tournament getGameOptions() const {
        return argument_data_.tournament_options;
    }

    [[nodiscard]] stats_map getResults() const { return argument_data_.stats; }

   private:
    /// @brief Adds an option to the parser
    /// @param optionName
    /// @param option
    void addOption(const std::string &optionName, parseFunc func) {
        options_.insert(std::make_pair("-" + optionName, func));
    }

    /// @brief Parses the command line arguments and calls the corresponding option. Parse will
    /// increment i if need be.
    /// @param argc
    /// @param argv
    void parse(int argc, char const *argv[]) {
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            if (options_.count(arg) > 0) {
                options_[arg](i, argc, argv, argument_data_);
            } else {
                throw std::runtime_error("Unrecognized option: " + arg + " parsing failed.");
            }
        }
    }

    ArgumentData argument_data_;

    std::map<std::string, parseFunc> options_;
};

}  // namespace fast_chess::cli