#include <engine/option/option_factory.hpp>

#include "doctest/doctest.hpp"

namespace fastchess {

TEST_SUITE("Uci Options") {
    TEST_CASE("Parse Spin Option") {
        std::string line = "name Hash type spin default 16 min 1 max 16384";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "Hash");

        CHECK(option->getType() == UCIOption::Type::Spin);

        CHECK(option->getValue() == "16");

        CHECK(option->isValid("1"));
        CHECK(option->isValid("16384"));

        CHECK(!option->isValid("0"));
        CHECK(!option->isValid("16385"));

        option->setValue("1");

        CHECK(option->getValue() == "1");
    }

    TEST_CASE("Parse Combo Option") {
        std::string line = "name Style type combo default Normal var Solid var Normal var Risky";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "Style");

        CHECK(option->getType() == UCIOption::Type::Combo);

        CHECK(option->getValue() == "Normal");

        CHECK(option->isValid("Solid"));
        CHECK(option->isValid("Normal"));
        CHECK(option->isValid("Risky"));

        CHECK(!option->isValid("0"));
        CHECK(!option->isValid("Random"));

        option->setValue("Solid");

        CHECK(option->getValue() == "Solid");
    }

    TEST_CASE("Parse Button Option") {
        std::string line = "name Clear Hash type button";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "Clear Hash");

        CHECK(option->getType() == UCIOption::Type::Button);

        CHECK(option->getValue() == "false");

        CHECK(option->isValid("true"));

        option->setValue("true");

        CHECK(option->getValue() == "true");
    }

    TEST_CASE("Parse Check Option") {
        std::string line = "name OwnBook type check default false";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "OwnBook");

        CHECK(option->getType() == UCIOption::Type::Check);

        CHECK(option->getValue() == "false");

        CHECK(option->isValid("true"));

        option->setValue("true");

        CHECK(option->getValue() == "true");
    }

    TEST_CASE("Parse String Option") {
        std::string line = "name NalimovPath type string default <empty>";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "NalimovPath");

        CHECK(option->getType() == UCIOption::Type::String);

        CHECK(option->getValue() == "<empty>");

        CHECK(option->isValid(""));

        option->setValue("nalimov");

        CHECK(option->getValue() == "nalimov");
    }

    TEST_CASE("Parse String Option Default Foo") {
        std::string line = "name NalimovPath type string default Foo";
        auto option      = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(option->getName() == "NalimovPath");

        CHECK(option->getType() == UCIOption::Type::String);

        CHECK(option->getValue() == "Foo");

        CHECK(option->isValid(""));

        option->setValue("nalimov");

        CHECK(option->getValue() == "nalimov");
    }
}

}  // namespace fastchess