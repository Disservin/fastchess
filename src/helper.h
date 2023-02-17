#pragma once

#include <algorithm>
#include <string>
#include <vector>

bool starts_with(std::string_view haystack, std::string_view needle);

template <typename T> T findElement(std::string_view needle, const std::vector<std::string> &haystack)
{
    int index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
    if constexpr (std::is_same_v<T, int>)
        return std::stoi(haystack[index + 1]);
    else
        return haystack[index + 1];
}

bool contains(std::string_view needle, std::string_view haystack);
bool contains(std::string_view needle, const std::vector<std::string> &haystack);