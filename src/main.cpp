#include <iostream>
#include <csignal>

#include "engine.h"
#include "options.h"

int main(int argc, char const* argv[])
{
	CMD::Options options = CMD::Options(argc, argv);
	options.print_params();

	Engine engine("./DummyEngine");

	engine.startProcess();

	engine.writeProcess("uci"); // NOTE startProcess also sends uci some engines might not like this
	auto output = engine.readProcess("uciok");

	for (const auto& item: output)
	{
		std::cout << item << std::endl;
	}

	sleep(5);
	engine.stopProcess();

	return 0;
}
