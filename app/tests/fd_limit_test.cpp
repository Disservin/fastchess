#include <core/filesystem/fd_limit.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("File Descriptor Limit") {
    TEST_CASE("Static FD limit") {
#ifndef _WIN64
        CHECK(fd_limit::maxSystemFileDescriptorCount() > 0);
        CHECK(fd_limit::minFileDescriptorRequired(1) == 22);
        CHECK(fd_limit::minFileDescriptorRequired(2) == 30);
        CHECK(fd_limit::minFileDescriptorRequired(4) == 46);
        CHECK(fd_limit::minFileDescriptorRequired(8) == 78);
        CHECK(fd_limit::minFileDescriptorRequired(16) == 142);
        CHECK(fd_limit::minFileDescriptorRequired(32) == 270);
        CHECK(fd_limit::maxConcurrency(22) == 1);
        CHECK(fd_limit::maxConcurrency(30) == 2);
        CHECK(fd_limit::maxConcurrency(46) == 4);
        CHECK(fd_limit::maxConcurrency(78) == 8);
        CHECK(fd_limit::maxConcurrency(142) == 16);
        CHECK(fd_limit::maxConcurrency(270) == 32);
#else
        CHECK(true);
#endif
    }
}

}  // namespace fastchess