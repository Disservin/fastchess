#include "helper.h"
bool starts_with(std::string_view haystack, std::string_view needle)
{
    return (haystack.rfind(needle, 0) != std::string::npos);
}