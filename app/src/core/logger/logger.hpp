#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <variant>

#include <core/logger/async_log_sink.hpp>
#include <core/time/time.hpp>

#define FMT_HEADER_ONLY
#include <fmt/include/fmt/args.h>
#include <fmt/include/fmt/core.h>
#include "fmt/include/fmt/std.h"

#ifdef USE_ZLIB
#    include <gzip/gzstream.h>

class fcgzstream : public ogzstream {
   public:
    fcgzstream() : ogzstream() {}
    fcgzstream(const char* name, int mode = std::ios::out) : ogzstream(name, mode) {}

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

    Logger(Logger const&)         = delete;
    void operator=(Logger const&) = delete;

    static void setLevel(Level level) { Logger::level_ = level; }
    static void setCompress(bool compress) { compress_ = compress; }
    static void openFile(const std::string& file, bool append = true);
    static void shutdown();

    // Direct function calls - no file path
    template <bool thread = false, typename... T>
    static void trace(fmt::format_string<T...> format, T&&... args) {
        log<Level::TRACE, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void warn(fmt::format_string<T...> format, T&&... args) {
        log<Level::WARN, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void info(fmt::format_string<T...> format, T&&... args) {
        log<Level::INFO, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void err(fmt::format_string<T...> format, T&&... args) {
        log<Level::ERR, thread>(format, std::forward<T>(args)...);
    }

    template <bool thread = false, typename... T>
    static void fatal(fmt::format_string<T...> format, T&&... args) {
        log<Level::FATAL, thread>(format, std::forward<T>(args)...);
    }

    template <Level LEVEL = Level::INFO, bool thread = false, typename... T>
    static void print(fmt::format_string<T...> format, T&&... args) {
        const auto msg = fmt::format(format, std::forward<T>(args)...) + "\n";

        std::cout << msg << std::flush;

        if (!should_log_) {
            return;
        }

        log<LEVEL, thread>("{}", msg);
    }

    static void writeToEngine(const std::string& msg, const std::string& time, const std::string& name);
    static void readFromEngine(const std::string& msg, const std::string& time, const std::string& name,
                               bool err = false, std::thread::id id = std::this_thread::get_id());

    static std::atomic_bool should_log_;

   private:
    template <Level level = Level::WARN, bool thread = false, typename... T>
    static void log(fmt::format_string<T...> format, T&&... args) {
        if (level < level_) {
            return;
        }

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

        auto thread_id_str = thread ? fmt::format("{}", std::this_thread::get_id()) : "";
        auto format_str = std::string(format.get().data(), format.get().size());
        auto arg_store  = std::make_shared<fmt::dynamic_format_arg_store<fmt::format_context>>();
        (arg_store->push_back(std::forward<T>(args)), ...);

        enqueueForFile([label = std::move(label), thread_id_str = std::move(thread_id_str),
                        format_str = std::move(format_str), arg_store = std::move(arg_store)]() mutable {
            auto prefix  = make_prefix(label, time::datetime_precise(), thread_id_str);
            auto message = fmt::vformat(format_str, *arg_store);
            writeToFile(fmt::format("{}fastchess --- {}\n", prefix, message));
        });
    }

#ifdef _WIN32
    static constexpr int ID_WIDTH = 3;
#else
    static constexpr int ID_WIDTH = 20;
#endif

    /*
    label, time, thread_id, message
    */
    template <typename T1, typename T2, typename T3>
    static std::string make_prefix(T1 label, T2 timestamp, T3 thread_id) {
        return fmt::format("[{:<6}] [{:>15}] <{:>{}}> ", label, timestamp, thread_id, ID_WIDTH);
    }

    Logger() {}
    static void enqueueForFile(AsyncLogSink::Task task);
    static void writeToFile(const std::string& message);

    static Level level_;
    static bool compress_;
    static log_file_type log_;
    static AsyncLogSink async_sink_;
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
