#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include <core/memory/scope_guard.hpp>

namespace fastchess::util {

template <typename T, typename ID>
class CachedEntry {
   public:
    template <typename... ARGS>
    CachedEntry(const ID& identifier, ARGS&&... arg)
        : entry_(std::make_shared<T>(std::forward<ARGS>(arg)...)), id(identifier) {}

    [[nodiscard]] auto& get() noexcept { return entry_; }

   private:
    std::shared_ptr<T> entry_;
    ID id;

    template <typename TT, typename II>
    friend class CachePool;
};

template <typename T, typename ID>
class CachePool {
   public:
    template <typename... ARGS>
    [[nodiscard]] auto get_or_emplace(const ID& identifier, ARGS&&... arg) {
        std::lock_guard<std::mutex> lock(access_mutex_);

        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it->id == identifier) {
                auto& entry = *it;
                cache_.erase(it);
                return entry;
            }
        }

        cache_.emplace_back(identifier, std::forward<ARGS>(arg)...);
        return cache_.back();
    }

    void put(CachedEntry<T, ID>& entry) {
        std::lock_guard<std::mutex> lock(access_mutex_);
        cache_.push_back(std::move(entry));
    }

   private:
    std::deque<CachedEntry<T, ID>> cache_;
    std::mutex access_mutex_;
};

}  // namespace fastchess::util
