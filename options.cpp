#include "options.h"

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
    std::string current_exec_name = argv[0]; // Name of the current exec program

    if (argc > 1)
    {
        user_input.assign(argv + 1, argv + argc);
    }
}

std::vector<std::string> Options::getUserInput()
{
    return user_input;
}

Options::~Options()
{
}

} // namespace CMD