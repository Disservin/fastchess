#include <deque>
#include <mutex>
#include <atomic>
#include <functional>
#include <iostream>

template <typename T>
class ScopeManager {
   public:
    explicit ScopeManager(T &entry) : entry_(entry) {}

    ~ScopeManager() { entry_.release(); }

    ScopeManager(const ScopeManager &)            = delete;
    ScopeManager &operator=(const ScopeManager &) = delete;

    [[nodiscard]] auto &get() noexcept { return entry_.get(); }

   private:
    T &entry_;
};

template <typename T, typename ID>
class CachedEntry {
   public:
    template <typename... ARGS>
    CachedEntry(const ID &identifier, ARGS &&...arg)
        : entry_(std::forward<ARGS>(arg)...), id(identifier), available_(false) {}

    [[nodiscard]] T &get() noexcept { return entry_; }

    void release() noexcept { available_ = true; }

   private:
    T entry_;
    ID id;
    std::atomic<bool> available_;

    template <typename TT, typename II>
    friend class CachePool;
};

template <typename T, typename ID>
class CachePool {
   public:
    template <typename... ARGS>
    [[nodiscard]] CachedEntry<T, ID> &getEntry(const ID &identifier, ARGS &&...arg) {
        std::lock_guard<std::mutex> lock(access_mutex_);

        for (auto &entry : cache_) {
            if (entry.available_ && entry.id == identifier) {
                entry.available_ = false;
                return entry;
            }
        }

        cache_.emplace_back(identifier, std::forward<ARGS>(arg)...);
        return cache_.back();
    }

   private:
    std::deque<CachedEntry<T, ID>> cache_;
    std::mutex access_mutex_;
};
