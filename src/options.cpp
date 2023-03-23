#include "options.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>
#include <unordered_map>

#include "third_party/json.hpp"

namespace fast_chess {

namespace CMD {

using json = nlohmann::json;

Options::Options(int argc, char const *argv[]) {
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "-engine") {
            configs_.push_back(EngineConfiguration());
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                parseEngineKeyValues(configs_.back(), key, value);
            });
        } else if (arg == "-each")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                for (auto &config : configs_) parseEngineKeyValues(config, key, value);
            });
        else if (arg == "-pgnout")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    game_options_.pgn.file = value;
                else if (key == "tracknodes")
                    game_options_.pgn.track_nodes = true;
                else if (key == "trackseldepth")
                    game_options_.pgn.track_seldepth = true;
                else if (key == "notation")
                    game_options_.pgn.notation = value;
                else
                    coutMissingCommand("pgnout", key, value);
            });
        else if (arg == "-openings")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    game_options_.opening.file = value;
                else if (key == "format")
                    game_options_.opening.format = value;
                else if (key == "order")
                    game_options_.opening.order = value;
                else if (key == "plies")
                    game_options_.opening.plies = std::stoi(value);
                else if (key == "start")
                    game_options_.opening.start = std::stoi(value);
                else
                    coutMissingCommand("openings", key, value);
            });
        else if (arg == "-sprt")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (game_options_.rounds == 0) game_options_.rounds = 500000;

                if (key == "elo0")
                    game_options_.sprt.elo0 = std::stod(value);
                else if (key == "elo1")
                    game_options_.sprt.elo1 = std::stod(value);
                else if (key == "alpha")
                    game_options_.sprt.alpha = std::stod(value);
                else if (key == "beta")
                    game_options_.sprt.beta = std::stod(value);
                else
                    coutMissingCommand("sprt", key, value);
            });
        else if (arg == "-draw")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                game_options_.draw.enabled = true;

                if (key == "movenumber")
                    game_options_.draw.move_number = std::stoi(value);
                else if (key == "movecount")
                    game_options_.draw.move_count = std::stoi(value);
                else if (key == "score")
                    game_options_.draw.score = std::stoi(value);
                else
                    coutMissingCommand("draw", key, value);
            });
        else if (arg == "-resign")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                game_options_.resign.enabled = true;

                if (key == "movecount")
                    game_options_.resign.move_count = std::stoi(value);
                else if (key == "score")
                    game_options_.resign.score = std::stoi(value);
                else
                    coutMissingCommand("resign", key, value);
            });
        else if (arg == "-log")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    Logger::openFile(value);
                else
                    coutMissingCommand("log", key, value);
            });
        else if (arg == "-config")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    loadJson(value);
                else if (key == "discard" && value == "true") {
                    std::cout << "Discarded previous results.\n";
                    stats_.clear();
                } else
                    coutMissingCommand("config", key, value);
            });
        else if (arg == "-report")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "penta") {
                    game_options_.report_penta = value == "true";
                } else
                    coutMissingCommand("report", key, value);
            });
        else if (arg == "-concurrency")
            parseValue(i, argc, argv, game_options_.concurrency);
        else if (arg == "-event")
            parseValue(i, argc, argv, game_options_.event_name);
        else if (arg == "-site")
            parseValue(i, argc, argv, game_options_.site);
        else if (arg == "-games")
            parseValue(i, argc, argv, game_options_.games);
        else if (arg == "-rounds")
            parseValue(i, argc, argv, game_options_.rounds);
        else if (arg == "-ratinginterval")
            parseValue(i, argc, argv, game_options_.ratinginterval);
        else if (arg == "-srand")
            parseValue(i, argc, argv, game_options_.seed);
        else if (arg == "-version")
            printVersion(i);
        else if (arg == "-recover")
            game_options_.recover = true;
        else if (arg == "-repeat")
            game_options_.games = 2;
        else
            throw std::runtime_error("Dash command: " + arg + " doesnt exist!");
    }

    for (auto &config : configs_) {
        if (config.name.empty()) throw std::runtime_error("Warning: Each engine must have a name!");

        config.recover = game_options_.recover;
    }
}

TimeControl Options::parseTc(const std::string &tcString) const {
    TimeControl tc;

    std::string remainingStringVector = tcString;
    const bool has_moves = contains(tcString, "/");
    const bool has_inc = contains(tcString, "+");

    if (has_moves) {
        const auto moves = splitString(tcString, '/');
        tc.moves = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc) {
        const auto moves = splitString(remainingStringVector, '+');
        tc.increment = std::stod(moves[1].c_str()) * 1000;
        remainingStringVector = moves[0];
    }

    tc.time = std::stod(remainingStringVector.c_str()) * 1000;

    return tc;
}

void Options::parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                                   const std::string &value) const {
    if (key == "cmd")
        engineConfig.cmd = value;
    else if (key == "name")
        engineConfig.name = value;
    else if (key == "tc")
        engineConfig.tc = parseTc(value);
    else if (key == "st")
        engineConfig.tc.fixed_time = std::stod(value) * 1000;
    else if (key == "nodes")
        engineConfig.nodes = std::stoll(value);
    else if (key == "plies")
        engineConfig.plies = std::stoll(value);
    else if (key == "dir")
        engineConfig.dir = value;
    else if (isEngineSettableOption(key)) {
        // Strip option.Name of the option. Part
        const std::size_t pos = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.push_back(std::make_pair(strippedKey, value));
    } else
        coutMissingCommand("engine", key, value);
}

void Options::parseDashOptions(int &i, int argc, char const *argv[],
                               std::function<void(std::string, std::string)> func) {
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        std::string param = argv[i];
        std::size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        func(key, value);
    }
}

void Options::saveJson(const std::map<std::string, std::map<std::string, Stats>> &stats) const {
    nlohmann::ordered_json jsonfile = game_options_;
    jsonfile["engines"] = configs_;
    jsonfile["stats"] = stats;

    std::ofstream file("config.json");
    file << std::setw(4) << jsonfile << std::endl;
}

void Options::loadJson(const std::string &filename) {
    std::cout << "Loading config file: " << filename << std::endl;
    std::ifstream f(filename);
    json jsonfile = json::parse(f);

    game_options_ = jsonfile.get<GameManagerOptions>();
    stats_ = jsonfile["stats"].get<std::map<std::string, std::map<std::string, Stats>>>();

    for (auto engine : jsonfile["engines"]) {
        EngineConfiguration ec = engine.get<EngineConfiguration>();

        configs_.push_back(ec);
    }
}

std::vector<EngineConfiguration> Options::getEngineConfigs() const { return configs_; }

GameManagerOptions Options::getGameOptions() const { return game_options_; }

std::map<std::string, std::map<std::string, Stats>> Options::getStats() const { return stats_; }

bool Options::isEngineSettableOption(const std::string &stringFormat) const {
    return startsWith(stringFormat, "option.");
}

void Options::printVersion(int &i) const {
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

void Options::coutMissingCommand(std::string_view name, std::string_view key,
                                 std::string_view value) const {
    std::cout << "\nUnrecognized " << name << " option: " << key << " with value " << value
              << " parsing failed." << std::endl;
}

bool startsWith(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

bool contains(const std::vector<std::string> &haystack, std::string_view needle) {
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

std::vector<std::string> splitString(const std::string &string, const char &delimiter) {
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter)) seglist.emplace_back(segment);

    return seglist;
}

}  // namespace CMD

}  // namespace fast_chess
