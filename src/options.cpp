#include "options.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>
#include <unordered_map>

#include "third_party/json.hpp"

namespace fast_chess
{

namespace CMD
{

using json = nlohmann::json;

Options::Options(int argc, char const *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        const std::string arg = argv[i];
        if (arg == "-engine")
        {
            configs_.push_back(EngineConfiguration());
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                parseEngineKeyValues(configs_.back(), key, value);
            });
        }
        else if (arg == "-each")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                for (auto &config : configs_)
                    parseEngineKeyValues(config, key, value);
            });
        else if (arg == "-pgnout")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    game_options_.pgn.file = value;
                else if (key == "tracknodes")
                    game_options_.pgn.track_nodes = true;
                else if (key == "notation")
                    game_options_.pgn.notation = value;
                else
                    std::cout << "\nUnrecognized pgn option: " << key << " with value " << value
                              << " parsing failed." << std::endl;
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
                    std::cout << "\nUnrecognized opening option: " << key << " with value " << value
                              << " parsing failed." << std::endl;
            });
        else if (arg == "-sprt")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (game_options_.rounds == 0)
                    game_options_.rounds = 500000;

                if (key == "elo0")
                    game_options_.sprt.elo0 = std::stod(value);
                else if (key == "elo1")
                    game_options_.sprt.elo1 = std::stod(value);
                else if (key == "alpha")
                    game_options_.sprt.alpha = std::stod(value);
                else if (key == "beta")
                    game_options_.sprt.beta = std::stod(value);
                else
                    std::cout << "\nUnrecognized sprt option: " << key << " with value " << value
                              << " parsing failed." << std::endl;
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
                    std::cout << "\nUnrecognized draw option: " << key << " with value " << value
                              << " parsing failed." << std::endl;
            });
        else if (arg == "-resign")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                game_options_.resign.enabled = true;

                if (key == "movecount")
                    game_options_.resign.move_count = std::stoi(value);
                else if (key == "score")
                    game_options_.resign.score = std::stoi(value);
                else
                    std::cout << "\nUnrecognized resign option: " << key << " with value " << value
                              << " parsing failed." << std::endl;
            });
        else if (arg == "-log")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    Logger::openFile(value);
                else
                    std::cout << "\nUnrecognized log option: " << key << " parsing failed."
                              << std::endl;
            });
        else if (arg == "-config")
            parseDashOptions(i, argc, argv, [&](std::string key, std::string value) {
                if (key == "file")
                    loadJson(value);
                else if (key == "discard" && value == "true")
                {
                    std::cout << "Discarded previous results.\n";
                    stats_ = Stats();
                }
                else
                    std::cout << "\nUnrecognized config option: " << key << " parsing failed."
                              << std::endl;
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
            std::cout << "\nDash command: " << arg << " doesnt exist!" << std::endl;
    }

    for (auto &config : configs_)
    {
        if (config.name.empty())
            throw std::runtime_error("Warning: Each engine must have a name!");

        config.recover = game_options_.recover;
    }
}

TimeControl Options::parseTc(const std::string &tcString)
{
    TimeControl tc;

    std::string remainingStringVector = tcString;
    const bool has_moves = contains(tcString, "/");
    const bool has_inc = contains(tcString, "+");

    if (has_moves)
    {
        const auto moves = splitString(tcString, '/');
        tc.moves = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc)
    {
        const auto moves = splitString(remainingStringVector, '+');
        tc.increment = std::stod(moves[1].c_str()) * 1000;
        remainingStringVector = moves[0];
    }

    tc.time = std::stod(remainingStringVector.c_str()) * 1000;

    return tc;
}

void Options::parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                                   const std::string &value)
{
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

    else if (isEngineSettableOption(key))
    {
        // Strip option.Name of the option. Part
        const size_t pos = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.push_back(std::make_pair(strippedKey, value));
    }
    else

        std::cout << "\nUnrecognized engine option: " << key << " parsing failed." << std::endl;
}

void Options::parseDashOptions(int &i, int argc, char const *argv[],
                               std::function<void(std::string, std::string)> func)
{
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++)
    {
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        func(key, value);
    }
}

bool Options::startsWith(std::string_view haystack, std::string_view needle)
{
    if (needle.empty())
        return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

bool Options::contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

bool Options::contains(const std::vector<std::string> &haystack, std::string_view needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

std::vector<std::string> Options::splitString(const std::string &string, const char &delimiter)
{
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter))

        seglist.emplace_back(segment);

    return seglist;
}

void Options::saveJson(const Stats &stats) const
{
    nlohmann::ordered_json jsonfile;

    jsonfile["event"] = game_options_.event_name;

    jsonfile["rounds"] = game_options_.rounds;

    jsonfile["games"] = game_options_.games;

    jsonfile["recover"] = game_options_.recover;

    jsonfile["concurrency"] = game_options_.concurrency;

    jsonfile["rating-interval"] = game_options_.ratinginterval;

    nlohmann::ordered_json objects = nlohmann::ordered_json::array();

    for (const auto &engine : configs_)
    {
        nlohmann::ordered_json object;
        object["name"] = engine.name;
        object["dir"] = engine.dir;
        object["cmd"] = engine.cmd;
        // jsonfile["args"] = engine.args;

        for (const auto &option : engine.options)
        {
            object["options"]["name"] = option.first;
            object["options"]["value"] = option.second;
        }

        object["tc"]["time"] = engine.tc.time;
        object["tc"]["fixed_time"] = engine.tc.fixed_time;
        object["tc"]["increment"] = engine.tc.increment;
        object["tc"]["moves"] = engine.tc.moves;

        object["nodes"] = engine.nodes;
        object["plies"] = engine.plies;
        object["recover"] = engine.recover;

        objects.push_back(object);
    }

    jsonfile["engines"] = objects;

    jsonfile["opening"]["file"] = game_options_.opening.file;
    jsonfile["opening"]["format"] = game_options_.opening.format;
    jsonfile["opening"]["order"] = game_options_.opening.order;
    jsonfile["opening"]["start"] = game_options_.opening.start;

    jsonfile["adjudication"]["resign"]["enabled"] = game_options_.resign.enabled;
    jsonfile["adjudication"]["resign"]["movecount"] = game_options_.resign.move_count;
    jsonfile["adjudication"]["resign"]["score"] = game_options_.resign.score;

    jsonfile["adjudication"]["draw"]["enabled"] = game_options_.draw.enabled;
    jsonfile["adjudication"]["draw"]["movecount"] = game_options_.draw.move_count;
    jsonfile["adjudication"]["draw"]["movenumber"] = game_options_.draw.move_number;
    jsonfile["adjudication"]["draw"]["score"] = game_options_.draw.score;

    jsonfile["sprt"]["elo0"] = game_options_.sprt.elo0;
    jsonfile["sprt"]["elo1"] = game_options_.sprt.elo1;
    jsonfile["sprt"]["alpha"] = game_options_.sprt.alpha;
    jsonfile["sprt"]["beta"] = game_options_.sprt.beta;

    jsonfile["stats"]["wins"] = stats.wins;
    jsonfile["stats"]["draws"] = stats.draws;
    jsonfile["stats"]["losses"] = stats.losses;
    jsonfile["stats"]["pentaWW"] = stats.penta_WW;
    jsonfile["stats"]["pentaWD"] = stats.penta_WD;
    jsonfile["stats"]["pentaWL"] = stats.penta_WL;
    jsonfile["stats"]["pentaLD"] = stats.penta_LD;
    jsonfile["stats"]["pentaLL"] = stats.penta_LL;
    jsonfile["stats"]["roundcount"] = stats.round_count;
    jsonfile["stats"]["totalcount"] = stats.total_count;
    jsonfile["stats"]["timeouts"] = stats.timeouts;

    std::ofstream file("config.json");
    file << std::setw(4) << jsonfile << std::endl;
}

void Options::loadJson(const std::string &filename)
{
    std::cout << "Loading config file: " << filename << std::endl;
    std::ifstream f(filename);
    json jsonfile = json::parse(f);

    game_options_.event_name = jsonfile["event"];
    game_options_.rounds = jsonfile["rounds"];
    game_options_.games = jsonfile["games"];
    game_options_.recover = jsonfile["recover"];
    game_options_.concurrency = jsonfile["concurrency"];
    game_options_.ratinginterval = jsonfile["rating-interval"];

    configs_.clear();

    for (auto engine : jsonfile["engines"])
    {
        EngineConfiguration ec;

        ec.name = engine["name"];
        ec.dir = engine["dir"];
        ec.cmd = engine["cmd"];

        ec.tc.time = engine["tc"]["time"];
        ec.tc.fixed_time = engine["tc"]["fixed_time"];
        ec.tc.increment = engine["tc"]["increment"];
        ec.tc.moves = engine["tc"]["moves"];

        ec.nodes = engine["nodes"];
        ec.plies = engine["plies"];
        ec.recover = engine["recover"];

        configs_.push_back(ec);
    }

    game_options_.opening.file = jsonfile["opening"]["file"];
    game_options_.opening.format = jsonfile["opening"]["format"];
    game_options_.opening.order = jsonfile["opening"]["order"];
    game_options_.opening.start = jsonfile["opening"]["start"];

    game_options_.resign.enabled = jsonfile["adjudication"]["resign"]["enabled"];
    game_options_.resign.move_count = jsonfile["adjudication"]["resign"]["movecount"];
    game_options_.resign.score = jsonfile["adjudication"]["resign"]["score"];

    game_options_.draw.enabled = jsonfile["adjudication"]["draw"]["enabled"];
    game_options_.draw.move_count = jsonfile["adjudication"]["draw"]["movecount"];
    game_options_.draw.move_number = jsonfile["adjudication"]["draw"]["movenumber"];
    game_options_.draw.score = jsonfile["adjudication"]["draw"]["score"];

    game_options_.sprt.elo0 = jsonfile["sprt"]["elo0"];
    game_options_.sprt.elo1 = jsonfile["sprt"]["elo1"];
    game_options_.sprt.alpha = jsonfile["sprt"]["alpha"];
    game_options_.sprt.beta = jsonfile["sprt"]["beta"];

    stats_ = Stats(
        jsonfile["stats"]["wins"], jsonfile["stats"]["draws"], jsonfile["stats"]["losses"],
        jsonfile["stats"]["pentaWW"], jsonfile["stats"]["pentaWD"], jsonfile["stats"]["pentaWL"],
        jsonfile["stats"]["pentaLD"], jsonfile["stats"]["pentaLL"], jsonfile["stats"]["roundcount"],
        jsonfile["stats"]["totalcount"], jsonfile["stats"]["timeouts"]);
}

std::vector<EngineConfiguration> Options::getEngineConfigs() const
{
    return configs_;
}

GameManagerOptions Options::getGameOptions() const
{
    return game_options_;
}

Stats Options::getStats() const
{
    return stats_;
}

bool Options::isEngineSettableOption(const std::string &stringFormat) const
{
    return startsWith(stringFormat, "option.");
}

void Options::printVersion(int &i)
{
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
    std::stringstream ss, date(__DATE__); // {month} {date} {year}

    ss << "fast-chess ";
#ifdef GIT_DATE
    ss << GIT_DATE;
#else

    date >> month >> day >> year;
    if (day.length() == 1)
        day = "0" + day;
    ss << year.substr(2) << months[month] << day;
#endif

#ifdef GIT_SHA
    ss << "-" << GIT_SHA;
#endif

    ss << "\n";

    std::cout << ss.str();
    exit(0);
}

} // namespace CMD

} // namespace fast_chess
