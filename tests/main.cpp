#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "../src/helper.h"
#include "../src/options.h"
#include "../src/engine.h"

TEST_CASE("Testing the getUserInput function")
{
    const char *args[] = {"fastchess", "hello", "world"};

    CMD::Options options = CMD::Options(3, args);

    CHECK(options.getUserInput().size() == 2);
}

TEST_CASE("Testing the starts_with function")
{
    CHECK(starts_with("-engine", "-"));
    CHECK(starts_with("-engine", "") == false);
    CHECK(starts_with("-engine", "/-") == false);
    CHECK(starts_with("-engine", "e") == false);
}

TEST_CASE("Testing the contains function")
{
    CHECK(contains("-engine", "-"));
    CHECK(contains("-engine", "e"));
    CHECK(contains("info string depth 10", "depth"));
}

TEST_CASE("Testing the EngineProcess class")
{
#ifdef _WIN32
    Engine engine("DummyEngine.exe");
#else
    Engine engine("./DummyEngine");
#endif

    engine.startProcess();
    bool timedOut = false;

    engine.writeProcess("uci");
    auto uciOutput = engine.readProcess("uciok", timedOut);
    CHECK(timedOut == false);
    CHECK(uciOutput.size() == 3);
    CHECK(uciOutput[0] == "line0");
    CHECK(uciOutput[1] == "line1");
    CHECK(uciOutput[2] == "uciok");

    engine.writeProcess("isready");
    auto readyok = engine.readProcess("readyok", timedOut);
    CHECK(timedOut == false);
    CHECK(readyok.size() == 1);
    CHECK(readyok[0] == "readyok");

    engine.writeProcess("sleep");
    auto sleeper = engine.readProcess("done", timedOut, 100);
    CHECK(timedOut == true);

    engine.writeProcess("sleep");
    auto sleeper2 = engine.readProcess("done", timedOut, 5000);
    CHECK(timedOut == false);
    CHECK(sleeper2.size() == 1);
    CHECK(sleeper2[0] == "done");
}