#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <core/printing/printing.h>
#include <cli/args.hpp>
#include <cli/cli_args.hpp>
#include <cli/man.hpp>
#include <cli/sanitize.hpp>
#include <core/config/config.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fastchess {
extern const char* version;
}

namespace fastchess::cli {

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

const inline static auto Version = getVersion();

// Holds the data of the OptionParser (same as before)

CommandLineParser create_parser();

inline auto cli_parse(const cli::Args& args) {
    auto parser  = create_parser();
    auto options = parser.parse(args.args());

    // apply the variant type to all configs
    for (auto& config : options.configs) {
        config.variant = options.tournament_config.variant;
    }

    // sanitize for now, should be moved to the validators
    cli::sanitize(options.tournament_config);

    cli::sanitize(options.configs);

    struct Parsed {
        Parsed(ArgumentData&& options) : options(options) {}

        [[nodiscard]] std::vector<EngineConfiguration> getEngineConfigs() const { return options.configs; }

        [[nodiscard]] config::Tournament getTournamentConfig() const { return options.tournament_config; }

        [[nodiscard]] stats_map getResults() const { return options.stats; }

       private:
        ArgumentData options;
    };

    return Parsed(std::move(options));
}
}  // namespace fastchess::cli