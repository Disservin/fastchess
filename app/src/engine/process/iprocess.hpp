#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fastchess::engine::process {

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

    virtual void setupRead() = 0;

    // Initialize the process
    virtual Status init(const std::string &wd, const std::string &command, const std::string &args,
                        const std::string &log_name) = 0;

    // Returns true if the process is alive
    [[nodiscard]] virtual Status alive() const noexcept = 0;

    virtual void setAffinity(const std::vector<int> &cpus) noexcept = 0;

    virtual void restart() = 0;

   protected:
    // Read stdout until the line matches last_word or timeout is reached
    // 0 threshold means no timeout
    virtual Status readProcess(std::vector<Line> &lines, std::string_view last_word,
                               std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual Status writeProcess(const std::string &input) noexcept = 0;

    bool realtime_logging_ = true;
};

}  // namespace fastchess::engine::process
