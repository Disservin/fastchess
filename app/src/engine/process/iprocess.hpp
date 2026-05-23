#pragma once

#include <chrono>
#include <functional>
#include <optional>
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
        struct CompletedLine {
            size_t start = 0;
            size_t end = 0;
            std::string time;
            size_t sequence = 0;
        };

        LineAccumulator() = default;

        LineAccumulator(bool realtime_logging, Standard type, std::string log_name,
                        size_t* line_sequence, std::thread::id log_thread_id = std::this_thread::get_id())
            : realtime_logging_(realtime_logging), log_name_(std::move(log_name)), type_(type),
              line_sequence_(line_sequence), log_thread_id_(log_thread_id) {
            data_.reserve(4096);
            completed_.reserve(64);
        }

        void clear() {
            data_.clear();
            completed_.clear();
            current_line_start_ = 0;
            next_completed_ = 0;
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

                if (current_line_start_ != data_.size() && finalizeLine(searchword)) {
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
            (void)lines;
            if (current_line_start_ == data_.size()) return;
            finalizeLine("");
        }

        bool hasCompletedLines() const { return next_completed_ < completed_.size(); }

        std::optional<size_t> nextSequence() const {
            if (!hasCompletedLines()) return std::nullopt;
            return completed_[next_completed_].sequence;
        }

        void drainOne(std::vector<Line>& lines) {
            const auto& line = completed_[next_completed_++];
            lines.emplace_back(Line{data_.substr(line.start, line.end - line.start), line.time, type_});

            if (next_completed_ == completed_.size()) {
                completed_.clear();
                next_completed_ = 0;

                if (current_line_start_ > 0) {
                    data_.erase(0, current_line_start_);
                    current_line_start_ = 0;
                }
            }
        }

       private:
        bool finalizeLine(std::string_view searchword) {
            const auto timestamp = Logger::should_log_ ? time::datetime_precise() : "";
            const size_t line_end = data_.size();

            completed_.push_back(CompletedLine{current_line_start_, line_end, timestamp,
                                               line_sequence_ ? (*line_sequence_)++ : completed_.size()});

            if (realtime_logging_) {
                Logger::readFromEngine(data_.substr(current_line_start_, line_end - current_line_start_), timestamp,
                                       log_name_, type_ == Standard::ERR, log_thread_id_);
            }

            const bool matched = startsWith(current_line_start_, line_end, searchword);
            current_line_start_ = line_end;
            return matched;
        }

        bool startsWith(size_t start, size_t end, std::string_view searchword) const {
            if (searchword.empty()) return false;

            const size_t size = end - start;
            if (searchword.size() > size) return false;

            return std::string_view(data_.data() + start, searchword.size()) == searchword;
        }

        bool realtime_logging_ = true;
        std::string data_;
        std::string log_name_;
        Standard type_ = Standard::OUTPUT;
        size_t* line_sequence_ = nullptr;
        std::thread::id log_thread_id_ = std::this_thread::get_id();
        size_t current_line_start_ = 0;
        std::vector<CompletedLine> completed_;
        size_t next_completed_ = 0;
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
