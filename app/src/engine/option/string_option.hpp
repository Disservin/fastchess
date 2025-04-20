#pragma once

#include <string>

#include "ucioption.hpp"

namespace fastchess {

class StringOption : public UCIOption {
   public:
    StringOption(const std::string& name, const std::string& defaultValue) : name(name), value(defaultValue) {}

    std::string getName() const override { return name; }

    tl::expected<void, option_error> setValue(const std::string& value) override {
        if (isValid(value).value()) {
            this->value = value;
        }

        return {};
    }

    std::string getValue() const override { return value.empty() ? "<empty>" : value; }

    tl::expected<bool, option_error> isValid(const std::string&) const override {
        return true;  // All string values are valid
    }

    Type getType() const override { return Type::String; }

   private:
    std::string name;
    std::string value;
};

}  // namespace fastchess
