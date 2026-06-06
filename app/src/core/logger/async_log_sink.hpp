#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace fastchess {

// AsyncLogSink is a single-consumer logging queue.
//
// Producers reserve slots in a bounded ring with atomics and hand off heap-owned
// log tasks to one worker thread. The worker drains tasks in reservation order
// and executes the actual file writes out of band. `wait_mutex_`/`cv_` are only
// used for sleeping and shutdown coordination; slot ownership is tracked by the
// ring sequence numbers.
class AsyncLogSink {
   public:
    using Task = std::function<void()>;

    static constexpr std::size_t capacity = 1u << 14;

    AsyncLogSink() = default;
    AsyncLogSink(const AsyncLogSink&) = delete;
    AsyncLogSink& operator=(const AsyncLogSink&) = delete;
    ~AsyncLogSink();

    void start();
    void enqueue(Task task);
    void shutdown();

   private:
    // Each slot cycles through three states identified by `sequence`:
    // free for producer `pos`, ready for consumer `pos + 1`, then free again at `pos + capacity`.
    struct Node {
        explicit Node(Task task_) : task(new Task(std::move(task_))) {}
        Node() = default;

        std::atomic<std::size_t> sequence{0};
        std::atomic<Task*> task{nullptr};
    };

    void run();

    static constexpr std::size_t queue_mask = capacity - 1;

    std::mutex wait_mutex_;
    // Protects publication of the heap-owned task pointer after a producer has
    // claimed a slot and before the consumer reclaims that same slot.
    std::mutex publish_mutex_;
    std::condition_variable cv_;
    std::array<Node, capacity> queue_{};
    std::atomic<std::size_t> enqueue_pos_{0};
    std::size_t dequeue_pos_ = 0;
    std::atomic<std::size_t> inflight_enqueue_{0};
    std::thread worker_;
    std::atomic_bool started_{false};
    std::atomic_bool accepting_{false};
    std::atomic_bool stop_{false};
};

}  // namespace fastchess
