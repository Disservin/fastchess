#pragma once

#include <deque>
#include <memory>
#include <mutex>

#include <core/memory/scope_guard.hpp>

namespace fastchess::util {

template <typename T, typename ID>
class CachedEntry : public Resource<T> {
   public:
    template <typename... ARGS>
    CachedEntry(const ID &identifier, ARGS &&...arg) : Resource<T>(std::forward<ARGS>(arg)...), id(identifier) {}

   private:
    ID id;

    template <typename TT, typename II>
    friend class CachePool;
};

// CachePool is a pool of objects which can be reused. An object is indentified by an ID
// type and value. The pool is thread safe.
template <typename T, typename ID>
class CachePool {
   public:
    template <typename... ARGS>
    [[nodiscard]] CachedEntry<T, ID> &getEntry(const ID &identifier, ARGS &&...arg) {
        std::lock_guard<std::mutex> lock(access_mutex_);

        for (auto &entry : cache_) {
            if (entry.available() && entry.id == identifier) {
                entry.acquire();
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

}  // namespace fastchess::util
