#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include <util/date.hpp>

namespace fast_chess {

class Logger {
   public:
    enum class Level { ALL, TRACE, WARN, INFO, ERR, FATAL };

    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    static void setLevel(Level level);

    static void openFile(const std::string &file);

    template <Level level = Level::WARN, typename First, typename... Args>
    static void log(First &&first, Args &&...args) {
        if (level < level_) {
            return;
        }

        std::stringstream ss;
        ss << std::forward<First>(first);
        ((ss << " " << std::forward<Args>(args)), ...);
        ss << "\n";

        std::cout << ss.str() << std::flush;

        if (!should_log_) {
            return;
        }

        // Acquire the lock
        const std::lock_guard<std::mutex> lock(log_mutex_);

        std::stringstream file_ss;
        file_ss << "[" << util::time::datetime("%H:%M:%S") << "] " << "<fastchess>" << ss.str()
                << std::endl;

        log_ << file_ss.str() << std::flush;
    }

    static void writeToEngine(const std::string &msg, const std::string &name);

    static void readFromEngine(const std::string &msg, const std::string &name, bool err = false);

    static std::atomic_bool should_log_;

   private:
    Logger() {}

    static Level level_;

    static std::ofstream log_;
    static std::mutex log_mutex_;
};

}  // namespace fast_chess
