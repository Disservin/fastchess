#pragma once

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <json.hpp>

namespace fastchess::str_utils {

// @todo unnecessary with c++20
[[nodiscard]] inline bool startsWith(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

// @todo unnecessary with c++20
[[nodiscard]] inline bool endsWith(std::string_view value, std::string_view ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// Check if a string contains a substring. @todo unnecessary with c++20
[[nodiscard]] inline bool contains(std::string_view haystack, std::string_view needle) noexcept {
    return haystack.find(needle) != std::string::npos;
}

// Check if a vector of strings contains a string.
[[nodiscard]] inline bool contains(const std::vector<std::string> &haystack, std::string_view needle) noexcept {
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

// Split a string into a vector of strings based on a delimiter.
[[nodiscard]] inline std::vector<std::string> splitString(const std::string &string, const char &delimiter) {
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter))
        if (!segment.empty()) seglist.emplace_back(segment);

    return seglist;
}

// Find an element in a vector of strings and return the next element as a specified type.
template <typename T>
[[nodiscard]] std::optional<T> findElement(const std::vector<std::string> &haystack, std::string_view needle) {
    auto position = std::find(haystack.begin(), haystack.end(), needle);
    auto index    = position - haystack.begin();
    if (position == haystack.end()) return std::nullopt;
    if constexpr (std::is_same_v<T, int>)
        return std::stoi(haystack[index + 1]);
    else if constexpr (std::is_same_v<T, float>)
        return std::stof(haystack[index + 1]);
    else if constexpr (std::is_same_v<T, uint64_t>)
        return std::stoull(haystack[index + 1]);
    else
        return haystack[index + 1];
}

[[nodiscard]] inline std::string join(const std::vector<std::string> &strings, const std::string &delimiter) {
    std::string result;

    for (const auto &string : strings) {
        result += string + delimiter;
    }

    // remove delimiter at the end
    return result.substr(0, result.size() - delimiter.size());
}

[[nodiscard]] inline std::optional<std::string> stringAfter(const std::vector<std::string> &haystack,
                                                            std::string_view needle) {
    auto position = std::find(haystack.begin(), haystack.end(), needle);
    auto index    = position - haystack.begin();
    if (position == haystack.end()) return std::nullopt;
    std::vector<std::string> trailing(haystack.begin() + index + 1, haystack.end());
    return join(trailing, " ");
}
}  // namespace fastchess::str_utils
