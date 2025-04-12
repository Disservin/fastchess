#pragma once

#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <core/printing/printing.h>
#include <cli/cli_args.hpp>
#include <cli/man.hpp>
#include <core/config/config.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace fastchess::cli {

// Holds the data of the OptionParser (same as before)
struct ArgumentData {
    config::Tournament tournament_config;
    config::Tournament old_tournament_config;
    stats_map stats;
    std::vector<EngineConfiguration> configs;
    std::vector<EngineConfiguration> old_configs;
};

// Class to define a sub-option (like the "file" in "-pgnout file=value")
class SubOption {
   public:
    SubOption() = default;
    SubOption(std::string name, std::string description)
        : name_(std::move(name)), description_(std::move(description)) {}

    // Fluent interface for configuration
    SubOption& required(bool req = true) {
        required_ = req;
        return *this;
    }

    SubOption& defaultValue(std::string value) {
        default_value_ = std::move(value);
        return *this;
    }

    SubOption& allowValues(std::vector<std::string> values) {
        allowed_values_ = std::move(values);
        return *this;
    }

    SubOption& addValidator(std::function<bool(const std::string&, std::string&)> validator) {
        validators_.push_back(std::move(validator));
        return *this;
    }

    SubOption& setSetter(std::function<void(const std::string&, ArgumentData&)> setter) {
        setter_ = std::move(setter);
        return *this;
    }

    bool validate(const std::string& value, std::string& error) const;
    void apply(const std::string& value, ArgumentData& data) const;

    // Getters
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    bool isRequired() const { return required_; }
    const std::string& defaultValue() const { return default_value_; }

   private:
    std::string name_;
    std::string description_;
    bool required_ = false;
    std::string default_value_;
    std::vector<std::string> allowed_values_;
    std::vector<std::function<bool(const std::string&, std::string&)>> validators_;
    std::function<void(const std::string&, ArgumentData&)> setter_;
};

// Main option class (like "-pgnout")
class Option {
   public:
    Option() = default;
    Option(std::string name, std::string description) : name_(std::move(name)), description_(std::move(description)) {}

    Option& setBeforeProcess(std::function<void(ArgumentData&)> setup) {
        before_process_ = std::move(setup);
        return *this;
    }

    Option& setDynamicOptionHandler(
        std::function<bool(const std::string&, const std::string&, ArgumentData&)> handler) {
        dynamic_handler_ = std::move(handler);
        return *this;
    }

    Option& addSubOption(SubOption sub_option) {
        sub_options_[sub_option.name()] = std::move(sub_option);
        return *this;
    }

    Option& setHandler(std::function<void(const std::vector<std::string>&, ArgumentData&)> handler) {
        handler_ = std::move(handler);
        return *this;
    }

    void parse(const std::vector<std::string>& params, ArgumentData& data) const;

    // Getters
    const std::string& name() const { return name_; }
    const std::string& description() const { return description_; }
    const std::map<std::string, SubOption>& subOptions() const { return sub_options_; }

   private:
    std::string name_;
    std::string description_;
    std::map<std::string, SubOption> sub_options_;
    std::function<void(const std::vector<std::string>&, ArgumentData&)> handler_;
    std::function<void(ArgumentData&)> before_process_;
    std::function<bool(const std::string&, const std::string&, ArgumentData&)> dynamic_handler_;
};

// Main parser class
class CommandLineParser {
   public:
    CommandLineParser(std::string program_name, std::string description)
        : program_name_(std::move(program_name)), description_(std::move(description)) {}

    CommandLineParser& addOption(Option option) {
        options_[option.name()] = std::move(option);
        return *this;
    }

    ArgumentData parse(int argc, char const* argv[]);
    ArgumentData parse(const std::vector<std::string>& args);
    void printHelp(std::ostream& out = std::cout) const;

   private:
    std::string program_name_;
    std::string description_;
    std::map<std::string, Option> options_;
};

// Validators namespace
namespace validators {
bool is_boolean(const std::string& value, std::string& error);
bool is_integer(const std::string& value, std::string& error);
bool is_float(const std::string& value, std::string& error);
std::function<bool(const std::string&, std::string&)> integer_range(int min, int max);
}  // namespace validators

// Value converters
namespace converters {
bool to_bool(const std::string& value);
int to_int(const std::string& value);
float to_float(const std::string& value);
double to_double(const std::string& value);
}  // namespace converters

}  // namespace fastchess::cli