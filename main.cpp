#include <iostream>

#include "options.h"

int main(int argc, char const *argv[])
{
    CMD::Options options = CMD::Options(argc, argv);

    for (auto item : options.getUserInput())
    {
        std::cout << item << std::endl;
    }
    return 0;
}
