#include <engine/option/option_factory.hpp>

#include <doctest/doctest.hpp>

namespace fastchess {

TEST_SUITE("Uci Options") {
    TEST_CASE("Parse Spin Option Integer") {
        std::string line = "name Hash type spin default 16 min 1 max 16384";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "Hash");

        CHECK(option->getType() == UCIOption::Type::Spin);

        CHECK(option->getValue() == "16");

        CHECK(!option->isInvalid("1"));
        CHECK(!option->isInvalid("16384"));

        CHECK(option->isInvalid("0"));
        CHECK(option->isInvalid("16385"));

        option->setValue("1");

        CHECK(option->getValue() == "1");
    }

    TEST_CASE("Parse Spin Option Double") {
        std::string line = "name x1 type spin default 321.12321 min 0 max 321321.3213";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "x1");

        CHECK(option->getType() == UCIOption::Type::Spin);

        CHECK(option->getValue() == "321.123210");

        CHECK(!option->isInvalid("1"));
        CHECK(!option->isInvalid("16384"));

        CHECK(option->isInvalid("-1"));
        CHECK(option->isInvalid("321322"));

        option->setValue("1");

        CHECK(option->getValue() == "1.000000");
    }

    TEST_CASE("Parse Spin Option Default String") {
        std::string line = "name x1 type spin default dsad min 0 max 321321.3213";

        CHECK(UCIOptionFactory::parseUCIOptionLine(line).error() == option_error::not_numeric);
    }

    TEST_CASE("Parse Spin Option Min String") {
        std::string line = "name x1 type spin default 3213.21 min foobar max 321321.3213";

        CHECK(UCIOptionFactory::parseUCIOptionLine(line).error() == option_error::not_numeric);
    }

    TEST_CASE("Parse Spin Option Max String") {
        std::string line = "name x1 type spin default 3213.21 min 321.321 max foobar";

        CHECK(UCIOptionFactory::parseUCIOptionLine(line).error() == option_error::not_numeric);
    }

    TEST_CASE("Parse Spin Option Min Larger Than Max") {
        std::string line = "name x1 type spin default 3213.21 min 10 max 0";

        CHECK(!UCIOptionFactory::parseUCIOptionLine(line).has_value());
        CHECK(UCIOptionFactory::parseUCIOptionLine(line).error() == option_error::min_greater_than_max);
    }

    TEST_CASE("Parse Spin Option Default Not In Range") {
        std::string line = "name x1 type spin default 3213 min 0 max 10";

        CHECK(UCIOptionFactory::parseUCIOptionLine(line).error() == option_error::value_out_of_range);
    }

    TEST_CASE("Parse Combo Option") {
        std::string line = "name Style type combo default Normal var Solid var Normal var Risky";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "Style");

        CHECK(option->getType() == UCIOption::Type::Combo);

        CHECK(option->getValue() == "Normal");

        CHECK(!option->isInvalid("Solid"));
        CHECK(!option->isInvalid("Normal"));
        CHECK(!option->isInvalid("Risky"));

        CHECK(option->isInvalid("0"));
        CHECK(option->isInvalid("Random"));

        option->setValue("Solid");

        CHECK(option->getValue() == "Solid");
    }

    TEST_CASE("Parse Button Option") {
        std::string line = "name Clear Hash type button";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "Clear Hash");

        CHECK(option->getType() == UCIOption::Type::Button);

        CHECK(option->getValue() == "false");

        CHECK(!option->isInvalid("true"));

        option->setValue("true");

        CHECK(option->getValue() == "true");
    }

    TEST_CASE("Parse Check Option") {
        std::string line = "name OwnBook type check default false";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "OwnBook");

        CHECK(option->getType() == UCIOption::Type::Check);

        CHECK(option->getValue() == "false");

        CHECK(!option->isInvalid("true"));

        option->setValue("true");

        CHECK(option->getValue() == "true");
    }

    TEST_CASE("Parse String Option") {
        std::string line = "name NalimovPath type string default <empty>";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "NalimovPath");

        CHECK(option->getType() == UCIOption::Type::String);

        CHECK(option->getValue() == "<empty>");

        CHECK(!option->isInvalid(""));

        option->setValue("nalimov");

        CHECK(option->getValue() == "nalimov");
    }

    TEST_CASE("Parse String Option Default Foo") {
        std::string line = "name NalimovPath type string default Foo";
        auto ex_option   = UCIOptionFactory::parseUCIOptionLine(line);

        CHECK(ex_option.has_value());

        auto option = std::move(ex_option.value());

        CHECK(option->getName() == "NalimovPath");

        CHECK(option->getType() == UCIOption::Type::String);

        CHECK(option->getValue() == "Foo");

        CHECK(!option->isInvalid(""));

        option->setValue("nalimov");

        CHECK(option->getValue() == "nalimov");
    }
}

}  // namespace fastchess