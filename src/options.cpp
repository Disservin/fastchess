#include "options.h"
#include "engine.h"
#include "helper.h"
#include <limits>
#include <map>
#include <sstream>

namespace CMD
{

void Options::fill_parameters()
{
    parameters.push_back({"--engine", "-e", "", "", ""});
    parameters.push_back({"--concurrency", "-c", "1", "1", "512"});
    parameters.push_back({"--openings", "-o", "", "", ""});
    parameters.push_back({"--debug", "-d", "false", "false", "true"});
    parameters.push_back({"--games", "-g", "1", "1", std::to_string(std::numeric_limits<int>::max())});
}

Options::Options(int argc, char const *argv[])
{
    // Name of the current exec program
    std::string current_exec_name = argv[0];
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-engine")
        {
            configs.push_back(EngineConfiguration());
            parseEngineParams(i, argc, argv, configs.back());
        }
        else if (arg == "-concurrency")
            parseConcurrency(i, argc, argv, game_options);
        else if (arg == "-event")
            parseEvent(i, argc, argv, game_options);
        else if (arg == "-games")
            parseGames(i, argc, argv, game_options);
        else if (arg == "-rounds")
            parseRounds(i, argc, argv, game_options);
        else if (arg == "-openings")
            parseOpeningOptions(i, argc, argv, game_options);
        else if (arg == "-pgnout")
            parsePgnOptions(i, argc, argv, game_options);
        else if (arg == "-recover")
            game_options.recover = true;
        else if (arg == "-repeat")
            game_options.repeat = true;
    }
}

bool Options::isEngineSettableOption(std::string string_format)
{
    if (starts_with(string_format, "option."))
        return true;
    return false;
}

void Options::parseEngineParams(int &i, int argc, char const *argv[], EngineConfiguration &engine_params)
{
    i++;
    std::vector<std::pair<std::string, std::string>> engine_settable_options;
    while (i < argc && argv[i][0] != '-')
    {
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);
        if (key == "cmd")
        {
            engine_params.cmd = value;
        }
        else if (key == "name")
        {
            engine_params.name = value;
        }
        else if (key == "tc")
        {
            engine_params.tc = parseTc(value);
        }
        else if (key == "nodes")
        {
            engine_params.nodes = std::stoll(value);
        }
        else if (key == "plies")
        {
            engine_params.plies = std::stoll(value);
        }
        else if (key == "dir")
        {
            engine_params.dir = value;
        }
        else if (isEngineSettableOption(key))
        {
            engine_settable_options.push_back(std::make_pair(key, value));
        }
        else
        {
            std::cout << "\n unrecognized engine option:" << key << " parsing failed\n";
            return;
        }
        i++;
    }
    engine_params.options = engine_settable_options;
    i--;
}

// Takes a string in input and returns a TimeControl object
TimeControl Options::parseTc(const std::string tc_string)
{
    // Create time control object and parse the strings into usable values
    TimeControl time_control;
    bool has_moves = contains(tc_string, "/");
    bool has_inc = contains(tc_string, "+");
    if (has_moves && has_inc)
    {
        // Split the string into move count and time+inc
        std::vector<std::string> moves_and_time;
        moves_and_time = splitString(tc_string, '/');
        std::string moves = moves_and_time.front();
        // Split time+inc into time and inc
        std::vector<std::string> time_and_inc = splitString(moves_and_time.back(), '+');
        std::string time = time_and_inc.front();
        std::string inc = time_and_inc.back();
        time_control.moves = std::stoi(moves);
        time_control.time = std::stof(time) * 1000;
        time_control.increment = std::stof(inc) * 1000;
    }
    else if (has_moves)
    {
        // Split the string into move count and time+inc
        std::vector<std::string> moves_and_time;
        moves_and_time = splitString(tc_string, '/');
        std::string moves = moves_and_time.front();
        std::string time = moves_and_time.back();
        time_control.moves = std::stoi(moves);
        time_control.time = std::stof(time) * 1000;
        time_control.increment = 0;
    }
    else if (has_inc)
    {
        std::vector<std::string> time_and_inc = splitString(tc_string, '+');
        std::string time = time_and_inc.front();
        std::string inc = time_and_inc.back();
        time_control.moves = 0;
        time_control.time = std::stof(time) * 1000;
        time_control.increment = std::stof(inc) * 1000;
    }
    else
    {
        time_control.moves = 0;
        time_control.time = std::stof(tc_string) * 1000;
        time_control.increment = 0;
    }

    return time_control;
};

// Takes a string in input and returns a TimeControl object
void Options::parseConcurrency(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {4
        std::string thread_num = argv[i];
        cli_options.concurrency = std::stoi(thread_num);

    }
    return;
};

void Options::parseEvent(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        std::string event_name = argv[i];
        cli_options.event_name = event_name;
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseGames(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        std::string games_num = argv[i];
        cli_options.games = std::stoi(games_num);
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseRounds(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{

    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseOpeningOptions(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{
    openingOptions opop;

    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parsePgnOptions(int &i, int argc, char const *argv[], gameManagerOptions &cli_options)
{
    pgnOptions pgn_options;

    return;
};

EngineConfiguration Options::getEngineConfig(int engine_index)
{
    assert(engine_index >= 0);
    assert(engine_index <= configs.size() - 1);
    return configs.at(engine_index);
};

Options::~Options()
{
}

} // namespace CMD