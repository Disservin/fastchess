#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest/doctest.hpp"

#include <process_list.hpp>

namespace fast_chess {
namespace atomic {
std::atomic_bool stop = false;
}  // namespace atomic
}  // namespace fast_chess

namespace fast_chess {

#ifdef _WIN64
#include <windows.h>
ProcessList<HANDLE> pid_list;
#else
#include <unistd.h>
ProcessList<pid_t> pid_list;
#endif
}  // namespace fast_chess

int main(int argc, char **argv) {
    doctest::Context ctx;

    ctx.setOption("abort-after", 1);  // default - stop after 5 failed asserts
    ctx.setOption("no-exitcode", false);
    ctx.setOption("success", true);

    ctx.applyCommandLine(argc, argv);

    const int res = ctx.run();  // run test cases unless with --no-run

    if (ctx.shouldExit())  // query flags (and --exit) rely on this
        return res;        // propagate the result of the tests

    return res;
}
