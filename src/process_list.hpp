#pragma once

#include <vector>
#include <mutex>

template <typename T>
class ProcessList {
   public:
    void push(T process) {
        std::lock_guard<std::mutex> lock(mutex);
        processes.push_back(process);
    }

    void remove(T process) {
        std::lock_guard<std::mutex> lock(mutex);
        processes.erase(std::remove(processes.begin(), processes.end(), process), processes.end());
    }

    auto begin() {
        std::lock_guard<std::mutex> lock(mutex);
        return processes.begin();
    }

    auto end() {
        std::lock_guard<std::mutex> lock(mutex);
        return processes.end();
    }

   private:
    std::mutex mutex;
    std::vector<T> processes;
};
