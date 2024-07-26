#pragma once

#include "ucioption.hpp"

namespace mercury {

class CheckOption : public UCIOption {
   public:
    CheckOption(const std::string& name, const std::string& defaultValue) : name(name) { setValue(defaultValue); }

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

}  // namespace mercury
