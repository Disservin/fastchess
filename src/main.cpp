#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "engine.h"
#include "options.h"
#include "uci_engine.h"

int main(int argc, char const *argv[])
{
    CMD::Options options = CMD::Options(argc, argv);
    options.print_params();

    bool timeout = false;

    Engine engine;

    engine.initProcess("./DummyEngine.exe");
    engine.writeProcess("uci");
    auto output = engine.readProcess("uciok", timeout);

    for (const auto &item : output)
    {
        std::cout << item << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.writeProcess("quit");
    engine.stopEngine();

    return 0;
}