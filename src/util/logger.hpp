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

    /// @brief Thread safe cout
    /// @tparam First
    /// @tparam ...Args
    /// @param first
    /// @param ...args
    template <typename First, typename... Args>
    static void cout(First &&first, Args &&...args) {
        std::stringstream ss;
        ss << std::forward<First>(first);
        ((ss << " " << std::forward<Args>(args)), ...);
        ss << "\n";
        std::cout << ss.str();
    }

    /// @brief Thread safe debug cout, will only print if NDEBUG is not defined
    /// @tparam First
    /// @tparam ...Args
    /// @param first
    /// @param ...args
    template <typename First, typename... Args>
    static void debug([[maybe_unused]] First &&first, [[maybe_unused]] Args &&...args) {
#ifndef NDEBUG
        std::stringstream ss;
        ss << std::forward<First>(first);
        ((ss << " " << std::forward<Args>(args)), ...);
        ss << "\n";
        std::cout << ss.str();
#endif
    }

    /// @brief open log file
    /// @param file
    static void openFile(const std::string &file);

    /// @brief write to file indicating that a write was done
    /// @param msg
    /// @param thread
    static void write(const std::string &msg, std::thread::id thread, const std::string &name);

    /// @brief write to file indicating that a read was done
    /// @param msg
    /// @param thread
    static void read(const std::string &msg, std::thread::id thread, const std::string &name);

    /// @brief log an error
    /// @param msg
    /// @param thread
    static void error(const std::string &msg, std::thread::id thread, const std::string &name);

    [[nodiscard]] static std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");

    [[nodiscard]] static std::string formatDuration(std::chrono::seconds duration);

    static std::atomic_bool should_log_;

   private:
    Logger() {}

    static std::ofstream log_;
    static std::mutex log_mutex_;
};

}  // namespace fast_chess
