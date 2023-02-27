#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Process
{
  public:
    // Read engine's stdout until the line matches last_word or timeout is reached
    virtual std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                                 int64_t timeoutThreshold = 1000) = 0;
    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;

    // Returns true if the engine process is alive
    virtual bool isAlive() = 0;

    bool isInitalized = false;
};

#ifdef _WIN64

#include <iostream>
#include <windows.h>

class EngineProcess : public Process
{
  public:
    EngineProcess() = default;
    EngineProcess(const std::string &command);
    ~EngineProcess();

    void initProcess(const std::string &command);
    void killProcess();
    void closeHandles();

    virtual bool isAlive();

    virtual std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                                 int64_t timeoutThreshold = 1000);
    virtual void writeProcess(const std::string &input);

  private:
    DWORD childdwProcessId;
    HANDLE childProcessHandle;
    HANDLE childStdOut;
    HANDLE childStdIn;
};

#else

class EngineProcess : public Process
{
  public:
    EngineProcess() = default;
    EngineProcess(const std::string &command);
    ~EngineProcess();

    void initProcess(const std::string &command);
    void killProcess();

    virtual bool isAlive();

    virtual std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                                 int64_t timeoutThreshold = 1000);
    virtual void writeProcess(const std::string &input);

  private:
    pid_t processPid;
    int inPipe[2], outPipe[2];
};

#endif