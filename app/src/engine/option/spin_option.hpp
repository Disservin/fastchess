#pragma once

#include "ucioption.hpp"

namespace fastchess {

class SpinOption : public UCIOption {
   public:
    SpinOption(const std::string& name, const std::string& minValue, const std::string& maxValue) : name(name) {
        this->minValue = std::stoi(minValue);
        this->maxValue = std::stoi(maxValue);
    }

    std::string getName() const override { return name; }

    void setValue(const std::string& value) override {
        int intValue = std::stoi(value);
        if (isValid(value)) {
            this->value = intValue;
        }
    }

    std::string getValue() const override { return std::to_string(value); }

    bool isValid(const std::string& value) const override {
        int intValue = std::stoi(value);
        return intValue >= minValue && intValue <= maxValue;
    }

    Type getType() const override { return Type::Spin; }

   private:
    std::string name;
    int value;
    int minValue;
    int maxValue;
};

}  // namespace fastchess
