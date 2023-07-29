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

#include <engines/engine_config.hpp>
#include <logger.hpp>
#include <types.hpp>

namespace fast_chess::cmd {

/// @brief Holds the data of the OptionParser
struct ArgumentData {
    // Holds all the relevant settings for the handling of the games
    GameManagerOptions game_options;

    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    stats_map stats;

    /*previous olded values before config*/
    GameManagerOptions old_game_options;
    std::vector<EngineConfiguration> old_configs;
};

/// @brief Base class for all options
class Option {
   public:
    virtual void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) = 0;

    virtual ~Option() = default;
};

class OptionsParser {
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

        ss << "fast-chess alpha-0.5.0 ";
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

        std::cout << ss.str();
        exit(0);
    }

    static void printHelp() {
        std::cout << "Fast-Chess is a command-line tool designed for creating chess engine tournaments.\n"
                  << "Homepage: https://github.com/Disservin/fast-chess\n\n";
        std::cout << "Options:\n";

        std::cout << "  -engine                           placeholder\n"
                  << "  -each                             Options to apply for both engines.\n"
                  << "  -pgnout                           placeholder\n"
                  << "  -openings                         placeholder\n"
                  << "  -sprt elo0=ELO0 elo1=ELO1         Starts a SPRT test between the engines with the given parameters.\n"
                  << "        alpha=ALPHA beta=BETA                  \n"
                  << "  -draw                             placeholder\n"
                  << "  -resign                           placeholder\n"
                  << "  -log file=NAME                    Debug information will be logged into NAME.\n"
                  << "  -config                           placeholder\n"
                  << "  -report                           placeholder\n"
                  << "  -output format=FORMAT             FORMAT can be \"cutechess\" or \"fastchess\" (default) change this if you have scripts\n"
                  << "                                    that only parse cutechess like output. This is experimental as of now.\n"
                  << "  -concurrency                      Number of games to run simultaneously.\n"
                  << "  -event                            placeholder\n"
                  << "  -site                             placeholder\n"
                  << "  -games                            This should be set to 1 or 2, each round will play n games with,\n"
                  << "                                    setting this higher than 2 does not really make sense.\n"
                  << "  -rounds                           placeholder\n"
                  << "  -ratinginterval                   placeholder\n"
                  << "  -srand SEED                       Sets the random seed to SEED.\n"
                  << "  -version                          Prints version and exit.\n"
                  << "  -help                             Prints this help and exit.\n"
                  << "  -recover                          placeholder\n"
                  << "  -repeat                           This has the same effect as -games 2 and is the default.\n"
                  << "  -variant                          placeholder\n";
        exit(0);
    }

    void saveJson(const stats_map &stats) const {
        nlohmann::ordered_json jsonfile = argument_data_.game_options;
        jsonfile["engines"] = argument_data_.configs;
        jsonfile["stats"] = stats;

        std::ofstream file("config.json");
        file << std::setw(4) << jsonfile << std::endl;
    }

    [[nodiscard]] std::vector<EngineConfiguration> getEngineConfigs() const {
        return argument_data_.configs;
    }
    [[nodiscard]] GameManagerOptions getGameOptions() const { return argument_data_.game_options; }

    [[nodiscard]] stats_map getResults() const { return argument_data_.stats; }

   private:
    /// @brief Adds an option to the parser
    /// @param optionName
    /// @param option
    void addOption(const std::string &optionName, Option *option) {
        options_.insert(std::make_pair("-" + optionName, std::unique_ptr<Option>(option)));
    }

    /// @brief Parses the command line arguments and calls the corresponding option. Parse will
    /// increment i if need be.
    /// @param argc
    /// @param argv
    void parse(int argc, char const *argv[]) {
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            if (options_.count(arg) > 0) {
                options_[arg]->parse(i, argc, argv, argument_data_);
            } else {
                throw std::runtime_error("Unrecognized option: " + arg + " parsing failed.");
            }
        }
    }

    std::map<std::string, std::unique_ptr<Option>> options_;

    ArgumentData argument_data_;
};

/// @brief Generic function to parse a standalone value after a dash command. i.e -concurrency 10
/// @tparam T
/// @param i
/// @param argc
/// @param argv
/// @param optionValue
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

/// @brief Generic function to parse a standalone value after a dash command. Continues parsing
/// until another dash command is found
/// @param i
/// @param argc
/// @param argv
/// @param func
inline void parseDashOptions(int &i, int argc, char const *argv[],
                             const std::function<void(std::string, std::string)> &func) {
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        std::string param = argv[i];
        std::size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        func(key, value);
    }
}

inline std::string parseUntilDash(int &i, int argc, char const *argv[]) {
    std::string result;
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        result += argv[i] + std::string(" ");
    }
    return result.erase(result.size() - 1);
}

}  // namespace fast_chess::cmd