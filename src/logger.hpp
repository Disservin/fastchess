#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

/*
 * Singleton logger class.
 */
class Logger
{
  private:
    Logger()
    {
    }

    static std::ofstream log;
    static std::mutex logMutex;

  public:
    static Logger &getInstance()
    {
        static Logger instance;

        return instance;
    }
    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    static void openFile(const std::string &file);

    static void writeLog(const std::string &msg, std::thread::id thread);

    // write to file indicating that a read was done
    static void readLog(const std::string &msg, std::thread::id thread);

    template <typename... Args> static void coutInfo(Args &&...args)
    {
        std::stringstream ss;
        ((ss << " " << std::forward<Args>(args)), ...) << "\n";
        std::cout << ss.str();
    }

    static std::atomic_bool shouldLog;
};
