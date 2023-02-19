#include <iostream>
#include <csignal>

#include "engine.h"
#include "options.h"

int main(int argc, char const* argv[])
{
	CMD::Options options = CMD::Options(argc, argv);
	options.print_params();

	Engine engine("./DummyEngine");

	engine.writeProcess("uci");
	auto output = engine.readProcess("uciok");

	for (const auto& item: output)
	{
		std::cout << item << std::endl;
	}

	sleep(1);

	return 0;
}
