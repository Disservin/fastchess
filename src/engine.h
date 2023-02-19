#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct TimeControl
{
  uint64_t moves;
  uint64_t time;
  uint64_t increment;
};

class Engine
{
private:
  // engine name
  std::string name;

  // Path to engine
  std::string cmd;

  // Custom args that should be sent
  std::string args;

  // UCI options
  std::vector<std::pair<std::string, std::string>> options;

  // time control for the engine
  TimeControl tc;

public:
  void setName(const std::string &name);
  void setCmd(const std::string &cmd);
  void setArgs(const std::string &args);
  void setOptions(const std::vector<std::pair<std::string, std::string>> &options);
  void setTc(const TimeControl &tc);

  std::string getName() const;
  std::string getCmd() const;
  std::string getArgs() const;
  std::vector<std::pair<std::string, std::string>> getOptions() const;
  TimeControl getTc() const;
};
