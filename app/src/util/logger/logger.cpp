#include <util/logger/logger.hpp>

#include <chrono>
#include <iomanip>
#include <thread>
#include <variant>

#include <time/time.hpp>

#ifdef USE_ZLIB
#    include "../../../third_party/gzip/gzstream.h"
#endif

namespace fastchess {

bool Logger::compress_               = false;
Logger::Level Logger::level_         = Logger::Level::WARN;
std::atomic_bool Logger::should_log_ = false;

std::mutex Logger::log_mutex_;
Logger::log_file_type Logger::log_;

void Logger::openFile(const std::string &file) {
#ifdef USE_ZLIB
    if (compress_) {
        auto t   = std::chrono::system_clock::now();
        auto fmt = fmt::format("{}{:%Y-%m-%dT.%H.%M.%S}.gz", file, t);
        log_.emplace<ogzstream>(fmt.c_str(), std::ios::out);
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

    // verify that the file was opened

    std::visit(
        [&](auto &&arg) {
            if (!arg.is_open()) {
                std::cerr << "Failed to open log file." << std::endl;
            }
        },
        log_);

    Logger::should_log_ = true;
}

void Logger::writeToEngine(const std::string &msg, const std::string &time, const std::string &name) {
    if (!should_log_) {
        return;
    }

    const auto id = std::this_thread::get_id();

    auto fmt_message = fmt::format("[{:<6}] [{}] <{:>3}> {} <--- {}\n", "Engine", time, id, name, msg);

    const std::lock_guard<std::mutex> lock(log_mutex_);
    std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
}

void Logger::readFromEngine(const std::string &msg, const std::string &time, const std::string &name, bool err,
                            std::thread::id id) {
    if (!should_log_) {
        return;
    }

    auto fmt_message =
        fmt::format("[{:<6}] [{}] <{:>3}> {}{} ---> {}\n", "Engine", time, id, (err ? "<stderr> " : ""), name, msg);

    const std::lock_guard<std::mutex> lock(log_mutex_);
    std::visit([&](auto &&arg) { arg << fmt_message << std::flush; }, log_);
}

}  // namespace fastchess
