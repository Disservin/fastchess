#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <core/printing/printing.h>
#include <cli/cli_args.hpp>
#include <cli/man.hpp>
#include <core/config/config.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/exception.hpp>
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
    using parseFunc = std::function<void(const std::vector<std::string> &, ArgumentData &)>;

    enum class ParamStyle { Free, None, Single, KeyValue };

    struct OptionEntry {
        parseFunc func;
        bool deferred = false;
    };

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
        throw fastchess_exception("Unrecognized " + std::string(name) + " option \"" + std::string(key) +
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
    enum class Dispatch { Immediate, Deferred };

    template <typename Handler>
    void addOption(const std::string &optionName, Handler &&handler) {
        addOption<ParamStyle::Free, Dispatch::Immediate>(optionName, std::forward<Handler>(handler));
    }

    template <ParamStyle Style, typename Handler>
    void addOption(const std::string &optionName, Handler &&handler) {
        addOption<Style, Dispatch::Immediate>(optionName, std::forward<Handler>(handler));
    }

    template <ParamStyle Style, Dispatch Mode, typename Handler>
    void addOption(const std::string &optionName, Handler &&handler) {
        std::string flag = optionName;
        flag.insert(flag.begin(), '-');

        options_.insert(std::make_pair(
            flag, OptionEntry{wrapHandler<Style>(flag, std::forward<Handler>(handler)), Mode == Dispatch::Deferred}));
    }

    void registerOptions();

    // Parses the command line arguments and calls the corresponding option. Parse will
    // increment i if need be.
    void parse(const cli::Args &args) {
        std::unordered_map<std::string, std::vector<std::string>> deferred;

        const auto joinParams = [](const std::vector<std::string> &params) -> std::string {
            if (params.empty()) return std::string("<none>");
            std::ostringstream oss;
            for (std::size_t i = 0; i < params.size(); ++i) {
                if (i != 0) oss << ' ';
                oss << params[i];
            }
            return oss.str();
        };

        for (int i = 1; i < args.argc(); i++) {
            const std::string arg = args[i];
            if (options_.count(arg) == 0) {
                throw fastchess_exception("Unrecognized option: " + arg + " parsing failed.");
            }

            try {
                std::vector<std::string> params;

                while (i + 1 < args.argc() && args[i + 1][0] != '-') {
                    params.push_back(args[++i]);
                }

                const auto &entry = options_.at(arg);
                if (entry.deferred) {
                    auto &slot = deferred[arg];
                    slot.insert(slot.end(), params.begin(), params.end());
                    continue;
                }

                entry.func(params, argument_data_);

            } catch (const std::exception &e) {
                auto err =
                    fmt::format("Error while reading option \"{}\" with value \"{}\"", arg, std::string(args[i]));
                auto msg = fmt::format("Reason: {}", e.what());

                throw fastchess_exception(err + "\n" + msg);
            }
        }

        for (auto &[flag, params] : deferred) {
            try {
                const auto &entry = options_.at(flag);
                entry.func(params, argument_data_);
            } catch (const std::exception &e) {
                auto err = fmt::format("Error while reading option \"{}\" with value \"{}\"", flag, joinParams(params));
                auto msg = fmt::format("Reason: {}", e.what());

                throw fastchess_exception(err + "\n" + msg);
            }
        }
    }

    template <ParamStyle Style, typename Handler>
    parseFunc wrapHandler(const std::string &flag, Handler &&handler) {
        if constexpr (Style == ParamStyle::None) {
            static_assert(std::is_invocable_r_v<void, Handler, ArgumentData &>, "Handler must accept (ArgumentData&)");
            std::function<void(ArgumentData &)> fn = std::forward<Handler>(handler);
            return [flag, fn](const std::vector<std::string> &params, ArgumentData &data) {
                if (!params.empty()) {
                    throw fastchess_exception("Option \"" + flag + "\" does not accept parameters.");
                }
                fn(data);
            };
        } else if constexpr (Style == ParamStyle::Single) {
            static_assert(std::is_invocable_r_v<void, Handler, std::string_view, ArgumentData &>,
                          "Handler must accept (std::string_view, ArgumentData&)");
            std::function<void(std::string_view, ArgumentData &)> fn = std::forward<Handler>(handler);
            return [flag, fn](const std::vector<std::string> &params, ArgumentData &data) {
                if (params.size() != 1) {
                    throw fastchess_exception("Option \"" + flag + "\" expects exactly one value.");
                }
                fn(params.front(), data);
            };
        } else if constexpr (Style == ParamStyle::KeyValue) {
            static_assert(std::is_invocable_r_v<void, Handler, const std::vector<std::pair<std::string, std::string>> &,
                                                ArgumentData &>,
                          "Handler must accept (key/value list, ArgumentData&)");
            std::function<void(const std::vector<std::pair<std::string, std::string>> &, ArgumentData &)> fn =
                std::forward<Handler>(handler);
            return [flag, fn](const std::vector<std::string> &params, ArgumentData &data) {
                if (params.empty()) {
                    throw fastchess_exception("Option \"" + flag + "\" expects key=value parameters.");
                }

                std::vector<std::pair<std::string, std::string>> kv;
                kv.reserve(params.size());
                for (const auto &param : params) {
                    const auto pos = param.find('=');
                    if (pos == std::string::npos || pos == 0 || pos + 1 == param.size()) {
                        throw fastchess_exception("Option \"" + flag + "\" expects key=value pairs, got \"" + param +
                                                  "\".");
                    }
                    kv.emplace_back(param.substr(0, pos), param.substr(pos + 1));
                }

                fn(kv, data);
            };
        } else {
            static_assert(std::is_invocable_r_v<void, Handler, const std::vector<std::string> &, ArgumentData &>,
                          "Handler must accept (value list, ArgumentData&)");
            std::function<void(const std::vector<std::string> &, ArgumentData &)> fn = std::forward<Handler>(handler);
            return [fn](const std::vector<std::string> &params, ArgumentData &data) { fn(params, data); };
        }
    }

    ArgumentData argument_data_;

    std::unordered_map<std::string, OptionEntry> options_;
};

}  // namespace fastchess::cli
