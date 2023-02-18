#include "helper.h"

#include <sstream>
#include <vector>

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

std::vector<std::string> splitString(const std::string &string, const char &delimiter)
{
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter))
    {
        seglist.emplace_back(segment);
    }

    return seglist;
}

TimeControl ParseTc(std::string tc_string)
{
    // Split the string into move count and time+inc
    std::vector<std::string> move_and_time;
    move_and_time = splitString(tc_string, '/');
    std::string move = move_and_time.front();
    // Split time+inc into time and inc
    std::vector<std::string> time_and_inc = splitString(move_and_time.back(), '+');
    std::string time = time_and_inc.front();
    std::string inc = time_and_inc.back();
    TimeControl time_control;
};