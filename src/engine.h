#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engineprocess.h"

struct TimeControl
{
    uint64_t moves = 0;
    uint64_t time = 0;
    uint64_t increment = 0;
};

class Engine : public EngineProcess
{
  protected:
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
    uint64_t nodes = 0;

    // Ply limit for the engine
    uint64_t plies = 0;

  public:
    Engine() = default;

    Engine(std::string command);

    ~Engine();

    void setName(const std::string &name);

    void setArgs(const std::string &args);

    void setOptions(const std::vector<std::pair<std::string, std::string>> &options);

    void setTc(const TimeControl &tc);

    void setNodeLimit(const uint64_t nodes);

    void setPlyLimit(const uint64_t plies);

    void setCmd(const std::string &command);

    void stopEngine();

    bool pingEngine();

    std::string getName() const;

    std::string getCmd() const;

    std::string getArgs() const;

    std::vector<std::pair<std::string, std::string>> getOptions() const;

    TimeControl getTc() const;
    uint64_t getNodeLimit() const;
    uint64_t getPlyLimit() const;
};
