#include <matchmaking/output/output_factory.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("Output Factory") {
    TEST_CASE("Return Cutechess Output") {
        auto output = OutputFactory::create(OutputType::CUTECHESS, false);

        CHECK(output != nullptr);
        CHECK(output->getType() == OutputType::CUTECHESS);
    }

    TEST_CASE("Return Fastchess Output") {
        auto output = OutputFactory::create(OutputType::FASTCHESS, false);

        CHECK(output != nullptr);
        CHECK(output->getType() == OutputType::FASTCHESS);
    }

    TEST_CASE("Return Fastchess Output by default") {
        auto output = OutputFactory::create(OutputType::NONE, false);

        CHECK(output != nullptr);
        CHECK(output->getType() == OutputType::FASTCHESS);
    }

    TEST_CASE("Get Cutechess Output Type") {
        auto type = OutputFactory::getType("cutechess");

        CHECK(type == OutputType::CUTECHESS);
    }

    TEST_CASE("Get Fastchess Output Type") {
        auto type = OutputFactory::getType("fastchess");

        CHECK(type == OutputType::FASTCHESS);
    }

    TEST_CASE("Get Fastchess Output Type by default") {
        auto type = OutputFactory::getType("none");

        CHECK(type == OutputType::FASTCHESS);
    }
}

}  // namespace fastchess