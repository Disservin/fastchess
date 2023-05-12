#pragma once

#include <engines/engine_config.hpp>
#include <logger.hpp>
#include <types.hpp>

namespace fast_chess {
namespace cmd {

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

    static void printVersion(int &i) {
        i++;
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

        ss << "fast-chess ";
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
    void addOption(std::string optionName, Option *option) {
        options_.insert(std::make_pair("-" + optionName, std::unique_ptr<Option>(option)));
    }

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

inline void parseDashOptions(int &i, int argc, char const *argv[],
                             std::function<void(std::string, std::string)> func) {
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        std::string param = argv[i];
        std::size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        func(key, value);
    }
}

}  // namespace cmd
}  // namespace fast_chess