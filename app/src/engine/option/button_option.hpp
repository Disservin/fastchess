#pragma once

#include <string>

#include "ucioption.hpp"

namespace fastchess {

// idk
class ButtonOption : public UCIOption {
   public:
    ButtonOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    std::optional<option_error> setValue(const std::string& value) override {
        if (!isInvalid(value)) {
            this->value = true;
            return std::nullopt;
        }

        return option_error::invalid_button_option_value;
    }

    std::string getValue() const override { return value ? "true" : "false"; }

    std::optional<option_error> isInvalid(const std::string& value) const override {
        if (value != "true") {
            return option_error::invalid_button_option_value;
        }

        return {};
    }

    Type getType() const override { return Type::Button; }

   private:
    std::string name;
    bool value = false;
};

}  // namespace fastchess
