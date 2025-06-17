#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <core/printing/printing.h>
#include <cli/cli_args.hpp>
#include <cli/man.hpp>
#include <core/config/config.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

#define FMT_HEADER_ONLY
#include <fmt/include/fmt/core.h>

namespace fastchess {
extern const char *version;
}

namespace fastchess::cli {

// Holds the data of the OptionParser
struct ArgumentData {
    // Holds all the relevant settings for the handling of the games
    config::Tournament tournament_config;
    /*previous olded values before config*/
    config::Tournament old_tournament_config;

    stats_map stats;

    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    std::vector<EngineConfiguration> old_configs;
};

class OptionsParser {
    using parseFunc = std::function<void((const std::vector<std::string> &, ArgumentData &))>;

    static std::string getVersion() {
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

        ss << "fastchess " << version;

#ifdef COMPILE_MSG
        ss << COMPILE_MSG << " ";
#endif

#ifndef RELEASE
#    ifdef GIT_DATE
        ss << GIT_DATE;
#    else
        date >> month >> day >> year;
        if (day.length() == 1) day = "0" + day;
        ss << year.substr(2) << months.at(month) << day;
#    endif

#    ifdef GIT_SHA
        ss << "-" << GIT_SHA;
#    endif
#endif

#ifdef USE_CUTE
        ss << " (compiled with cutechess output)";
#endif

        return ss.str();
    }

   public:
    OptionsParser(const cli::Args &args);

    static void throwMissing(std::string_view name, std::string_view key, std::string_view value) {
        throw std::runtime_error("Unrecognized " + std::string(name) + " option \"" + std::string(key) +
                                 "\" with value \"" + std::string(value) + "\".");
    }

    static void printHelp() {
        setTerminalOutput();

        for (const auto c : man::man) {
            std::cout << c;
        }

        std::flush(std::cout);

        std::exit(0);
    }

    [[nodiscard]] std::vector<EngineConfiguration> getEngineConfigs() const { return argument_data_.configs; }

    [[nodiscard]] config::Tournament getTournamentConfig() const { return argument_data_.tournament_config; }

    [[nodiscard]] stats_map getResults() const { return argument_data_.stats; }

    const inline static auto Version = getVersion();

   private:
    // Adds an option to the parser
    void addOption(const std::string &optionName, parseFunc func) {
        options_.insert(std::make_pair("-" + optionName, func));
    }

    // Parses the command line arguments and calls the corresponding option. Parse will
    // increment i if need be.
    void parse(const cli::Args &args) {
        std::vector<std::string> each;
        for (int i = 1; i < args.argc(); i++) {
            const std::string arg = args[i];
            if (options_.count(arg) == 0) {
                throw std::runtime_error("Unrecognized option: " + arg + " parsing failed.");
            }

            try {
                std::vector<std::string> params;

                while (i + 1 < args.argc() && args[i + 1][0] != '-') {
                    if (arg != "-each")
                        params.push_back(args[++i]);
                    else
                        each.push_back(args[++i]);
                }
                if (arg != "-each")
                    options_.at(arg)(params, argument_data_);

            } catch (const std::exception &e) {
                auto err =
                    fmt::format("Error while reading option \"{}\" with value \"{}\"", arg, std::string(args[i]));
                auto msg = fmt::format("Reason: {}", e.what());

                throw std::runtime_error(err + "\n" + msg);
            }
        }
        if (each.size() > 0) {
            try {
                options_.at("-each")(each, argument_data_);
            } catch (const std::exception &e) {
                auto err =
                    fmt::format("Error while reading option \"{}\" with value \"{}\"", "-each", each.back());
                auto msg = fmt::format("Reason: {}", e.what());

                throw std::runtime_error(err + "\n" + msg);
            }
        }
    }

    ArgumentData argument_data_;

    std::map<std::string, parseFunc> options_;
};

}  // namespace fastchess::cli
