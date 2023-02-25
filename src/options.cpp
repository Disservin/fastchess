#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>

#include "engine.h"
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
        else if (arg == "-each")
            parseEachOptions(i, argc, argv);
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
        else if (arg == "-draw")
            parseDrawOptions(i, argc, argv);
        else if (arg == "-resign")
            parseDrawOptions(i, argc, argv);
    }

    for (const auto &config : configs)
    {
        if (config.name == "")
        {
            throw std::runtime_error("Each engine must have a name!");
        }
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

void Options::parseEachOptions(int &i, int argc, char const *argv[])
{
    i++;
    while (i < argc && argv[i][0] != '-')
    {
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        for (auto &config : configs)
        {
            if (key == "cmd")
            {
                config.cmd = value;
            }
            else if (key == "name")
            {
                config.name = value;
            }
            else if (key == "tc")
            {
                config.tc = parseTc(value);
            }
            else if (key == "nodes")
            {
                config.nodes = std::stoll(value);
            }
            else if (key == "plies")
            {
                config.plies = std::stoll(value);
            }
            else if (key == "dir")
            {
                config.dir = value;
            }
            else if (isEngineSettableOption(key))
            {
                config.options.push_back(std::make_pair(key, value));
            }
            else
            {
                std::cout << "\n unrecognized engine option:" << key << " parsing failed" << std::endl;
            }
        }

        i++;
    }
    i--;
}

void Options::parseDrawOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        gameOptions.draw.enabled = true;
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);
        if (key == "movenumber")
        {
            gameOptions.draw.moveNumber = std::stoi(value);
        }
        else if (key == "movecount")
        {
            gameOptions.draw.moveCount = std::stoi(value);
        }
        else if (key == "score")
        {
            gameOptions.draw.score = std::stoi(value);
        }

        i++;
    }

    i--;
};

void Options::parseResignOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        gameOptions.resign.enabled = true;
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        if (key == "movecount")
        {
            gameOptions.resign.moveCount = std::stoi(value);
        }
        else if (key == "score")
        {
            gameOptions.resign.score = std::stoi(value);
        }

        i++;
    }

    i--;
};

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
        else if (key == "start")
        {
            gameOptions.opening.start = std::stoi(value);
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

bool Options::startsWith(std::string_view haystack, std::string_view needle)
{
    if (needle == "")
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
    {
        seglist.emplace_back(segment);
    }

    return seglist;
}

} // namespace CMD