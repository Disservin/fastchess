#include <logger.hpp>

#include <iomanip>

namespace fast_chess {

std::atomic_bool Logger::should_log_ = false;
std::ofstream Logger::log_;
std::mutex Logger::log_mutex_;

void Logger::openFile(const std::string &file) {
    Logger::log_.open(file, std::ios::app);
    Logger::should_log_ = true;
}

void Logger::write(const std::string &msg, std::thread::id thread) {
    if (!Logger::should_log_) {
        return;
    }

    // Acquire the lock
    const std::lock_guard<std::mutex> lock(Logger::log_mutex_);

    std::stringstream ss;
    ss << "[" << getDateTime("%H:%M:%S") << "]"
       << " <" << std::setw(3) << thread << "> <---" << msg << std::endl;

    Logger::log_ << ss.str() << std::flush;
}

void Logger::read(const std::string &msg, std::thread::id thread) {
    if (!Logger::should_log_) {
        return;
    }

    // Acquire the lock
    const std::lock_guard<std::mutex> lock(Logger::log_mutex_);

    std::stringstream ss;
    ss << "[" << getDateTime("%H:%M:%S") << "]"
       << " <" << std::setw(3) << thread << "> --->" << msg << std::endl;

    Logger::log_ << ss.str() << std::flush;
}

void Logger::error(const std::string &msg, std::thread::id thread) {
    if (!Logger::should_log_) {
        return;
    }

    // Acquire the lock
    const std::lock_guard<std::mutex> lock(Logger::log_mutex_);

    std::stringstream ss;
    ss << "[" << getDateTime("%H:%M:%S") << "]"
       << " <" << std::setw(3) << thread << "> !!!" << msg << std::endl;

    Logger::log_ << ss.str() << std::flush;
}

std::string Logger::getDateTime(std::string format) {
    // Get the current time in UTC
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm buf;

#ifdef _WIN32
    auto res = gmtime_s(&buf, &time_t_now);
    if (res != 0) {
        throw std::runtime_error("Warning: gmtime_s failed");
    }

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(&buf, format.c_str());
    return ss.str();
#else
    const auto res = gmtime_r(&time_t_now, &buf);

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(res, format.c_str());
    return ss.str();
#endif
}

std::string Logger::formatDuration(std::chrono::seconds duration) {
    const auto hours = std::chrono::duration_cast<std::chrono::hours>(duration);
    duration -= hours;
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(duration);
    duration -= minutes;
    const auto seconds = duration;

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours.count() << ":" << std::setfill('0')
       << std::setw(2) << minutes.count() << ":" << std::setfill('0') << std::setw(2)
       << seconds.count();
    return ss.str();
}
}  // namespace fast_chess
