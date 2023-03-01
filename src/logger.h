#pragma once

#include <atomic>
#include <fstream>
#include <mutex>
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

    std::ofstream log;
    std::mutex logMutex;

  public:
    static Logger &getInstance()
    {
        static Logger instance;

        return instance;
    }
    Logger(Logger const &) = delete;

    void operator=(Logger const &) = delete;

    void openFile(const std::string &file);

    void writeLog(const std::string &msg, std::thread::id thread);

    // write to file indicating that a read was done
    void readLog(const std::string &msg, std::thread::id thread);

    std::atomic_bool shouldLog = false;
};
