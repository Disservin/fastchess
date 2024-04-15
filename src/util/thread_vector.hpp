#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

namespace fast_chess {

template <typename T>
class ThreadVector {
   public:
    ThreadVector() = default;

    ThreadVector(const ThreadVector &other) {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_ = other.vec_;
    }

    ThreadVector(ThreadVector &&other) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_ = std::move(other.vec_);
    }

    ThreadVector &operator=(const ThreadVector &other) {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_ = other.vec_;
        return *this;
    }

    ThreadVector &operator=(ThreadVector &&other) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_ = std::move(other.vec_);
        return *this;
    }

    void push(T element) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_.push_back(element);
    }

    void remove(T element) {
        std::lock_guard<std::mutex> lock(mutex_);
        vec_.erase(std::remove(vec_.begin(), vec_.end(), element), vec_.end());
    }

    /// @brief Not thread safe!
    /// @return
    auto begin() noexcept { return vec_.begin(); }

    /// @brief Not thread safe!
    /// @return
    auto end() noexcept { return vec_.end(); }

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }

   private:
    std::mutex mutex_;
    std::vector<T> vec_;
};

}  // namespace fast_chess