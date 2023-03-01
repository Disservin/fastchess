#include <sstream>

#include "logger.h"

void Logger::openFile(const std::string &file)
{
    log.open(file, std::ios::app);
}

void Logger::writeLog(const std::string &msg, std::thread::id thread)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(logMutex);

    std::stringstream ss;
    ss << "<" << thread << "> <---" << msg << std::endl;

    log << ss.str();
}

void Logger::readLog(const std::string &msg, std::thread::id thread)
{
    // Acquire the lock
    const std::lock_guard<std::mutex> lock(logMutex);

    std::stringstream ss;
    ss << "<" << thread << "> --->" << msg << std::endl;

    log << ss.str();
}
