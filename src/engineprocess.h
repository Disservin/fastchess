#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class Process
{
  public:
    // Read engine's stdout until the line matches last_word or timeout is reached
    virtual std::vector<std::string> readEngine(std::string_view last_word, int64_t timeout, bool &timedOut) = 0;

    // Write input to the engine's stdin
    virtual void writeEngine(const std::string &input) = 0;

    // Returns true if the engine process is alive
    // virtual bool isAlive() = 0;

    // Returns true of the engine responds to isready in PING_TIMEOUT_THRESHOLD milliseconds
    // virtual bool isResponsive() = 0;

    // Kills the process
    // virtual void killProcess() = 0;

    // Init the process
    virtual void initProcess(const std::string &command) = 0;
};

#ifdef _WIN64

#include <iostream>
#include <windows.h>

class EngineProcess : Process
{
  public:
    EngineProcess() = default;
    EngineProcess(const std::string &command);
    ~EngineProcess();

    virtual void initProcess(const std::string &command);

    virtual std::vector<std::string> readEngine(std::string_view last_word, int64_t timeout, bool &timedOut);
    virtual void writeEngine(const std::string &input);

    void closeProcess();

  private:
    DWORD m_childdwProcessId;
    HANDLE m_childProcessHandle;
    HANDLE m_childStdOut;
    HANDLE m_childStdIn;
};

#else
class EngineProcess : Process
{
  public:
    EngineProcess() = default;
    EngineProcess(const std::string &command);
    ~EngineProcess();

    virtual void initProcess(const std::string &command);

    virtual std::vector<std::string> readEngine(std::string_view last_word, int64_t timeout, bool &timedOut);
    virtual void writeEngine(const std::string &input);

  private:
    pid_t processPid;
    int inPipe[2], outPipe[2];
};
#endif