#pragma once

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

#include <expected.hpp>

namespace fastchess {

class UCIOptionFactory {
   public:
    static tl::expected<std::unique_ptr<UCIOption>, option_error> parseUCIOptionLine(const std::string& line) {
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
                std::string var;

                while (ss >> var) {
                    if (var == "var") {
                        continue;
                    }

                    params["var"] += var + " ";
                }
            }
        }

        std::unique_ptr<UCIOption> option;

        if (type == "check") {
            option = std::make_unique<CheckOption>(name);
        } else if (type == "spin") {
            if (isInteger(params["default"]) && isInteger(params["min"]) && isInteger(params["max"])) {
                option = std::make_unique<SpinOption<int>>(name, params["min"], params["max"]);
            } else if (isFloat(params["default"]) && isFloat(params["min"]) && isFloat(params["max"])) {
                option = std::make_unique<SpinOption<double>>(name, params["min"], params["max"]);
            } else {
                return tl::unexpected(option_error::not_numeric);
            }
        } else if (type == "combo") {
            std::istringstream varStream(params["var"]);
            std::vector<std::string> options;

            {
                std::string option;
                while (varStream >> option) {
                    options.push_back(option);
                }
            }

            option = std::make_unique<ComboOption>(name, options);
        } else if (type == "button") {
            option = std::make_unique<ButtonOption>(name);
        } else if (type == "string") {
            option = std::make_unique<StringOption>(name);
        }

        if (!option) {
            return tl::unexpected(option_error::unknown_option_type);
        }

        if (option->hasError().has_value()) {
            return tl::unexpected(option->hasError().value());
        }

        if (params.find("default") != params.end()) {
            auto ret = option->setValue(params["default"]);

            if (ret.has_value()) {
                return tl::unexpected(ret.value());
            }
        }

        return option;
    }

   private:
    static bool isInteger(const std::string& s) {
        std::stringstream ss(s);
        int num;
        ss >> num;
        return ss.eof() && !ss.fail();
    }

    static bool isFloat(const std::string& s) {
        std::stringstream ss(s);
        double num;
        ss >> num;
        return ss.eof() && !ss.fail();
    }
};

}  // namespace fastchess
