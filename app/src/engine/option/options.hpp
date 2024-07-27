#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ucioption.hpp"

namespace fastchess {

class UCIOptions {
   public:
    void addOption(std::unique_ptr<UCIOption> option) { options.push_back(std::move(option)); }

    std::optional<UCIOption*> getOption(const std::string& name) {
        for (auto& option : options) {
            if (option->getName() == name) {
                return option.get();
            }
        }

        return std::nullopt;
    }

    std::optional<UCIOption*> getOption(const std::string& name) const {
        for (const auto& option : options) {
            if (option->getName() == name) {
                return option.get();
            }
        }

        return std::nullopt;
    }

   private:
    std::vector<std::unique_ptr<UCIOption>> options;
};

}  // namespace fastchess
