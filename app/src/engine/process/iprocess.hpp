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
            data_.reserve(4096);
        }

        void clear() {
            data_.clear();
            current_line_start_ = 0;
        }

        bool consume(std::string_view data, std::vector<Line>& lines, std::string_view searchword,
                     bool cr_is_newline = false) {
            (void)lines;

            size_t start = 0;
            while (start < data.size()) {
                const size_t pos = cr_is_newline ? data.find_first_of("\r\n", start) : data.find('\n', start);
                if (pos == std::string_view::npos) {
                    data_.append(data.data() + start, data.size() - start);
                    break;
                }

                if (pos > start) {
                    data_.append(data.data() + start, pos - start);
                }

                if (current_line_start_ != data_.size() && emitLine(lines, searchword)) {
                    return true;
                }

                start = pos + 1;
                if (cr_is_newline && data[pos] == '\r' && start < data.size() && data[start] == '\n') {
                    ++start;
                }
            }

            return false;
        }

        void flushPartial(std::vector<Line>& lines) {
            if (current_line_start_ == data_.size()) return;
            emitLine(lines, "");
        }

       private:
        bool emitLine(std::vector<Line>& lines, std::string_view searchword) {
            const auto timestamp = Logger::should_log_ ? time::datetime_precise() : "";
            const size_t line_end = data_.size();
            const std::string_view line_view(data_.data() + current_line_start_, line_end - current_line_start_);

            lines.emplace_back(Line{std::string(line_view), timestamp, type_});

            if (realtime_logging_) {
                Logger::readFromEngine(std::string(line_view), timestamp, log_name_, type_ == Standard::ERR,
                                       log_thread_id_);
            }

            const bool matched = !searchword.empty() && searchword.size() <= line_view.size() &&
                                 line_view.substr(0, searchword.size()) == searchword;

            current_line_start_ = line_end;
            if (current_line_start_ > 0) {
                data_.erase(0, current_line_start_);
                current_line_start_ = 0;
            }

            return matched;
        }

        bool realtime_logging_ = true;
        std::string data_;
        std::string log_name_;
        Standard type_ = Standard::OUTPUT;
        std::thread::id log_thread_id_ = std::this_thread::get_id();
        size_t current_line_start_ = 0;
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
