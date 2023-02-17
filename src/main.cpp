#include <iostream>

#include "options.h"

int main(int argc, char const *argv[])
{
    CMD::Options options = CMD::Options(argc, argv);
    options.print_params();

    return 0;
}
