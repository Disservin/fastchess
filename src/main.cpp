#include <iostream>

#include "communication.h"
#include "engine.h"
#include "options.h"

int main(int argc, char const *argv[])
{
    // CMD::Options options = CMD::Options(argc, argv);
    // options.print_params();

    Process process("E:\\Github\\fast-chess\\src\\smallbrain.exe");

    process.Write("uci\n");

    std::vector<std::string> output = process.Read("uciok");

    for (auto item : output)
    {
        std::cout << item << std::endl;
    }

    Engine engine;
    engine.setCmd("./Engine");

    engine.startProcess();
    engine.stopProcess();
    return 0;
}
