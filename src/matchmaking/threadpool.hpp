#pragma once

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace fast_chess {

class ThreadPool {
   public:
    ThreadPool(std::size_t num_threads) : stop_(false) {
        for (std::size_t i = 0; i < num_threads; ++i) workers_.emplace_back([this] { work(); });
    }

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (stop_) throw std::runtime_error("Warning: enqueue on stopped ThreadPool");
            tasks_.emplace([task]() { (*task)(); });
        }
        condition_.notify_one();
        return res;
    }

    void resize(std::size_t num_threads) {
        if (num_threads == 0)
            throw std::invalid_argument("Warning: ThreadPool::resize() - num_threads cannot be 0");

        if (num_threads == workers_.size()) return;

        kill();

        stop_ = false;

        workers_.clear();
        workers_.resize(num_threads);

        for (std::size_t i = 0; i < num_threads; ++i) workers_.emplace_back([this] { work(); });
    }

    void kill() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto &worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }

        workers_.clear();
    }

    ~ThreadPool() { kill(); }

    std::size_t queueSize() {
        std::unique_lock<std::mutex> lock(this->queue_mutex_);
        return tasks_.size();
    }

    bool getStop() { return stop_; }

   private:
    void work() {
        while (!this->stop_) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(this->queue_mutex_);
                this->condition_.wait(lock,
                                      [this] { return this->stop_ || !this->tasks_.empty(); });
                if (this->stop_ && this->tasks_.empty()) return;
                task = std::move(this->tasks_.front());
                this->tasks_.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;

    std::atomic_bool stop_;
};

}  // namespace fast_chess
