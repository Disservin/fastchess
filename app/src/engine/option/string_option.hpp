#pragma once

#include <algorithm>
#include <string>

#include "ucioption.hpp"

namespace fastchess {

class StringOption : public UCIOption {
   public:
    StringOption(const std::string& name, const std::string& defaultValue) : name(name), value(defaultValue) {}

    std::string getName() const override { return name; }

    void setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = value;
        }
    }

    std::string getValue() const override { return value.empty() ? "<empty>" : value; }

    bool isValid(const std::string&) const override {
        return true;  // All string values are valid
    }

   private:
    std::string name;
    std::string value;
};

}  // namespace fastchess
