#include "helper.h"

bool starts_with(std::string_view haystack, std::string_view needle)
{
    if (needle == "")
        return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

bool contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

bool contains(const std::vector<std::string> &haystack, std::string_view needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}