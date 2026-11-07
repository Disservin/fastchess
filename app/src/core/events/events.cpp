#include <core/events/events.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

#include <types/exception.hpp>

namespace fastchess::events {

namespace {

std::mutex mutex;
std::condition_variable cv;
std::deque<std::string> queue;
std::thread writer;
std::FILE* out         = nullptr;
bool owns_file         = false;
bool done              = false;
std::atomic_bool alive = false;

std::mutex reason_mutex;
std::string finish_reason;
std::atomic_bool finished = false;

void writeLoop() {
    std::deque<std::string> batch;

    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [] { return done || !queue.empty(); });

            if (queue.empty() && done) return;

            batch.swap(queue);
        }

        for (const auto& line : batch) {
            // one fwrite per event, stdio locks the stream for the call so
            // lines never interleave with human readable stdout output
            std::fwrite(line.data(), 1, line.size(), out);
            std::fflush(out);
        }

        batch.clear();
    }
}

}  // namespace

void EventWriter::open(const std::string& file) {
    if (alive) return;

    if (file == "-") {
        out       = stdout;
        owns_file = false;
    } else {
        out = std::fopen(file.c_str(), "wb");
        if (!out) throw fastchess_exception::format("Could not open events file: {}", file);
        owns_file = true;
    }

    done   = false;
    writer = std::thread(writeLoop);
    alive  = true;
}

bool EventWriter::enabled() noexcept { return alive.load(std::memory_order_relaxed); }

void EventWriter::emit(const json& event) {
    if (!enabled()) return;

    auto line = event.dump(-1, ' ', false, json::error_handler_t::replace);
    line.push_back('\n');

    {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push_back(std::move(line));
    }

    cv.notify_one();
}

void EventWriter::close() {
    if (!alive.exchange(false)) return;

    {
        std::lock_guard<std::mutex> lock(mutex);
        done = true;
    }

    cv.notify_one();
    writer.join();

    if (owns_file) std::fclose(out);
    out = nullptr;
}

void EventWriter::setFinishReason(const std::string& reason) {
    std::lock_guard<std::mutex> lock(reason_mutex);
    if (finish_reason.empty()) finish_reason = reason;
}

std::string EventWriter::finishReason() {
    std::lock_guard<std::mutex> lock(reason_mutex);
    return finish_reason;
}

void EventWriter::finishTournament(const std::string& reason, bool resumable) {
    if (!enabled()) return;

    if (!finished.exchange(true)) {
        json event;
        event["event"]     = "tournament_finished";
        event["reason"]    = reason;
        event["resumable"] = resumable;
        emit(event);
    }

    close();
}

}  // namespace fastchess::events
