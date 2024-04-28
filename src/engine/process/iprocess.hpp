#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fast_chess::engine::process {
class IProcess {
   public:
    enum class Standard { INPUT, OUTPUT, ERR };

    struct Line {
        std::string line;
        Standard std = Standard::OUTPUT;
    };

    enum class Status { OK, ERR, TIMEOUT };

    virtual ~IProcess() = default;

    // Initialize the process
    virtual void init(const std::string &command, const std::string &args,
                      const std::string &log_name) = 0;

    // Returns true if the process is alive
    [[nodiscard]] virtual bool alive() const = 0;

    virtual void setAffinity(const std::vector<int> &cpus) = 0;

    virtual void restart() = 0;

   protected:
    // Read stdout until the line matches last_word or timeout is reached
    // 0 threshold means no timeout
    virtual Status readProcess(std::vector<Line> &lines, std::string_view last_word,
                               std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;
};

}  // namespace fast_chess::engine::process
