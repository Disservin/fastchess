#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <util/date.hpp>

#define FMT_HEADER_ONLY
#include "../../../third_party/fmt/include/fmt/core.h"

namespace fast_chess {

// Singleton class for logging messages to the console/file.
class Logger {
   public:
    enum class Level { ALL, TRACE, WARN, INFO, ERR, FATAL };

    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    static void setLevel(Level level);

    static void openFile(const std::string &file);

    template <bool thread = false, typename... Args>
    static void trace(const std::string &format, Args &&...args) {
        log<Level::TRACE, thread>(format, std::forward<Args>(args)...);
    }

    template <bool thread = false, typename... Args>
    static void warn(const std::string &format, Args &&...args) {
        log<Level::WARN, thread>(format, std::forward<Args>(args)...);
    }

    template <bool thread = false, typename... Args>
    static void info(const std::string &format, Args &&...args) {
        log<Level::INFO, thread>(format, std::forward<Args>(args)...);
    }

    template <bool thread = false, typename... Args>
    static void err(const std::string &format, Args &&...args) {
        log<Level::ERR, thread>(format, std::forward<Args>(args)...);
    }

    template <bool thread = false, typename... Args>
    static void fatal(const std::string &format, Args &&...args) {
        log<Level::FATAL, thread>(format, std::forward<Args>(args)...);
    }

    static void writeToEngine(const std::string &msg, const std::string &time, const std::string &name);

    static void readFromEngine(const std::string &msg, const std::string &time, const std::string &name,
                               bool err = false);

    static std::atomic_bool should_log_;

   private:
    template <Level level = Level::WARN, bool thread = false, typename... Args>
    static void log(const std::string &format, Args &&...args) {
        if (level < level_) {
            return;
        }

        auto message = fmt::format(format + "\n", std::forward<Args>(args)...);
        std::cout << message << std::flush;

        if (!should_log_) {
            return;
        }

        std::stringstream file_ss;

        if constexpr (thread) {
            file_ss << "[" << util::time::datetime_precise() << "] "               //
                    << " <" << std::setw(3) << std::this_thread::get_id() << "> "  //
                    << "fastchess"                                                 //
                    << " --- "                                                     //
                    << message;
        } else {
            file_ss << "[" << util::time::datetime_precise() << "] "  //
                    << " <fastchess>"                                 //
                    << message;
        }

        const std::lock_guard<std::mutex> lock(log_mutex_);

        log_ << file_ss.str() << std::endl;
    }

    Logger() {}

    static Level level_;

    static std::ofstream log_;
    static std::mutex log_mutex_;
};

}  // namespace fast_chess
