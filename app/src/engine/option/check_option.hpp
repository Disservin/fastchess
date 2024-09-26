#pragma once

#include "ucioption.hpp"

namespace fastchess {

class CheckOption : public UCIOption {
   public:
    CheckOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    void setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = (value == "true");
        }
    }

    std::string getValue() const override { return value ? "true" : "false"; }

    bool isValid(const std::string& value) const override { return value == "true" || value == "false"; }

   private:
    std::string name;
    bool value;
};

}  // namespace fastchess
