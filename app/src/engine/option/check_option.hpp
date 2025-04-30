#pragma once

#include "ucioption.hpp"

namespace fastchess {

class CheckOption : public UCIOption {
   public:
    CheckOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    tl::expected<void, option_error> setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = (value == "true");
        }

        return tl::unexpected(option_error::invalid_check_option_value);
    }

    std::string getValue() const override { return value ? "true" : "false"; }

    tl::expected<void, option_error> isValid(const std::string& value) const override {
        if (value != "true" && value != "false") {
            return tl::unexpected(option_error::invalid_check_option_value);
        }

        return {};
    }

    Type getType() const override { return Type::Check; }

   private:
    std::string name;
    bool value;
};

}  // namespace fastchess
