#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <variant>

#include <core/time/time.hpp>

#define FMT_HEADER_ONLY
#include "../../../third_party/fmt/include/fmt/core.h"
#include "../../../third_party/fmt/include/fmt/std.h"

#ifdef USE_ZLIB
#    include "../../../third_party/gzip/gzstream.h"

class fcgzstream : public ogzstream {
   public:
    fcgzstream() : ogzstream() {}
    fcgzstream(const char *name, int mode = std::ios::out) : ogzstream(name, mode) {}

    bool is_open() { return rdbuf()->is_open(); }
};
#endif

namespace fastchess {

class Logger {
   public:
#ifdef USE_ZLIB
    using log_file_type = std::variant<std::ofstream, fcgzstream>;
#else
    using log_file_type = std::variant<std::ofstream>;
#endif

    enum class Level { ALL, TRACE, WARN, INFO, ERR, FATAL };

    Logger(Logger const &)         = delete;
    void operator=(Logger const &) = delete;

    static void setLevel(Level level) { Logger::level_ = level; }
    static void setCompress(bool compress) { compress_ = compress; }
    static void openFile(const std::string &file);
    static void setEngineComs(bool engine_coms) { engine_coms_ = engine_coms; }

    // Direct function calls - no file path
    template <bool thread = false, typename... T>
    static void trace(fmt::format_string<T...> format, T &&...args) {
        log<Level::TRACE, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void warn(fmt::format_string<T...> format, T &&...args) {
        log<Level::WARN, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void info(fmt::format_string<T...> format, T &&...args) {
        log<Level::INFO, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void err(fmt::format_string<T...> format, T &&...args) {
        log<Level::ERR, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void fatal(fmt::format_string<T...> format, T &&...args) {
        log<Level::FATAL, thread>(format, std::forward<T>(args)...);
    }

    template <Level LEVEL = Level::INFO, bool thread = false, typename... T>
    static void print(fmt::format_string<T...> format, T &&...args) {
        const auto msg = fmt::format(format, std::forward<T>(args)...);

        std::cout << msg << std::endl;

        if (!should_log_) {
            return;
        }

        log<LEVEL, thread>("{}", msg);
    }

    static void writeToEngine(const std::string &msg, const std::string &time, const std::string &name);
    static void readFromEngine(const std::string &msg, const std::string &time, const std::string &name,
                               bool err = false, std::thread::id id = std::this_thread::get_id());

    static std::atomic_bool should_log_;

   private:
    template <Level level = Level::WARN, bool thread = false, typename... T>
    static void log(fmt::format_string<T...> format, T &&...args) {
        if (level < level_) {
            return;
        }

        const auto message = fmt::format(format, std::forward<T>(args)...) + "\n";

        if (!should_log_) {
            return;
        }

        std::string label;
        switch (level) {
            case Level::TRACE:
                label = "TRACE";
                break;
            case Level::WARN:
                label = "WARN";
                break;
            case Level::INFO:
                label = "INFO";
                break;
            case Level::ERR:
                label = "ERR";
                break;
            case Level::FATAL:
                label = "FATAL";
                break;
            default:
                break;
        }

        /*
        label, time, thread_id, message
        */

        const auto fmt = "[{:<6}] [{:>15}] <{:>20}> fastchess --- {}";
        std::string fmt_message;

        if constexpr (thread) {
            fmt_message = fmt::format(fmt, label, util::time::datetime_precise(), std::this_thread::get_id(), message);
        } else {
            fmt_message = fmt::format(fmt, label, util::time::datetime_precise(), "", message);
        }

        const std::lock_guard<std::mutex> lock(log_mutex_);
        std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
    }

    Logger() {}

    static Level level_;
    static bool compress_;
    static bool engine_coms_;
    static log_file_type log_;
    static std::mutex log_mutex_;
};

#define LOG_TRACE(...) Logger::trace(__VA_ARGS__)
#define LOG_WARN(...) Logger::warn(__VA_ARGS__)
#define LOG_INFO(...) Logger::info(__VA_ARGS__)
#define LOG_ERR(...) Logger::err(__VA_ARGS__)
#define LOG_FATAL(...) Logger::fatal(__VA_ARGS__)

#define LOG_TRACE_THREAD(...) Logger::trace<true>(__VA_ARGS__)
#define LOG_WARN_THREAD(...) Logger::warn<true>(__VA_ARGS__)
#define LOG_INFO_THREAD(...) Logger::info<true>(__VA_ARGS__)
#define LOG_ERR_THREAD(...) Logger::err<true>(__VA_ARGS__)
#define LOG_FATAL_THREAD(...) Logger::fatal<true>(__VA_ARGS__)
}  // namespace fastchess