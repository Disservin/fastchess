#pragma once

#include <atomic>
#include <functional>
#include <iostream>
#include <type_traits>
#include <utility>

namespace fastchess::util {

template <typename T>
class Resource {
   public:
    template <typename... Args>
    Resource(Args &&...args) : resource_(std::forward<Args>(args)...), available_(true) {}

    Resource(Resource &&other) noexcept : resource_(std::move(other.resource_)), available_(other.available_.load()) {}

    Resource &operator=(Resource &&other) noexcept {
        if (this != &other) {
            resource_ = std::move(other.resource_);
            available_.store(other.available_.load());
        }

        return *this;
    }

    Resource(const Resource &)            = delete;
    Resource &operator=(const Resource &) = delete;

    bool available() const noexcept { return available_; }

    T *acquire() noexcept {
        bool expected = true;

        if (available_.compare_exchange_strong(expected, false)) {
            return &resource_;
        }

        return nullptr;
    }

    void release() noexcept { available_ = true; }

    T *operator->() noexcept { return &resource_; }
    const T *operator->() const noexcept { return &resource_; }

    T &operator*() noexcept { return resource_; }
    const T &operator*() const noexcept { return resource_; }

   private:
    T resource_;
    std::atomic<bool> available_;
};

template <typename F>
class scope_exit {
   private:
    F func;
    bool active;

   public:
    scope_exit(F &&f) noexcept : func(std::forward<F>(f)), active(true) {}

    ~scope_exit() {
        if (active) {
            func();
        }
    }

    scope_exit(const scope_exit &)            = delete;
    scope_exit &operator=(const scope_exit &) = delete;
    scope_exit &operator=(scope_exit &&)      = delete;

    scope_exit(scope_exit &&other) noexcept : func(std::move(other.func)), active(other.active) {
        other.active = false;
    }

    void release() noexcept { active = false; }
};

template <typename F>
scope_exit(F &&) -> scope_exit<F>;

template <typename F>
auto make_scope_exit(F &&f) {
    return scope_exit<F>(std::forward<F>(f));
}

}  // namespace fastchess::util