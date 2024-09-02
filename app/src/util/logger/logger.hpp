#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <variant>

#include <util/date.hpp>

#define FMT_HEADER_ONLY
#include "../../../third_party/fmt/include/fmt/core.h"
#include "../../../third_party/fmt/include/fmt/std.h"

#ifdef USE_ZLIB
#    include "../../../third_party/gzip/gzstream.h"
#endif

namespace fastchess {

// Singleton class for logging messages to the console/file.
class Logger {
   public:
#ifdef USE_ZLIB
    using log_file_type = std::variant<std::ofstream, ogzstream>;
#else
    using log_file_type = std::variant<std::ofstream>;
#endif

    enum class Level { ALL, TRACE, WARN, INFO, ERR, FATAL, NONE };

    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    static void setLevel(Level level) { Logger::level_ = level; }

    static void setCompress(bool compress) { compress_ = compress; }

    static void setOnlyErrors(bool onlyerrors) {
        Logger::onlyerrors_ = onlyerrors;
        if (onlyerrors)
           Logger::level_   = Logger::Level::NONE;
    }

    static void openFile(const std::string &file);

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

        if (level >= Level::WARN) {
            std::cout << message << std::flush;
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

        std::string fmt_message;

        if constexpr (thread) {
            fmt_message = fmt::format("[{:<6}] [{}] <{:>3}> fastchess --- {}", label, util::time::datetime_precise(),
                                      std::this_thread::get_id(), message);
        } else {
            fmt_message = fmt::format("[{:<6}] [{}] <fastchess> {}", label, util::time::datetime_precise(), message);
        }

        const std::lock_guard<std::mutex> lock(log_mutex_);

        std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
    }

    Logger() {}

    static Level level_;
    static bool compress_;

    static log_file_type log_;
    static std::mutex log_mutex_;
};

}  // namespace fastchess
