#include <core/logger/async_log_sink.hpp>

#include <cassert>
#include <cstdint>
#include <thread>
#include <utility>

namespace fastchess {

AsyncLogSink::~AsyncLogSink() { shutdown(); }

void AsyncLogSink::start() {
    std::lock_guard<std::mutex> lock(wait_mutex_);
    if (started_.load(std::memory_order_acquire)) {
        return;
    }

    // Reset the bounded ring so every slot initially belongs to producer position `i`.
    for (std::size_t i = 0; i < capacity; ++i) {
        queue_[i].sequence.store(i, std::memory_order_relaxed);
        queue_[i].task.store(nullptr, std::memory_order_relaxed);
    }

    enqueue_pos_.store(0, std::memory_order_relaxed);
    dequeue_pos_ = 0;
    inflight_enqueue_.store(0, std::memory_order_relaxed);
    stop_.store(false, std::memory_order_relaxed);
    accepting_.store(true, std::memory_order_release);
    worker_ = std::thread(&AsyncLogSink::run, this);
    started_.store(true, std::memory_order_release);
}

void AsyncLogSink::enqueue(Task task) {
    assert(started_.load(std::memory_order_acquire) && "AsyncLogSink::enqueue called before start()");

    inflight_enqueue_.fetch_add(1, std::memory_order_acq_rel);

    if (!accepting_.load(std::memory_order_acquire)) {
        inflight_enqueue_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    auto* pending_task = new Task(std::move(task));
    std::size_t pos    = enqueue_pos_.load(std::memory_order_relaxed);

    for (;;) {
        auto& slot             = queue_[pos & queue_mask];
        const auto sequence    = slot.sequence.load(std::memory_order_acquire);
        const auto difference  = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(pos);

        if (difference == 0) {
            // We own this logical position once the CAS succeeds; no other producer
            // can publish into the same slot for this turn around the ring.
            if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed)) {
                {
                    std::lock_guard<std::mutex> publish_lock(publish_mutex_);
                    slot.task.store(pending_task, std::memory_order_relaxed);
                    // Hand the slot to the single consumer.
                    slot.sequence.store(pos + 1, std::memory_order_release);
                }
                break;
            }
        } else if (difference < 0) {
            if (!accepting_.load(std::memory_order_acquire)) {
                delete pending_task;
                inflight_enqueue_.fetch_sub(1, std::memory_order_acq_rel);
                return;
            }
            std::this_thread::yield();
        } else {
            pos = enqueue_pos_.load(std::memory_order_relaxed);
        }
    }

    inflight_enqueue_.fetch_sub(1, std::memory_order_acq_rel);

    {
        // Pair notify with the wait mutex so sleeping/wakeup has a stable predicate.
        std::lock_guard<std::mutex> lock(wait_mutex_);
    }
    cv_.notify_one();
}

void AsyncLogSink::shutdown() {
    std::thread worker;

    {
        std::lock_guard<std::mutex> lock(wait_mutex_);
        if (!started_.load(std::memory_order_acquire)) {
            accepting_.store(false, std::memory_order_release);
            stop_.store(true, std::memory_order_release);
            return;
        }

        accepting_.store(false, std::memory_order_release);
        stop_.store(true, std::memory_order_release);
        worker = std::move(worker_);
    }

    cv_.notify_all();

    if (worker.joinable()) {
        worker.join();
    }

    started_.store(false, std::memory_order_release);
}

void AsyncLogSink::run() {
    while (true) {
        std::unique_lock<std::mutex> lock(wait_mutex_);
        cv_.wait(lock, [this] {
            const auto& slot          = queue_[dequeue_pos_ & queue_mask];
            const auto sequence       = slot.sequence.load(std::memory_order_acquire);
            const auto difference     = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(dequeue_pos_ + 1);
            return stop_.load(std::memory_order_acquire) || difference == 0;
        });

        auto& slot            = queue_[dequeue_pos_ & queue_mask];
        const auto sequence   = slot.sequence.load(std::memory_order_acquire);
        const auto difference = static_cast<std::intptr_t>(sequence) - static_cast<std::intptr_t>(dequeue_pos_ + 1);

        if (difference == 0) {
            Task* task = nullptr;
            {
                std::lock_guard<std::mutex> publish_lock(publish_mutex_);
                task = slot.task.exchange(nullptr, std::memory_order_relaxed);
                // Return the slot to the next producer that wraps onto it.
                slot.sequence.store(dequeue_pos_ + capacity, std::memory_order_release);
                ++dequeue_pos_;
            }
            lock.unlock();

            if (task != nullptr) {
                (*task)();
                delete task;
            }
            continue;
        }

        // `stop_` only lets the worker exit after producers have stopped entering
        // `enqueue`, so no claimed-but-unpublished slot is left behind.
        if (stop_.load(std::memory_order_acquire) && inflight_enqueue_.load(std::memory_order_acquire) == 0) {
            break;
        }
    }
}

}  // namespace fastchess
