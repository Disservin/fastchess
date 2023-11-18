#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class IProcess {
   public:
    enum class Status { OK, ERR, TIMEOUT };

    virtual ~IProcess() = default;

    // Initialize the process
    virtual void init(const std::string &command, const std::string &args,
                      const std::string &log_name) = 0;

    /// @brief Returns true if the process is alive
    /// @return
    [[nodiscard]] virtual bool alive() const = 0;

    virtual void setAffinity(const std::vector<int> &cpus) = 0;

    virtual void restart() = 0;

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold 0 means no timeout
    virtual Status read(std::vector<std::string> &lines, std::string_view last_word,
                        std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual void write(const std::string &input) = 0;
};
