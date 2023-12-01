#pragma once

#include <mutex>
#include <vector>

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

    auto begin() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return vec_.begin();
    }

    auto end() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return vec_.end();
    }

   private:
    std::mutex mutex_;
    std::vector<T> vec_;
};
