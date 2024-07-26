#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mercury::engine::process {

enum class Standard { INPUT, OUTPUT, ERR };
enum class Status { OK, ERR, TIMEOUT, NONE };

struct Line {
    std::string line;
    std::string time;
    Standard std = Standard::OUTPUT;
};

class IProcess {
   public:
    virtual ~IProcess() = default;

    IProcess()                            = default;
    IProcess(const IProcess &)            = delete;
    IProcess(IProcess &&)                 = delete;
    IProcess &operator=(const IProcess &) = delete;
    IProcess &operator=(IProcess &&)      = delete;

    void setRealtimeLogging(bool realtime_logging) noexcept { realtime_logging_ = realtime_logging; }

    // Initialize the process
    virtual bool init(const std::string &command, const std::string &args, const std::string &log_name) = 0;

    // Returns true if the process is alive
    [[nodiscard]] virtual bool alive() const noexcept = 0;

    virtual void setAffinity(const std::vector<int> &cpus) noexcept = 0;

    virtual void restart() = 0;

   protected:
    // Read stdout until the line matches last_word or timeout is reached
    // 0 threshold means no timeout
    virtual Status readProcess(std::vector<Line> &lines, std::string_view last_word,
                               std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual bool writeProcess(const std::string &input) noexcept = 0;

    bool realtime_logging_ = true;
};

}  // namespace mercury::engine::process
