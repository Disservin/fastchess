#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.h"

#include "elo_test.h"
#include "engine_test.h"
#include "fen_repetition_test.h"
#include "functions_test.h"
#include "hash_test.h"
#include "options_test.h"
#include "perft_test.h"
#include "san_test.h"
#include "tournament_test.h"
#include "uci_engine_test.h"

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
