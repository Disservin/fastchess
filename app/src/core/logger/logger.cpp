#include <core/logger/logger.hpp>

#include <chrono>
#include <iomanip>
#include <thread>
#include <variant>

#include <core/time/time.hpp>
#include <types/exception.hpp>

#ifdef USE_ZLIB
#    include <gzip/gzstream.h>
#endif

namespace fastchess {

bool Logger::compress_               = false;
Logger::Level Logger::level_         = Logger::Level::WARN;
std::atomic_bool Logger::should_log_ = false;

Logger::log_file_type Logger::log_;
AsyncLogSink Logger::async_sink_;

void Logger::openFile(const std::string& file, bool append) {
    if (file.empty()) {
        return;
    }

    auto flag = (append && !compress_) ? std::ios::app : std::ios::out;
#ifdef USE_ZLIB
    if (compress_) {
        auto t   = std::chrono::system_clock::now();
        auto fmt = fmt::format("{}{:%Y-%m-%dT.%H.%M.%S}.gz", file, t);
        log_.emplace<fcgzstream>(fmt.c_str(), flag);
    } else {
        log_.emplace<std::ofstream>(file.c_str(), flag);
    }
#else
    if (!compress_) {
        log_.emplace<std::ofstream>(file.c_str(), flag);
    } else {
        throw fastchess_exception("Compress is enabled but program wasn't compiled with zlib.");
    }
#endif

    Logger::should_log_ = true;

    // verify that the file was opened

    std::visit(
        [&](auto&& arg) {
            if (!arg.is_open()) {
                std::cerr << "Failed to open log file." << std::endl;
                Logger::should_log_ = false;
            }
        },
        log_);

    if (Logger::should_log_) {
        async_sink_.start();
    }
}

void Logger::shutdown() {
    should_log_ = false;
    async_sink_.shutdown();
}

void Logger::writeToEngine(const std::string& msg, const std::string& time, const std::string& name) {
    if (!should_log_.load()) {
        return;
    }

    const auto id = std::this_thread::get_id();
    enqueueForFile([msg, time, name, id] {
        const auto timestamp = time.empty() ? time::datetime_precise() : time;
        writeToFile(fmt::format("{} {} <--- {}\n", make_prefix("Engine", timestamp, id), name, msg));
    });
}

void Logger::readFromEngine(const std::string& msg, const std::string& time, const std::string& name, bool err,
                            std::thread::id id) {
    if (!should_log_.load()) {
        return;
    }

    enqueueForFile([msg, time, name, err, id] {
        const auto timestamp = time.empty() ? time::datetime_precise() : time;
        writeToFile(fmt::format("{} {}{} ---> {}\n", make_prefix("Engine", timestamp, id), (err ? "<stderr> " : ""),
                                name, msg));
    });
}

void Logger::enqueueForFile(AsyncLogSink::Task task) {
    async_sink_.enqueue(std::move(task));
}

void Logger::writeToFile(const std::string& message) {
    std::visit(
        [&](auto&& arg) {
            arg << message << std::flush;
        },
        log_);
}

}  // namespace fastchess
