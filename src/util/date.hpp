#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace fast_chess {

namespace time {

/// @brief Get the current date and time in a given format.
/// @param format
/// @return
[[nodiscard]] inline std::string datetime(const std::string &format) {
    // Get the current time in UTC
    const auto now        = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    struct tm buf {};

#ifdef _WIN32
    auto res = localtime_s(&buf, &time_t_now);
    if (res != 0) {
        throw std::runtime_error("Warning; localtime_s failed");
    }

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(&buf, format.c_str());
    return ss.str();
#else
    const auto res = localtime_r(&time_t_now, &buf);

    // Format the time as an ISO 8601 string
    std::stringstream ss;
    ss << std::put_time(res, format.c_str());
    return ss.str();
#endif
}

/// @brief Formats a duration in seconds to a string in the format HH:MM:SS.
/// @param duration
/// @return
[[nodiscard]] inline std::string duration(std::chrono::seconds duration) {
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
}  // namespace time

}  // namespace fast_chess
