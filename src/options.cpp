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
    // Parse the user command into a series of strings
    parseUserInput(argc, argv);
    // Code to Split UserInput into Cli/Engine1/Engine2/EngineN for easier parsing
    split_params();
    // Parse engine options
    parseEnginesOptions();
    // Parse cli options
    parseCliOptions();
}

std::vector<std::string> Options::getUserInput()
{
    return user_input;
}

void Options::parseUserInput(int argc, char const *argv[])
{

    if (argc > 1)
    {
        user_input.assign(argv + 1, argv + argc);
    }
}

void Options::split_params()
{
    bool meant_for_engine = false;
    // Read option token
    for (const auto &item : getUserInput())
    {
        // it's if a dash command check if we are moving from cli arguments to engine arguments or viceversa
        if (starts_with(item, "-"))
        {
            // If it's not -engine then the subsequent stuff is meant for the cli
            if (!(item.compare("-engine") == 0))
            {
                // Set flag for any subsequent token meant for an engine
                meant_for_engine = false;
                cli_options.push_back(item);
            }
            else
            {
                std::vector<std::string> new_engine_options;
                engines_options.push_back(new_engine_options);
                meant_for_engine = true;
            }
        }
        // If it's not a "Dash" command we are parsing an argument for either the engine or the cli;
        else
        {
            if (!meant_for_engine)
            {
                // Add option to list of cli options
                cli_options.push_back(item);
            }
            else
            {
                engines_options.back().push_back(item);
            }
        }
    }
}

// groups functionally linked subset of cli parameters
std::vector<std::vector<std::string>> Options::group_cli_params()
{
    std::vector<std::vector<std::string>> option_groups;

    for (const auto &cli_option : cli_options)
    {
        // New functionally independent group
        if (starts_with(cli_option, "-"))
        {
            // Create new option group
            std::vector<std::string> new_option_group;
            new_option_group.push_back(cli_option);
            option_groups.push_back(new_option_group);
        }
        else
        {
            // Add to the current option group
            option_groups.back().push_back(cli_option);
        }
    }

    return option_groups;
}

bool Options::isEngineSettableOption(std::string string_format)
{
    if (starts_with(string_format, "option."))
        return true;
    return false;
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
int Options::parseConcurrency(const std::vector<std::string> concurrency_string)
{
    assert(concurrency_string.size() == 2);
    int concurrency = std::stoi(concurrency_string.at(1));
    return concurrency;
};

std::string Options::parseEvent(const std::vector<std::string> event_string)
{
    assert(event_string.size() == 2);
    return event_string.at(1);
};

// Takes a string in input and returns a TimeControl object
int Options::parseGames(const std::vector<std::string> games_string)
{

    assert(games_string.size() == 2);
    return std::stoi(games_string.at(1));
};

// Takes a string in input and returns a TimeControl object
int Options::parseRounds(const std::vector<std::string> concurrency_string)
{

    return 0;
};

// Takes a string in input and returns a TimeControl object
openingOptions Options::parseOpeningOptions(const std::vector<std::string> concurrency_string)
{
    openingOptions opop;

    return opop;
};

// Takes a string in input and returns a TimeControl object
pgnOptions Options::parsePgnOptions(const std::vector<std::string> concurrency_string)
{
    pgnOptions pgn_options;

    return pgn_options;
};

void Options::parseEnginesOptions()
{
    for (const auto &engine_options : engines_options)
    {
        // Create engine object
        EngineConfiguration config;
        // Create Args vector
        std::vector<std::pair<std::string, std::string>> engine_settable_options;
        for (const auto &option : engine_options)
        {
            // get key and value pair
            std::vector<std::string> name_value_couple = splitString(option, '=');
            std::string param_name = name_value_couple.front();
            std::string param_value = name_value_couple.back();
            // Assign the value
            if (param_name == "cmd")
            {
                config.cmd = param_value;
            }
            else if (param_name == "name")
            {
                config.name = param_value;
            }
            else if (param_name == "tc")
            {
                config.tc = parseTc(param_value);
            }
            else if (param_name == "nodes")
            {
                config.nodes = std::stoll(param_value);
            }
            else if (param_name == "plies")
            {
                config.plies = std::stoll(param_value);
            }
            else if (param_name == "dir")
            {
                config.dir = param_value;
            }
            else if (isEngineSettableOption(param_name))
            {
                engine_settable_options.push_back(std::make_pair(param_name, param_value));
            }
            else
            {
                std::cout << "\n unrecognized engine option:" << option << " parsing failed\n";
                return;
            }
        }
        config.options = engine_settable_options;
        // Add engine
        configs.push_back(config);
    }
}

void Options::parseCliOptions()
{
    // Parse Cli options//
    // group those into functionality related subgroups
    std::vector<std::vector<std::string>> cli_parameters_groups = group_cli_params();

    for (auto parameter_group : cli_parameters_groups)
    {
        // The first element of the group is our key
        std::string key = parameter_group.front();
        if (key == "-concurrency")
            game_options.concurrency = parseConcurrency(parameter_group);
        else if (key == "-event")
            game_options.event_name = parseEvent(parameter_group);
        else if (key == "-games")
            game_options.games = parseGames(parameter_group);
        else if (key == "-rounds")
            game_options.rounds = parseRounds(parameter_group);
        else if (key == "-openings")
            game_options.opening_options = parseOpeningOptions(parameter_group);
        else if (key == "-pgnout")
            game_options.pgn_options = parsePgnOptions(parameter_group);
        else if (key == "-recover")
            game_options.recover = true;
        else if (key == "-repeat")
            game_options.repeat = true;
    }
}

EngineConfiguration Options::getEngineConfig(int engine_index)
{
    assert(engine_index >= 0);
    assert(engine_index <= configs.size() - 1);
    return configs.at(engine_index);
};

void Options::print_params()
{
    std::cout << "Printing cli options" << std::endl;
    for (const auto &cli_option : cli_options)
        std::cout << cli_option << std::endl;
    std::cout << "Printing Engines options" << std::endl;
    for (const auto &engine_options : engines_options)
    {
        std::cout << "New Engine" << std::endl;
        for (const auto &option : engine_options)
            std::cout << option << std::endl;
    }
};

Options::~Options()
{
}

} // namespace CMD