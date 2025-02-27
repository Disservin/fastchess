#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace fastchess::util {

namespace sc = std::chrono;

namespace time {

// Get the current date and time in a given format.
[[nodiscard]] inline std::optional<std::string> datetime(const std::string &format) {
    // Get the current time in UTC
    const auto now        = sc::system_clock::now();
    const auto time_t_now = sc::system_clock::to_time_t(now);
    struct tm buf{};

#ifdef _WIN64
    auto res = localtime_s(&buf, &time_t_now);
    if (res != 0) {
        return std::nullopt;
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

// Formats a duration in seconds to a string in the format HH:MM:SS.
[[nodiscard]] inline std::string duration(sc::seconds duration) {
    const auto hours = sc::duration_cast<sc::hours>(duration);
    duration -= hours;
    const auto minutes = sc::duration_cast<sc::minutes>(duration);
    duration -= minutes;
    const auto seconds = duration;

    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours.count() << ":" << std::setfill('0') << std::setw(2)
       << minutes.count() << ":" << std::setfill('0') << std::setw(2) << seconds.count();
    return ss.str();
}

[[nodiscard]] inline std::string datetime_precise() {
    const auto now        = sc::system_clock::now();
    const auto ms         = sc::duration_cast<sc::microseconds>(now.time_since_epoch());
    const auto elapsed_ms = ms % sc::seconds(1);
    const auto str        = datetime("%H:%M:%S");

    std::stringstream ss;
    ss << str.value_or("") << "." << std::setfill('0') << std::setw(6) << elapsed_ms.count();
    return ss.str();
}
}  // namespace time

}  // namespace fastchess::util
