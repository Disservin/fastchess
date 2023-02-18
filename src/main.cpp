#include <iostream>

#include "options.h"
#include "engine.h"

int main(int argc, char const *argv[])
{
    // CMD::Options options = CMD::Options(argc, argv);
    // options.print_params();

    Engine engine("./BlackCore-v6-0");

    engine.startProcess();

    engine.pingProcess();

    engine.stopProcess();
    return 0;
}
