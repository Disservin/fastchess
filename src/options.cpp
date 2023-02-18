#include "options.h"
#include "helper.h"
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