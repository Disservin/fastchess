#include <core/crc32.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess;

TEST_SUITE("CRC32") {
    TEST_CASE("Hello world") {
        std::uint32_t crc = crc::initial_crc32();
        crc               = crc::incremental_crc32(crc, "Hello, ");
        crc               = crc::incremental_crc32(crc, "world!");
        crc               = crc::finalize_crc32(crc);

        CHECK(crc);
        CHECK(crc == 0xebe6c6e6);
    }

    TEST_CASE("From File") {
        auto crc = crc::calculate_crc32("./app/tests/functions_test.cpp");

        CHECK(crc.has_value());
        CHECK(crc.value() == 0x7bad45e1);
    }
}
