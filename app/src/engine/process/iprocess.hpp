#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <core/filesystem/file_system.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::engine::process {

enum class Standard { INPUT, OUTPUT, ERR };
enum class Status { OK, ERR, TIMEOUT, NONE };

struct Result {
    Status code;
    std::string message;

    Result(Status code, std::string message) : code(code), message(std::move(message)) {}

    static Result OK() { return {Status::OK, ""}; }
    static Result Error(std::string msg) { return {Status::ERR, std::move(msg)}; }

    Result on_error(std::function<Result()> next) const {
        if (code == Status::OK) return *this;
        return next();
    }

    explicit operator bool() const { return code == Status::OK; }
};

struct Line {
    std::string line;
    std::string time;
    Standard std = Standard::OUTPUT;
};

class IProcess {
   public:
    virtual ~IProcess() = default;

    IProcess()                           = default;
    IProcess(const IProcess&)            = delete;
    IProcess(IProcess&&)                 = delete;
    IProcess& operator=(const IProcess&) = delete;
    IProcess& operator=(IProcess&&)      = delete;

    void setRealtimeLogging(bool realtime_logging) noexcept { realtime_logging_ = realtime_logging; }

    virtual void setupRead() = 0;

    // Initialize the process
    virtual Result init(const std::string& wd, const std::string& command, const std::string& args,
                        const std::string& log_name) = 0;

    // Returns OK if the process is alive, Error otherwise
    [[nodiscard]] virtual Result alive() noexcept = 0;

    virtual bool setAffinity(const std::vector<int>& cpus) noexcept = 0;

    // Read stdout until the line matches last_word or timeout is reached
    // 0 threshold means no timeout
    virtual Result readOutput(std::vector<Line>& lines, std::string_view last_word,
                              std::chrono::milliseconds threshold) = 0;

    // Write input to the engine's stdin
    virtual Result writeInput(const std::string& input) noexcept = 0;

    constexpr static std::chrono::seconds kill_timeout{60};

   protected:
    class LineAccumulator {
       public:
        LineAccumulator() = default;

        LineAccumulator(bool realtime_logging, Standard type, std::string log_name,
                        std::thread::id log_thread_id = std::this_thread::get_id())
            : realtime_logging_(realtime_logging), log_name_(std::move(log_name)), type_(type),
              log_thread_id_(log_thread_id) {
            line_buffer_.reserve(300);
        }

        void clear() { line_buffer_.clear(); }

        bool consume(std::string_view data, std::vector<Line>& lines, std::string_view searchword,
                     bool cr_is_newline = false) {
            size_t start = 0;

            while (start < data.size()) {
                const size_t pos = cr_is_newline ? data.find_first_of("\r\n", start) : data.find('\n', start);

                if (pos == std::string_view::npos) {
                    line_buffer_.append(data.substr(start));
                    break;
                }

                line_buffer_.append(data.substr(start, pos - start));

                if (!line_buffer_.empty() && emitLine(lines, searchword)) {
                    return true;
                }

                if (cr_is_newline && data[pos] == '\r' && pos + 1 < data.size() && data[pos + 1] == '\n') {
                    start = pos + 2;
                } else {
                    start = pos + 1;
                }
            }

            return false;
        }

        void flushPartial(std::vector<Line>& lines) {
            if (line_buffer_.empty()) return;
            emitLine(lines, "");
        }

       private:
        bool emitLine(std::vector<Line>& lines, std::string_view searchword) {
            const auto timestamp = Logger::should_log_ ? time::datetime_precise() : "";

            lines.emplace_back(Line{line_buffer_, timestamp, type_});

            if (realtime_logging_) {
                Logger::readFromEngine(line_buffer_, timestamp, log_name_, type_ == Standard::ERR, log_thread_id_);
            }

            const bool matched = !searchword.empty() && line_buffer_.rfind(searchword, 0) == 0;
            line_buffer_.clear();
            return matched;
        }

        bool realtime_logging_ = true;
        std::string line_buffer_;
        std::string log_name_;
        Standard type_ = Standard::OUTPUT;
        std::thread::id log_thread_id_ = std::this_thread::get_id();
    };

    [[nodiscard]] std::string getPath(const std::string& dir, const std::string& cmd) const {
        std::string path = (dir == "." ? "" : dir) + cmd;
#ifndef NO_STD_FILESYSTEM
        // convert path to a filesystem path
        auto p = std::filesystem::path(dir) / std::filesystem::path(cmd);
        path   = p.string();
#endif
        return path;
    }

    bool realtime_logging_ = true;
};

}  // namespace fastchess::engine::process
