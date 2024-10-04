#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "ucioption.hpp"

namespace fastchess {

class ComboOption : public UCIOption {
   public:
    ComboOption(const std::string& name, const std::vector<std::string>& options, const std::string& defaultValue)
        : name(name), options(options), value(defaultValue) {}

    std::string getName() const override { return name; }

    void setValue(const std::string& value) override {
        if (isValid(value)) {
            this->value = value;
        }
    }

    std::string getValue() const override { return value; }

    bool isValid(const std::string& value) const override {
        for (const auto& option : options) {
            std::cout << ":" << option << ":" << std::endl;
        }
        return std::find(options.begin(), options.end(), value) != options.end();
    }

    Type getType() const override { return Type::Combo; }

   private:
    std::string name;
    std::vector<std::string> options;
    std::string value;
};

}  // namespace fastchess
