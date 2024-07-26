#pragma once

#include <algorithm>
#include <mutex>
#include <vector>

namespace mercury::util {

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

    template <typename U>
    auto remove_if(U PREDICATE) {
        std::lock_guard<std::mutex> lock(mutex_);
        return vec_.erase(std::remove_if(vec_.begin(), vec_.end(), PREDICATE), vec_.end());
    }

    // Not thread safe!
    auto begin() noexcept { return vec_.begin(); }

    // Not thread safe!
    auto end() noexcept { return vec_.end(); }

    void lock() { mutex_.lock(); }
    void unlock() { mutex_.unlock(); }

   private:
    std::mutex mutex_;
    std::vector<T> vec_;
};

}  // namespace mercury::util