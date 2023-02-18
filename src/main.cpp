#include <iostream>

#include "engine.h"
#include "engineprocess.h"
#include "options.h"

int main(int argc, char const *argv[])
{
    // CMD::Options options = CMD::Options(argc, argv);
    // options.print_params();

    EngineProcess process("./BlackCore-v6-0");

    process.writeEngine("uci");

    std::vector<std::string> output = process.readEngine("uciok");

    for (auto item : output)
    {
        std::cout << item << std::endl;
    }

    process.writeEngine("quit");

    return 0;
}
