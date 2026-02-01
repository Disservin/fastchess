#include <core/filesystem/fd_limit.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("File Descriptor Limit") {
    TEST_CASE("Static FD limit") {
#ifndef _WIN64

#    if CAN_USE_EVENTFD_FLAGS

        CHECK(fd_limit::maxSystemFileDescriptorCount() > 0);
        CHECK(fd_limit::minFileDescriptorRequired(1) == 20);
        CHECK(fd_limit::minFileDescriptorRequired(2) == 28);
        CHECK(fd_limit::minFileDescriptorRequired(4) == 44);
        CHECK(fd_limit::minFileDescriptorRequired(8) == 76);
        CHECK(fd_limit::minFileDescriptorRequired(16) == 140);
        CHECK(fd_limit::minFileDescriptorRequired(32) == 268);
        CHECK(fd_limit::maxConcurrency(20) == 1);
        CHECK(fd_limit::maxConcurrency(28) == 2);
        CHECK(fd_limit::maxConcurrency(44) == 4);
        CHECK(fd_limit::maxConcurrency(76) == 8);
        CHECK(fd_limit::maxConcurrency(140) == 16);
        CHECK(fd_limit::maxConcurrency(268) == 32);
#    else
        CHECK(fd_limit::maxSystemFileDescriptorCount() > 0);
        CHECK(fd_limit::minFileDescriptorRequired(1) == 26);
        CHECK(fd_limit::minFileDescriptorRequired(2) == 38);
        CHECK(fd_limit::minFileDescriptorRequired(4) == 62);
        CHECK(fd_limit::minFileDescriptorRequired(8) == 110);
        CHECK(fd_limit::minFileDescriptorRequired(16) == 206);
        CHECK(fd_limit::minFileDescriptorRequired(32) == 398);

        CHECK(fd_limit::maxConcurrency(26) == 1);
        CHECK(fd_limit::maxConcurrency(38) == 2);
        CHECK(fd_limit::maxConcurrency(62) == 4);
        CHECK(fd_limit::maxConcurrency(110) == 8);
        CHECK(fd_limit::maxConcurrency(206) == 16);
        CHECK(fd_limit::maxConcurrency(398) == 32);
#    endif
#else
        CHECK(true);
#endif
    }
}

}  // namespace fastchess
