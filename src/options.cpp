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
            parseConcurrency(i, argc, argv);
        else if (arg == "-event")
            parseEvent(i, argc, argv);
        else if (arg == "-games")
            parseGames(i, argc, argv);
        else if (arg == "-rounds")
            parseRounds(i, argc, argv);
        else if (arg == "-openings")
            parseOpeningOptions(i, argc, argv);
        else if (arg == "-pgnout")
            parsePgnOptions(i, argc, argv);
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
    TimeControl tc;

    std::string remainingStringVector = tcString;
    bool has_moves = contains(tcString, "/");
    bool has_inc = contains(tcString, "+");

    if (has_moves)
    {
        auto moves = splitString(tcString, '/');
        tc.moves = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc)
    {
        auto moves = splitString(remainingStringVector, '+');
        tc.increment = std::stof(moves[1].c_str()) * 1000;
        remainingStringVector = moves[0];
    }

    tc.time = std::stof(remainingStringVector.c_str()) * 1000;

    return tc;
};

// Takes a string in input and returns a TimeControl object
void Options::parseConcurrency(int &i, int argc, char const *argv[])
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        gameOptions.concurrency = std::stoi(argv[i]);
    }
    return;
};

void Options::parseEvent(int &i, int argc, char const *argv[])
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        gameOptions.eventName = argv[i];
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseGames(int &i, int argc, char const *argv[])
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        gameOptions.games = std::stoi(argv[i]);
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseRounds(int &i, int argc, char const *argv[])
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        gameOptions.rounds = std::stoi(argv[i]);
    }
    return;
};

// Takes a string in input and returns a TimeControl object
void Options::parseOpeningOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        if (key == "file")
        {
            gameOptions.opening.file = value;
        }
        else if (key == "format")
        {
            gameOptions.opening.format = value;
        }
        else if (key == "order")
        {
            gameOptions.opening.order = value;
        }
        else if (key == "plies")
        {
            gameOptions.opening.plies = std::stoi(value);
        }
        else
        {
            std::cout << "\n unrecognized engine option:" << key << " with value " << value << " parsing failed\n";
            return;
        }
        i++;
    }
    i--;
};

// Takes a string in input and returns a TimeControl object
void Options::parsePgnOptions(int &i, int argc, char const *argv[])
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        gameOptions.pgn.file = argv[i];
    }
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

} // namespace CMD