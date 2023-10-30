#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

class IProcess {
   public:
    virtual ~IProcess() = default;

    // Initialize the process
    virtual void initProcess(const std::string &command, const std::string &args,
                             const std::string &log_name, const std::vector<int> &cpus) = 0;

    /// @brief Returns true if the process is alive
    /// @return
    virtual bool isAlive() = 0;

    bool timeout() const { return timeout_; }

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold_ms 0 means no timeout
    virtual void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                             std::int64_t threshold_ms) = 0;

    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;

    std::string log_name_;

    bool is_initalized_ = false;
    bool timeout_       = false;
};
