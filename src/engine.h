#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engineprocess.h"

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

    // Process wrapper around the engines process
    EngineProcess process;

  public:
    Engine() = default;

    Engine(std::string command);

    ~Engine();

    void setName(const std::string &name);

    void setArgs(const std::string &args);

    void setOptions(const std::vector<std::pair<std::string, std::string>> &options);

    void setTc(const TimeControl &tc);

    void setCmd(const std::string &command);

    void stopProcess();

    bool pingProcess();

    void writeProcess(const std::string &input);

    std::vector<std::string> readProcess(const std::string &last_word, int64_t timeoutThreshold = 1000);

    std::vector<std::string> readProcess(const std::string &last_word, bool &timedOut, int64_t timeoutThreshold = 1000);

    std::string getName() const;

    std::string getCmd() const;

    std::string getArgs() const;

    std::vector<std::pair<std::string, std::string>> getOptions() const;

    TimeControl getTc() const;
};
