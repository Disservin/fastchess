#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace fastchess::util {

struct heap_string {
    std::vector<char> buffer;
    std::string_view view;

    heap_string() : view() {}

    heap_string(const std::string& str) {
        buffer.assign(str.begin(), str.end());
        view = std::string_view(buffer.data(), buffer.size());
    }
};

}  // namespace fastchess::util
