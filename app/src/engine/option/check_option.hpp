#pragma once

#include "ucioption.hpp"

namespace fastchess {

class CheckOption : public UCIOption {
   public:
    CheckOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    std::optional<option_error> setValue(const std::string& value) override {
        if (!isInvalid(value)) {
            this->value = (value == "true");
            return std::nullopt;
        }

        return option_error::invalid_check_option_value;
    }

    std::string getValue() const override { return value ? "true" : "false"; }

    std::optional<option_error> isInvalid(const std::string& value) const override {
        if (value != "true" && value != "false") {
            return option_error::invalid_check_option_value;
        }

        return {};
    }

    Type getType() const override { return Type::Check; }

   private:
    std::string name;
    bool value;
};

}  // namespace fastchess
