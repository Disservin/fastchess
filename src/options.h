#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "engine_config.h"
namespace CMD
{

struct Parameter
{
    std::string long_name;
    std::string short_name;
    std::string default_value;
    std::string min_limit;
    std::string max_limit;
};
struct openingOptions
{
    std::string file;
    // TODO use enums for this
    std::string format;
    std::string order;
    int plies;
};
struct pgnOptions
{
    std::string file;
    // TODO use enums for this
    bool min = false;
    bool fi = false;
};
struct gameManagerOptions
{
    int games = 1;
    int rounds = 1;
    bool recover = false;
    bool repeat = false;
    int concurrency = 1;
    std::string event_name;
    openingOptions opening_options;
    pgnOptions pgn_options;
};

inline std::ostream &operator<<(std::ostream &os, const Parameter param)
{
    os << "long_name" << param.long_name << "short_name" << param.short_name << "default" << param.default_value
       << "min" << param.min_limit << "max" << param.max_limit;

    return os;
}

class Options
{
  public:
    std::vector<Parameter> parameters;
    std::vector<std::string> user_input;
    std::vector<std::string> cli_options;
    std::vector<std::vector<std::string>> engines_options;
    // Holds all the relevant settings for the handling of the games
    gameManagerOptions game_options;
    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    Options(int argc, char const *argv[]);

    ~Options();

    std::vector<std::string> getUserInput();

    void parseUserInput(int argc, char const *argv[]);

    void split_params();

    std::vector<std::vector<std::string>> group_cli_params();

    bool isEngineSettableOption(std::string string_format);

    TimeControl parseTc(const std::string tc_string);

    int parseConcurrency(const std::vector<std::string> concurrency_string);

    std::string parseEvent(const std::vector<std::string> event_string);

    int parseGames(const std::vector<std::string> games_string);

    int parseRounds(const std::vector<std::string> concurrency_string);

    openingOptions parseOpeningOptions(const std::vector<std::string> concurrency_string);

    pgnOptions parsePgnOptions(const std::vector<std::string> concurrency_string);

    void parseEnginesOptions();

    void parseCliOptions();

    void fill_parameters();

    void print_params();
};

} // namespace CMD