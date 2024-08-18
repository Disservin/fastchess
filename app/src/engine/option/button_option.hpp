#pragma once

#include <string>

#include "ucioption.hpp"

namespace fastchess {

// idk
class ButtonOption : public UCIOption {
   public:
    ButtonOption(const std::string& name) : name(name) {}

    std::string getName() const override { return name; }

    void setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = true;
        }
    }

    std::string getValue() const override { return value ? "pressed" : "not pressed"; }

    bool isValid(const std::string& value) const override { return value == "press"; }

   private:
    std::string name;
    bool value = false;
};

}  // namespace fastchess
