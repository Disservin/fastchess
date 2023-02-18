#include <iostream>

#include "engine.h"
#include "options.h"

int main(int argc, char const *argv[])
{
    // CMD::Options options = CMD::Options(argc, argv);
    // options.print_params();

    Engine engine("./smallbrain.exe");

    engine.startProcess();

    engine.pingProcess();

    engine.writeProcess("uci");
    auto output = engine.readProcess("uciok");

    for (const auto &item : output)
    {
        std::cout << item << std::endl;
    }

    engine.stopProcess();

    return 0;
}
