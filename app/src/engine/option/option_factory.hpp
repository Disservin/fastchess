#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ucioption.hpp"

#include "button_option.hpp"
#include "check_option.hpp"
#include "combo_option.hpp"
#include "spin_option.hpp"
#include "string_option.hpp"

namespace fastchess {

class UCIOptionFactory {
   public:
    static std::unique_ptr<UCIOption> parseUCIOptionLine(const std::string& line) {
        std::istringstream ss(line);
        std::string token, name, type;
        std::unordered_map<std::string, std::string> params;

        while (ss >> token) {
            if (token == "name") {
                ss >> name;
                while (ss >> token && token != "type") {
                    name += " " + token;
                }
            }
            if (token == "type") {
                ss >> type;
            } else if (token == "default") {
                ss >> params["default"];
            } else if (token == "min") {
                ss >> params["min"];
            } else if (token == "max") {
                ss >> params["max"];
            } else if (token == "var") {
                if (params.find("var") == params.end()) {
                    params["var"] = token;
                } else {
                    params["var"] += " " + token;
                }
            }
        }

        if (type == "check") {
            auto option = std::make_unique<CheckOption>(name);
            option->setValue(params["default"]);
            return option;
        } else if (type == "spin") {
            auto option = std::make_unique<SpinOption>(name, params["min"], params["max"]);
            option->setValue(params["default"]);
            return option;
        } else if (type == "combo") {
            std::istringstream varStream(params["var"]);
            std::vector<std::string> options;
            std::string option;
            while (varStream >> option) {
                options.push_back(option);
            }
            return std::make_unique<ComboOption>(name, options, params["default"]);
        } else if (type == "button") {
            return std::make_unique<ButtonOption>(name);
        } else if (type == "string") {
            return std::make_unique<StringOption>(name, params["default"]);
        }

        return nullptr;
    }
};

}  // namespace fastchess
