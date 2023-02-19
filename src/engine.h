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
    // Node limit for the engine
    uint64_t nodes;
    // Ply limit for the engine
    uint64_t plies;
    // Process wrapper around the engine
    EngineProcess process;

  public:
    Engine() = default;

    Engine(std::string command);

    ~Engine();

    Engine setName(const std::string &name);

    Engine setArgs(const std::string &args);

    Engine setOptions(const std::vector<std::pair<std::string, std::string>> &options);

    Engine setTc(const TimeControl &tc);

    Engine Engine::setNodeLimit(const uint64_t nodes);

    Engine Engine::setPlyLimit(const uint64_t plies);

    Engine setCmd(const std::string &command);

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
    uint64_t getNodeLimit() const;
    uint64_t getPlyLimit() const;
};
