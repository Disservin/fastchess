#include <iostream>

#include "engine.h"
#include "engineprocess.h"
#include "options.h"

int main(int argc, char const *argv[])
{
    // CMD::Options options = CMD::Options(argc, argv);
    // options.print_params();

    EngineProcess process("E:\\Github\\fast-chess\\src\\smallbrain.exe");

    process.write("uci\n");

    std::vector<std::string> output = process.read("uciok");

    for (auto item : output)
    {
        std::cout << item << std::endl;
    }

    return 0;
}
