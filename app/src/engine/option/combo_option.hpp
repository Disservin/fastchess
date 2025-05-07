#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ucioption.hpp"

namespace fastchess {

class ComboOption : public UCIOption {
   public:
    ComboOption(const std::string& name, const std::vector<std::string>& options) : name(name), options(options) {}

    std::string getName() const override { return name; }

    std::optional<option_error> setValue(const std::string& value) override {
        if (!isInvalid(value)) {
            this->value = value;
            return std::nullopt;
        }

        return option_error::invalid_combo_option_value;
    }

    std::string getValue() const override { return value; }

    std::optional<option_error> isInvalid(const std::string& value) const override {
        if (std::find(options.begin(), options.end(), value) == options.end()) {
            return option_error::invalid_combo_option_value;
        }

        return {};
    }

    Type getType() const override { return Type::Combo; }

   private:
    std::string name;
    std::vector<std::string> options;
    std::string value;
};

}  // namespace fastchess
