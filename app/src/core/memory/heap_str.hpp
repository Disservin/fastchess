#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace fastchess::util {

class heap_string {
   public:
    heap_string() : view_() {}

    heap_string(const std::string& str) {
        buffer.assign(str.begin(), str.end());
        view_ = std::string_view(buffer.data(), buffer.size());
    }

    heap_string(const heap_string& other) : buffer(other.buffer) {
        if (!buffer.empty()) {
            view_ = std::string_view(buffer.data(), buffer.size());
        }
    }

    heap_string(heap_string&& other) noexcept : buffer(std::move(other.buffer)), view_(other.view_) {
        other.view_ = std::string_view();
    }

    heap_string& operator=(const heap_string& other) {
        if (this != &other) {
            buffer = other.buffer;
            view_  = std::string_view(buffer.data(), buffer.size());
        }

        return *this;
    }

    heap_string& operator=(heap_string&& other) noexcept {
        if (this != &other) {
            buffer      = std::move(other.buffer);
            view_       = other.view_;
            other.view_ = std::string_view();
        }

        return *this;
    }

    auto view() const noexcept -> std::string_view { return view_; }

   private:
    std::vector<char> buffer;
    std::string_view view_;
};

}  // namespace fastchess::util
