#include <sstream>

#include "logger.hpp"

std::atomic_bool Logger::shouldLog = false;
std::ofstream Logger::log;
std::mutex Logger::logMutex;

void Logger::openFile(const std::string &file)
{
    Logger::log.open(file, std::ios::app);
    Logger::shouldLog = true;
}

void Logger::writeLog(const std::string &msg, std::thread::id thread)
{
    if (Logger::shouldLog)
    {
        // Acquire the lock
        const std::lock_guard<std::mutex> lock(Logger::logMutex);

        std::stringstream ss;
        ss << "<" << thread << "> <---" << msg << std::endl;

        Logger::log << ss.str();
    }
}

void Logger::readLog(const std::string &msg, std::thread::id thread)
{
    if (Logger::shouldLog)
    {
        // Acquire the lock
        const std::lock_guard<std::mutex> lock(Logger::logMutex);

        std::stringstream ss;
        ss << "<" << thread << "> --->" << msg << std::endl;

        Logger::log << ss.str();
    }
}
