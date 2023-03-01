#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.hpp"

#include "elo_test.hpp"
#include "engine_test.hpp"
#include "fen_repetition_test.hpp"
#include "functions_test.hpp"
#include "hash_test.hpp"
#include "options_test.hpp"
#include "perft_test.hpp"
#include "san_test.hpp"
#include "tournament_test.hpp"
#include "uci_engine_test.hpp"

int main(int argc, char **argv)
{
    doctest::Context ctx;

    ctx.setOption("abort-after", 1); // default - stop after 5 failed asserts
    ctx.setOption("no-exitcode", false);
    ctx.setOption("success", true);

    const int res = ctx.run(); // run test cases unless with --no-run

    if (ctx.shouldExit()) // query flags (and --exit) rely on this
        return res;       // propagate the result of the tests

    return res;
}
