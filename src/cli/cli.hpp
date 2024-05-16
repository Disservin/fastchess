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

#include <cli/man.hpp>
#include <matchmaking/result.hpp>
#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>

namespace fast_chess::cli {

// Holds the data of the OptionParser
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
        throw std::runtime_error("Unrecognized " + std::string(name) + " option \"" +
                                 std::string(key) + "\" with value \"" + std::string(value) +
                                 "\".");
    }

    static void printVersion() {
        const static std::unordered_map<std::string, std::string> months(  //
            {{"Jan", "01"},
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

        constexpr std::string_view version = "dev";
        ss << "fast-chess ";
        if (version == "dev") ss << "dev-";
        else ss << version << "-";
#ifdef GIT_DATE
        ss << GIT_DATE;
#else
        date >> month >> day >> year;
        if (day.length() == 1) day = "0" + day;
        ss << year.substr(2) << months.at(month) << day;
#endif

#ifdef GIT_SHA
        ss << "-" << GIT_SHA;
#else
        ss << "-nogit"
#endif

        ss << "\n";

        std::cout << ss.str() << std::flush;
        std::exit(0);
    }

    static void printHelp() {
        std::cout << std::string(man::man, man::man_len) << std::flush;

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
    // Adds an option to the parser
    void addOption(const std::string &optionName, parseFunc func) {
        options_.insert(std::make_pair("-" + optionName, func));
    }

    // Parses the command line arguments and calls the corresponding option. Parse will
    // increment i if need be.
    void parse(int argc, char const *argv[]) {
        for (int i = 1; i < argc; i++) {
            const std::string arg = argv[i];
            if (options_.count(arg) == 0) {
                throw std::runtime_error("Unrecognized option: " + arg + " parsing failed.");
            }

            try {
                options_[arg](i, argc, argv, argument_data_);
            } catch (const std::exception &e) {
                auto err = "\nError while reading option \"" + arg + "\" with value \"" +
                           std::string(argv[i]) + "\"\nReason: " + e.what();

                throw std::runtime_error(err);
            }
        }
    }

    ArgumentData argument_data_;

    std::map<std::string, parseFunc> options_;
};

}  // namespace fast_chess::cli
