#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN64
#include <windows.h>

#include <iostream>
#endif

namespace fast_chess {

class Process {
   public:
    // Returns true if the engine process is alive
    virtual bool isAlive() = 0;

    bool is_initalized_ = false;

   protected:
    // Read engine's stdout until the line matches last_word or timeout is reached
    virtual std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                                 int64_t timeoutThreshold = 1000) = 0;
    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;
};

#ifdef _WIN64

class EngineProcess : public Process {
   public:
    EngineProcess() = default;
    ~EngineProcess();

    void initProcess(const std::string &command);

    void killProcess();

    void closeHandles();

    bool isAlive() override;

   protected:
    std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                         int64_t timeoutThreshold = 1000) override;
    void writeProcess(const std::string &input) override;

   private:
    PROCESS_INFORMATION pi_ = PROCESS_INFORMATION();
    HANDLE child_std_out_;
    HANDLE child_std_in_;
};

#else

class EngineProcess : public Process {
   public:
    EngineProcess() = default;
    ~EngineProcess();

    void initProcess(const std::string &command);

    void killProcess();

    bool isAlive() override;

   protected:
    std::vector<std::string> readProcess(std::string_view last_word, bool &timeout,
                                         int64_t timeoutThreshold = 1000) override;
    void writeProcess(const std::string &input) override;

   private:
    pid_t process_pid_;
    int in_pipe_[2], out_pipe_[2];
};

#endif

}  // namespace fast_chess