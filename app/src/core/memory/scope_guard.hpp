#pragma once

#include <algorithm>
#include <atomic>
#include <deque>
#include <optional>
#include <type_traits>
#include <vector>

namespace fastchess::util {

template <typename T>
class Resource {
   public:
    template <typename... Args>
    Resource(Args &&...args) : data_(std::forward<Args>(args)...), available_(true) {}

    void release() const noexcept { available_ = true; }
    void acquire() const noexcept { available_ = false; }

    [[nodiscard]] bool isAvailable() const noexcept { return available_; }

    T &get() noexcept { return data_; }
    const T &get() const noexcept { return data_; }

   private:
    T data_;
    mutable bool available_;
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