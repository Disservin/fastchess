#include <engine/process/line_buffer.hpp>

#include <doctest/doctest.hpp>

using namespace fastchess::engine::process;

TEST_SUITE("LineBuffer") {
    TEST_CASE("Lines are taken one at a time, the rest stays") {
        LineBuffer buffer("\n");
        std::string line;

        buffer.append("bestmove e2e4\ninfo string after\nreadyok\n");

        REQUIRE(buffer.next(line));
        CHECK(line == "bestmove e2e4");
        REQUIRE(buffer.next(line));
        CHECK(line == "info string after");
        REQUIRE(buffer.next(line));
        CHECK(line == "readyok");
        CHECK_FALSE(buffer.next(line));
    }

    TEST_CASE("A line split across reads") {
        LineBuffer buffer("\n");
        std::string line;

        buffer.append("best");
        CHECK_FALSE(buffer.next(line));

        buffer.append("move e2e4\nre");
        REQUIRE(buffer.next(line));
        CHECK(line == "bestmove e2e4");
        CHECK_FALSE(buffer.next(line));

        buffer.append("adyok\n");
        REQUIRE(buffer.next(line));
        CHECK(line == "readyok");
    }

    TEST_CASE("Empty lines are skipped, \\r\\n is a single line end") {
        LineBuffer buffer("\r\n");
        std::string line;

        buffer.append("\n\nuciok\r\nreadyok\r");
        buffer.append("\n");

        REQUIRE(buffer.next(line));
        CHECK(line == "uciok");
        REQUIRE(buffer.next(line));
        CHECK(line == "readyok");
        CHECK_FALSE(buffer.next(line));
    }

    TEST_CASE("The incomplete last line") {
        LineBuffer buffer("\n");
        std::string line;

        buffer.append("readyok\nlast");

        REQUIRE(buffer.next(line));
        CHECK_FALSE(buffer.next(line));

        REQUIRE(buffer.takePartial(line));
        CHECK(line == "last");
        CHECK_FALSE(buffer.takePartial(line));
    }

    TEST_CASE("Dropping the incomplete last line keeps the complete ones") {
        LineBuffer buffer("\n");
        std::string line;

        buffer.append("one\ntwo\nthr");
        REQUIRE(buffer.next(line));

        buffer.dropPartial();

        REQUIRE(buffer.next(line));
        CHECK(line == "two");
        CHECK_FALSE(buffer.next(line));
        CHECK_FALSE(buffer.takePartial(line));

        buffer.append("partial");
        buffer.dropPartial();
        CHECK_FALSE(buffer.takePartial(line));
    }
}
