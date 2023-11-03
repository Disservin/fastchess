#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class IProcess {
   public:
    enum class ProcessStatus { OK, ERR, TIMEOUT };

    virtual ~IProcess() = default;

    // Initialize the process
    virtual void initProcess(const std::string &command, const std::string &args,
                             const std::string &log_name, const std::vector<int> &cpus) = 0;

    /// @brief Returns true if the process is alive
    /// @return
    [[nodiscard]] virtual bool isAlive() const = 0;

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold 0 means no timeout
    virtual ProcessStatus readProcess(std::vector<std::string> &lines, std::string_view last_word,
                                      std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;
};
