#pragma once

#include <string>

#include "ucioption.hpp"

namespace fastchess {

// idk
class ButtonOption : public UCIOption {
   public:
    ButtonOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    tl::expected<void, option_error> setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = true;
        }

        return tl::unexpected(option_error::invalid_button_option_value);
    }

    std::string getValue() const override { return value ? "true" : "false"; }

    tl::expected<bool, option_error> isValid(const std::string& value) const override { return value == "true"; }

    Type getType() const override { return Type::Button; }

   private:
    std::string name;
    bool value = false;
};

}  // namespace fastchess
