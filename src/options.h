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
    // Holds all the relevant settings for the handling of the games
    gameManagerOptions game_options;
    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    Options(int argc, char const *argv[]);

    ~Options();

    bool isEngineSettableOption(std::string string_format);

    TimeControl parseTc(const std::string tc_string);

    void parseConcurrency(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parseEvent(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parseGames(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parseRounds(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parseOpeningOptions(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parsePgnOptions(int &i, int argc, char const *argv[], gameManagerOptions &cli_options);

    void parseEngineParams(int &i, int argc, char const *argv[], EngineConfiguration &engine_params);

    EngineConfiguration getEngineConfig(int engine_index);

    void fill_parameters();
};

} // namespace CMD