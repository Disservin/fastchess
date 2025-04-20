#include <core/logger/logger.hpp>

#include <chrono>
#include <iomanip>
#include <thread>
#include <variant>

#include <core/time/time.hpp>

#ifdef USE_ZLIB
#    include "gzip/gzstream.h"
#endif

namespace fastchess {

bool Logger::compress_               = false;
Logger::Level Logger::level_         = Logger::Level::WARN;
std::atomic_bool Logger::should_log_ = false;

std::mutex Logger::log_mutex_;
Logger::log_file_type Logger::log_;
bool Logger::engine_coms_ = false;
bool Logger::auto_log_    = false;

std::unordered_map<std::thread::id, std::vector<std::string>> log_buffer_;

void Logger::openFile(const std::string &file) {
    if (file.empty()) {
        return;
    }

#ifdef USE_ZLIB
    if (compress_) {
        auto t   = std::chrono::system_clock::now();
        auto fmt = fmt::format("{}{:%Y-%m-%dT.%H.%M.%S}.gz", file, t);
        log_.emplace<fcgzstream>(fmt.c_str(), std::ios::out);
    } else {
        log_.emplace<std::ofstream>(file.c_str(), std::ios::app);
    }
#else
    if (!compress_) {
        log_.emplace<std::ofstream>(file.c_str(), std::ios::app);
    } else {
        throw std::runtime_error("Compress is enabled but program wasn't compiled with zlib.");
    }
#endif

    Logger::should_log_ = true;

    if (auto_log_) {
        should_log_ = false;
    }

    // verify that the file was opened

    std::visit(
        [&](auto &&arg) {
            if (!arg.is_open()) {
                std::cerr << "Failed to open log file." << std::endl;
                Logger::should_log_ = false;
            }
        },
        log_);
}

void Logger::writeToEngine(const std::string &msg, const std::string &time, const std::string &name) {
    if (!auto_log_ && (!should_log_ || !engine_coms_)) {
        return;
    }

    const auto timestamp = time.empty() ? time::datetime_precise() : time;

    const auto id = std::this_thread::get_id();

#ifdef _WIN32
    auto fmt_message = fmt::format("[{:<6}] [{:>15}] <{:>3}> {} <--- {}\n", "Engine", timestamp, id, name, msg);
#else
    auto fmt_message = fmt::format("[{:<6}] [{:>15}] <{:>20}> {} <--- {}\n", "Engine", timestamp, id, name, msg);
#endif

    if (!auto_log_) {
        const std::lock_guard<std::mutex> lock(log_mutex_);
        std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
    } else {
        auto_log(Level::INFO, fmt_message, id);
    }
}

void Logger::readFromEngine(const std::string &msg, const std::string &time, const std::string &name, bool err,
                            std::thread::id id) {
    if (!auto_log_ && (!should_log_ || !engine_coms_)) {
        return;
    }

#ifdef _WIN32
    auto fmt_message =
        fmt::format("[{:<6}] [{:>15}] <{:>3}> {}{} ---> {}\n", "Engine", time, id, (err ? "<stderr> " : ""), name, msg);
#else
    auto fmt_message = fmt::format("[{:<6}] [{:>15}] <{:>20}> {}{} ---> {}\n", "Engine", time, id,
                                   (err ? "<stderr> " : ""), name, msg);
#endif

    if (!auto_log_) {
        const std::lock_guard<std::mutex> lock(log_mutex_);
        std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
    } else {
        auto_log(Level::INFO, fmt_message, id);
    }
}

void Logger::auto_log(Level level, std::string_view message, std::thread::id id) {
    const auto timestamp = time::datetime_precise();

    auto &logs = log_buffer_[id];
    logs.emplace_back(message);

    if (level >= Level::WARN) {
        flush_log_buffer(id);
        clear_log_buffer(id);
    }
}

void Logger::clear_log_buffer(std::thread::id id) {
    auto it = log_buffer_.find(id);
    if (it != log_buffer_.end()) {
        it->second.clear();
    }
}

void Logger::flush_log_buffer(std::thread::id id) {
    auto it = log_buffer_.find(id);

    if (it != log_buffer_.end()) {
        for (const auto &msg : it->second) {
            const std::lock_guard<std::mutex> lock(log_mutex_);

            std::visit([&](auto &&arg) { arg << msg << std::flush; }, log_);
        }
    }
}

}  // namespace fastchess
