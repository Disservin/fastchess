#pragma once

#include <string>
#include <chrono>
#include <vector>

class Process
{
  public:

    // Read engine's stdout until the line matches last_word or timeout is reached
    virtual std::vector<std::string> readEngine(std::string_view last_word, unsigned long timeout, bool &timedOut) = 0;

    // Write input to the engine's stdin
    virtual void writeEngine(const std::string &input) = 0;
};

#ifdef _WIN64

#include <iostream>
#include <windows.h>

class EngineProcess : Process
{
  public:
    EngineProcess(const std::string &command);
    ~EngineProcess();

    virtual std::vector<std::string> readEngine(std::string_view last_word, unsigned long timeout, bool &timedOut);
    virtual void writeEngine(const std::string &input);

  private:
    HANDLE m_childProcessHandle;
    HANDLE m_childStdOut;
    HANDLE m_childStdIn;
};

#else
class EngineProcess : Process
{
  public:
    EngineProcess(const std::string &command);
    ~EngineProcess();

    virtual std::vector<std::string> readEngine(std::string_view last_word, unsigned long timeout, bool &timedOut);
    virtual void writeEngine(const std::string &input);

  private:
    int inPipe[2], outPipe[2];
};
#endif