#pragma once

#include <string>

#include "ucioption.hpp"

namespace fastchess {

class StringOption : public UCIOption {
   public:
    StringOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    std::optional<option_error> setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = value;
        }

        return std::nullopt;
    }

    std::string getValue() const override { return value.empty() ? "<empty>" : value; }

    tl::expected<void, option_error> isValid(const std::string&) const override {
        return {};  // All string values are valid
    }

    Type getType() const override { return Type::String; }

   private:
    std::string name;
    std::string value;
};

}  // namespace fastchess
