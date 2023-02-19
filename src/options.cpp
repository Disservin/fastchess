#include "options.h"
#include "helper.h"
#include "engine.h"
#include <map>
#include <sstream>
#include <limits>

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
        for (auto engine_options : engines_options)
        {
            // Create engine object
            Engine engine;
            // Create Args vector
            std::vector<std::pair<std::string, std::string>> engine_settable_options;
            for (auto option : engine_options)
            {
                // get key and value pair
                std::vector<std::string> name_value_couple = splitString(option, '=');
                std::string param_name = name_value_couple.front();
                std::string param_value = name_value_couple.back();
                // Assign the value
                if (param_name == "cmd")
                {
                    engine.setCmd(param_value);
                }
                if (param_name == "name")
                {
                    engine.setName(param_value);
                }
                if (param_name == "tc")
                {
                    engine.setTc(ParseTc(param_value));
                }
                if (isEngineSettableOption(param_name))
                {
                    engine_settable_options.push_back(std::make_pair(param_name, param_value));
                }
            }
            engine.setOptions(engine_settable_options);
            // Add engine
            engines.push_back(engine);
        }
        // Parse Cli options//
        // group those into functionality related subgroups
        std::vector<std::vector<std::string>> cli_parameters_groups = group_cli_params();
        // For each parameter group we use the first element as a key of a map, the rest as part of the value
        std::map<std::string, std::vector<std::string>> map;
        for (auto parameter_group : cli_parameters_groups)
        {
            // The first element of the group is our key
            std::string key = parameter_group.front();
            // Drop the  first element from the vector and use that as a value(this is much easier than copying a subset of it)
            parameter_group.erase(parameter_group.begin());
            map[key] = parameter_group;
        }
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
        for (auto item : getUserInput())
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
        for (auto cli_option : cli_options)
        {
            // New functionally independent group
            if (starts_with(cli_option, "-"))
            {
                // Create new option group
                std::vector<std::string> new_option_group;
                option_groups.push_back(new_option_group);
                // add new option to group
                engines_options.back().push_back(cli_option);
            }
            else
            {
                // Add to the current option group
                engines_options.back().push_back(cli_option);
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
    TimeControl ParseTc(const std::string tc_string)
    {
        // Split the string into move count and time+inc
        std::vector<std::string> moves_and_time;
        moves_and_time = splitString(tc_string, '/');
        std::string moves = moves_and_time.front();
        // Split time+inc into time and inc
        std::vector<std::string> time_and_inc = splitString(moves_and_time.back(), '+');
        std::string time = time_and_inc.front();
        std::string inc = time_and_inc.back();
        // Create time control object and parse the strings into usable values
        TimeControl time_control;
        time_control.moves = std::stoi(moves);
        time_control.time = std::stol(time) * 1000;
        time_control.increment = std::stol(inc) * 1000;
        return time_control;
    };
    void Options::print_params()
    {
        std::cout << "Printing cli options" << std::endl;
        for (auto cli_option : cli_options)
            std::cout << cli_option << std::endl;
        std::cout << "Printing Engines options" << std::endl;
        for (auto engine_options : engines_options)
        {
            std::cout << "New Engine" << std::endl;
            for (auto option : engine_options)
                std::cout << option << std::endl;
        }
    };

    Options::~Options()
    {
    }

} // namespace CMD