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
        reset();
        initializer_ = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    Lazy(const Lazy&)            = delete;
    Lazy& operator=(const Lazy&) = delete;
    Lazy(Lazy&&)                 = delete;
    Lazy& operator=(Lazy&&)      = delete;

    const T& get() const {
        if (!init_flag_) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!init_flag_) {
                instance_  = initializer_();
                init_flag_ = true;
            }
        }

        return *instance_;
    }

   private:
    void reset() const {
        std::lock_guard<std::mutex> lock(mutex_);
        instance_.reset();
        init_flag_ = false;
    }

    mutable bool init_flag_ = false;
    mutable std::unique_ptr<T> instance_;
    mutable std::mutex mutex_;

    std::function<std::unique_ptr<T>()> initializer_;
};

}  // namespace fast_chess::util