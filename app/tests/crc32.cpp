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
        // create tmp file
        std::ofstream file("tmp.txt");
        file << "Hello, world!";
        file.close();

        auto crc = crc::calculate_crc32("tmp.txt");

        CHECK(crc.has_value());
        CHECK(crc.value() == 0xebe6c6e6);

        // remove tmp file
        std::remove("tmp.txt");
    }
}
