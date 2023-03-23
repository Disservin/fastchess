#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

namespace fast_chess {

/// @brief Singleton class for logging
class Logger {
   public:
    static Logger &getInstance() {
        static Logger instance;

        return instance;
    }

    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    template <typename... Args>
    static void coutInfo(Args &&...args) {
        std::stringstream ss;
        ((ss << std::forward<Args>(args) << " "), ...) << "\n";
        std::cout << ss.str();
    }

    /// @brief open log file
    /// @param file
    static void openFile(const std::string &file);

    /// @brief write to file indicating that a write was done
    /// @param msg
    /// @param thread
    static void writeLog(const std::string &msg, std::thread::id thread);

    /// @brief write to file indicating that a read was done
    /// @param msg
    /// @param thread
    static void readLog(const std::string &msg, std::thread::id thread);

    static std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");

    static std::string formatDuration(std::chrono::seconds duration);

    static std::atomic_bool should_log_;

   private:
    Logger() {}

    static std::ofstream log_;
    static std::mutex log_mutex_;
};

}  // namespace fast_chess
