#include <iostream>

#include "engine.h"
#include "options.h"

int main(int argc, char const *argv[])
{
    CMD::Options options = CMD::Options(argc, argv);
    options.print_params();

    Engine engine;
    engine.setCmd("./Engine");

    engine.startProcess();
    engine.stopProcess();
    return 0;
}
