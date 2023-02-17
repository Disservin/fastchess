#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace CMD
{

struct parameter
{
    std::string long_name;
    std::string short_name;
    std::string default_value;
    std::string min_limit;
    std::string max_limit;
};

inline std::ostream &operator<<(std::ostream &os, const parameter param)
{
    os << "long_name" << param.long_name << "short_name" << param.short_name << "default" << param.default_value
       << "min" << param.min_limit << "max" << param.max_limit;

    return os;
}

class Options
{
  private:
    std::vector<parameter> parameters;
    std::vector<std::string> user_input;

    std::vector<std::string> cli_options;
    std::vector<std::vector<std::string>> engines_options;

  public:
    Options(int argc, char const *argv[]);
    ~Options();

    std::vector<std::string> getUserInput();
    void parseUserInput(int argc, char const *argv[]);
    void fill_parameters();
    void print_params();
};

} // namespace CMD