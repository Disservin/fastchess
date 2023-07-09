#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

template <typename T>
class ThreadCache {
   public:
    T &get(const std::string &name) {
        auto &thread_cache = get_thread_cache();
        auto it = thread_cache.find(name);
        if (it == thread_cache.end()) {
            throw std::runtime_error("T should have been initialized before get!");
        }
        return *(it->second);
    }

    template <typename T_>
    void save(const std::string &name, const T_ &arg) {
        auto &thread_cache = get_thread_cache();
        if (thread_cache.find(name) != thread_cache.end()) return;

        thread_cache[name] = std::make_unique<T>(arg);
    }

   private:
    std::unordered_map<std::string, std::unique_ptr<T>> &get_thread_cache() {
        static thread_local std::unordered_map<std::string, std::unique_ptr<T>> cache;
        return cache;
    }
};