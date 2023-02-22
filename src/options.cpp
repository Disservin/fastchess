#include <limits>
#include <map>
#include <sstream>
#include <type_traits>

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
            parseOption(i, argc, argv, gameOptions.concurrency);
        else if (arg == "-event")
            parseOption(i, argc, argv, gameOptions.eventName);
        else if (arg == "-games")
            parseOption(i, argc, argv, gameOptions.games);
        else if (arg == "-rounds")
            parseOption(i, argc, argv, gameOptions.rounds);
        else if (arg == "-pgnout")
            parseOption(i, argc, argv, gameOptions.pgn.file);
        else if (arg == "-openings")
            parseOpeningOptions(i, argc, argv);
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

void Options::parseEngineParams(int &i, int argc, char const *argv[], EngineConfiguration &engineParams)
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
            engineParams.cmd = value;
        }
        else if (key == "name")
        {
            engineParams.name = value;
        }
        else if (key == "tc")
        {
            engineParams.tc = parseTc(value);
        }
        else if (key == "nodes")
        {
            engineParams.nodes = std::stoll(value);
        }
        else if (key == "plies")
        {
            engineParams.plies = std::stoll(value);
        }
        else if (key == "dir")
        {
            engineParams.dir = value;
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
    engineParams.options = engine_settable_options;
    i--;
}

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

template <typename T> void Options::parseOption(int &i, int argc, const char *argv[], T &optionValue)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        if constexpr (std::is_same_v<T, int>)
            optionValue = std::stoi(argv[i]);
        else if constexpr (std::is_same_v<T, float>)
            optionValue = std::stof(argv[i]);
        else if constexpr (std::is_same_v<T, double>)
            optionValue = std::stod(argv[i]);
        else
            optionValue = argv[i];
    }
}

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

std::vector<EngineConfiguration> Options::getEngineConfigs() const
{
    return configs;
};

GameManagerOptions Options::getGameOptions() const
{
    return gameOptions;
}

} // namespace CMD