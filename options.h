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

  public:
    Options(int argc, char const *argv[]);
    ~Options();

    std::vector<std::string> getUserInput();

    void fill_parameters();
};

} // namespace CMD