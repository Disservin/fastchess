#pragma once

#include <functional>
#include <memory>
#include <mutex>

namespace fast_chess::util {

template <typename T>
class Lazy {
   public:
    Lazy() : instance_(nullptr), initializer_(nullptr) {}

    template <typename Func, typename... Args>
    void setup(Func&& func, Args&&... args) {
        initializer_ = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    Lazy(const Lazy&)            = delete;
    Lazy& operator=(const Lazy&) = delete;
    Lazy(Lazy&&)                 = delete;
    Lazy& operator=(Lazy&&)      = delete;

    const T& get() const {
        std::call_once(init_flag_, [this]() { instance_ = initializer_(); });
        return *instance_;
    }

   private:
    mutable std::once_flag init_flag_;
    mutable std::unique_ptr<T> instance_;

    std::function<std::unique_ptr<T>()> initializer_;
};

}  // namespace fast_chess::util