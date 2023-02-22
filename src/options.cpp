#include <limits>
#include <map>
#include <sstream>

#include "engine.h"
#include "helper.h"
#include "options.h"

namespace CMD
{

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
            parseConcurrency(i, argc, argv, gameOptions);
        else if (arg == "-event")
            parseEvent(i, argc, argv, gameOptions);
        else if (arg == "-games")
            parseGames(i, argc, argv, gameOptions);
        else if (arg == "-rounds")
            parseRounds(i, argc, argv, gameOptions);
        else if (arg == "-openings")
            parseOpeningOptions(i, argc, argv, gameOptions);
        else if (arg == "-pgnout")
            parsePgnOptions(i, argc, argv, gameOptions);
        else if (arg == "-recover")
            gameOptions.recover = true;
        else if (arg == "-repeat")
            gameOptions.repeat = true;
    }
}

bool Options::isEngineSettableOption(std::string stringFormat) const
{
    if (startsWith(stringFormat, "option."))
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
TimeControl Options::parseTc(const std::string tcString)
{
    // Create time control object and parse the strings into usable values
    TimeControl time_control;
    bool has_moves = contains(tcString, "/");
    bool has_inc = contains(tcString, "+");
    if (has_moves && has_inc)
    {
        // Split the string into move count and time+inc
        std::vector<std::string> moves_and_time;
        moves_and_time = splitString(tcString, '/');
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
        moves_and_time = splitString(tcString, '/');
        std::string moves = moves_and_time.front();
        std::string time = moves_and_time.back();
        time_control.moves = std::stoi(moves);
        time_control.time = std::stof(time) * 1000;
        time_control.increment = 0;
    }
    else if (has_inc)
    {
        std::vector<std::string> time_and_inc = splitString(tcString, '+');
        std::string time = time_and_inc.front();
        std::string inc = time_and_inc.back();
        time_control.moves = 0;
        time_control.time = std::stof(time) * 1000;
        time_control.increment = std::stof(inc) * 1000;
    }
    else
    {
        time_control.moves = 0;
        time_control.time = std::stof(tcString) * 1000;
        time_control.increment = 0;
    }

    return time_control;
};

// Takes a string in input and returns a TimeControl object
void Options::parseConcurrency(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        std::string thread_num = argv[i];
        cliOptions.concurrency = std::stoi(thread_num);
    }
    return;
};

void Options::parseEvent(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        std::string eventName = argv[i];
        cliOptions.eventName = eventName;
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseGames(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        std::string games_num = argv[i];
        cliOptions.games = std::stoi(games_num);
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseRounds(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{

    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseOpeningOptions(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{
    OpeningOptions opop;

    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parsePgnOptions(int &i, int argc, char const *argv[], GameManagerOptions &cliOptions)
{
    PgnOptions PgnOptions;

    return;
};

std::vector<EngineConfiguration> Options::getEngineConfig() const
{
    return configs;
};

GameManagerOptions Options::getGameOptions() const
{
    return gameOptions;
}

Options::~Options()
{
}

} // namespace CMD