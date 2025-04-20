#pragma once

#include <optional>
#include <stdexcept>
#include <type_traits>

#include <expected.hpp>
#include "ucioption.hpp"

namespace fastchess {

template <typename T>
class SpinOption : public UCIOption {
    static_assert(std::is_arithmetic<T>::value, "SpinOption only supports numeric types.");

   public:
    SpinOption(const std::string& name, const std::string& minValue, const std::string& maxValue) : name(name) {
        this->minValue = parseValue(minValue);
        this->maxValue = parseValue(maxValue);
    }

    std::string getName() const override { return name; }

    tl::expected<void, option_error> setValue(const std::string& value) override {
        if (!minValue || !maxValue) {
            return tl::unexpected(option_error::unsupported_value_conversion);
        }

        if (this->minValue > this->maxValue) {
            return tl::unexpected(option_error::min_greater_than_max);
        }

        auto parsedValue = parseValue(value);

        if (parsedValue && isValid(value).value()) {
            this->value = parsedValue.value();
            return {};
        }

        return tl::unexpected(option_error::value_out_of_range);
    }

    std::string getValue() const override { return std::to_string(value); }

    tl::expected<bool, option_error> isValid(const std::string& value) const override {
        auto parsedValue = parseValue(value);

        if (!parsedValue.has_value()) {
            return tl::unexpected(option_error::unsupported_value_conversion);
        }

        return parsedValue >= minValue && parsedValue <= maxValue;
    }

    Type getType() const override { return Type::Spin; }

   private:
    std::string name;
    T value;
    std::optional<T> minValue;
    std::optional<T> maxValue;

    // Helper function to parse the value as type T
    std::optional<T> parseValue(const std::string& valueStr) const {
        if constexpr (std::is_integral<T>::value) {
            return static_cast<T>(std::stoi(valueStr));
        } else if constexpr (std::is_floating_point<T>::value) {
            return static_cast<T>(std::stod(valueStr));
        } else {
            return std::nullopt;
        }
    }
};

}  // namespace fastchess
