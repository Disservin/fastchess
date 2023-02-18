#pragma once

#include <string>
#include <vector>

class Process
{
  public:
    // reads until it finds last_word
    virtual std::vector<std::string> readEngine(std::string_view last_word) = 0;
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

    virtual std::vector<std::string> readEngine(std::string_view last_word);
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

    virtual std::vector<std::string> readEngine(std::string_view last_word);
    virtual void writeEngine(const std::string &input);

  private:
    int inPipe[2], outPipe[2];
};
#endif