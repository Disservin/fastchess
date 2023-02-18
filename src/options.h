#pragma once

#include <iostream>
#include <string>
#include <vector>

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
  struct gameManagerOptions
  {
    int games = 1;
    int rounds = 1;
    bool recover = false;
    bool repeat = false;
    int concurrency = 1;
    std::string event_name;
  };
  inline std::ostream &operator<<(std::ostream &os, const Parameter param)
  {
    os << "long_name" << param.long_name << "short_name" << param.short_name << "default" << param.default_value
       << "min" << param.min_limit << "max" << param.max_limit;

    return os;
  }

  class Options
  {
  private:
    std::vector<Parameter> parameters;
    std::vector<std::string> user_input;

    std::vector<std::string> cli_options;
    std::vector<std::vector<std::string>> engines_options;
    // Holds all the relevant settings for the handling of the games
    gameManagerOptions game_options;

  public:
    Options(int argc, char const *argv[]);
    ~Options();

    std::vector<std::string> getUserInput();
    void parseUserInput(int argc, char const *argv[]);
    void split_params();
    std::vector<std::vector<std::string>> group_cli_params();
    void fill_parameters();
    void print_params();
  };

} // namespace CMD