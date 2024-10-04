#include <util/fd_limit.hpp>

#include <chess.hpp>
#include "doctest/doctest.hpp"

namespace fastchess {

TEST_SUITE("File Descriptor Limit") {
    TEST_CASE("Static FD limit") {
#ifndef _WIN64
        CHECK(util::fd_limit::maxSystemFileDescriptorCount() > 0);
        CHECK(util::fd_limit::minFileDescriptorRequired(1) == 26);
        CHECK(util::fd_limit::minFileDescriptorRequired(2) == 38);
        CHECK(util::fd_limit::minFileDescriptorRequired(4) == 62);
        CHECK(util::fd_limit::minFileDescriptorRequired(8) == 110);
        CHECK(util::fd_limit::minFileDescriptorRequired(16) == 206);
        CHECK(util::fd_limit::minFileDescriptorRequired(32) == 398);

        CHECK(util::fd_limit::maxConcurrency(26) == 1);
        CHECK(util::fd_limit::maxConcurrency(38) == 2);
        CHECK(util::fd_limit::maxConcurrency(62) == 4);
        CHECK(util::fd_limit::maxConcurrency(110) == 8);
        CHECK(util::fd_limit::maxConcurrency(206) == 16);
        CHECK(util::fd_limit::maxConcurrency(398) == 32);
#else
        CHECK(true);
#endif
    }
}

}  // namespace fastchess