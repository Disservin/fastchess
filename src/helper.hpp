#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace fast_chess {

/*
Modified version of the NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE macro in nlohmann's json lib.
ordered_json type conversion is not yet supported, though we only have to change the type.
*/
#define NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Type, ...)                                \
    inline void to_json(nlohmann::ordered_json &nlohmann_json_j, const Type &nlohmann_json_t) {   \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                  \
    }                                                                                             \
    inline void from_json(const nlohmann::ordered_json &nlohmann_json_j, Type &nlohmann_json_t) { \
        NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__))                \
    }

/// @brief Table template class for creating N-dimensional arrays.
/// @tparam T
/// @tparam N
/// @tparam ...Dims
template <typename T, std::size_t N, std::size_t... Dims>
struct Table {
    std::array<Table<T, Dims...>, N> data;

    Table() { data.fill({}); }

    Table<T, Dims...> &operator[](std::size_t index) { return data[index]; }

    const Table<T, Dims...> &operator[](std::size_t index) const { return data[index]; }

    void reset(T value) { data.fill({value}); }
};

template <typename T, std::size_t N>
struct Table<T, N> {
    std::array<T, N> data;

    Table() { data.fill({}); }

    T &operator[](std::size_t index) { return data[index]; }

    const T &operator[](std::size_t index) const { return data[index]; }

    void reset(T value) { data.fill({value}); }
};

inline bool startsWith(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

inline bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

inline bool contains(const std::vector<std::string> &haystack, std::string_view needle) {
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

inline std::vector<std::string> splitString(const std::string &string, const char &delimiter) {
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter)) seglist.emplace_back(segment);

    return seglist;
}

template <typename T>
std::optional<T> findElement(const std::vector<std::string> &haystack, std::string_view needle) {
    auto position = std::find(haystack.begin(), haystack.end(), needle);
    auto index = position - haystack.begin();
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

}  // namespace fast_chess
