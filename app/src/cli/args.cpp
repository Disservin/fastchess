#include <cli/cli.hpp>

#include <cli/cli_args.hpp>
#include <cli/sanitize.hpp>
#include <core/filesystem/file_system.hpp>
#include <core/logger/logger.hpp>
#include <core/rand.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

#define FMT_HEADER_ONLY
#include <fmt/include/fmt/core.h>
#include <fmt/include/fmt/std.h>

template <typename K, typename V>
class InsertionOrderMap {
   private:
    std::unordered_map<K, V> data;
    std::vector<K> order;

   public:
    V& operator[](const K& key) {
        if (data.find(key) == data.end()) {
            order.push_back(key);
        }
        return data[key];
    }

    class iterator {
       private:
        InsertionOrderMap* parent;
        size_t index;

       public:
        iterator(InsertionOrderMap* p, size_t i) : parent(p), index(i) {}

        std::pair<const K&, V&> operator*() {
            const K& key = parent->order[index];
            return {key, parent->data[key]};
        }

        iterator& operator++() {
            ++index;
            return *this;
        }

        bool operator!=(const iterator& other) const { return index != other.index; }
    };

    iterator begin() { return iterator(this, 0); }

    iterator end() { return iterator(this, order.size()); }

    auto has_key(const K& key) const { return data.find(key) != data.end(); }
};

namespace fastchess::cli {

bool SubOption::validate(const std::string& value, std::string& error) const {
    // Check allowed values
    if (!allowed_values_.empty()) {
        if (std::find(allowed_values_.begin(), allowed_values_.end(), value) == allowed_values_.end()) {
            error = "Value '" + value + "' not allowed for '" + name_ + "'. " + "Allowed values: [";
            for (size_t i = 0; i < allowed_values_.size(); ++i) {
                if (i > 0) error += ", ";
                error += allowed_values_[i];
            }
            error += "]";
            return false;
        }
    }

    // Run custom validators
    for (const auto& validator : validators_) {
        if (!validator(value, error)) {
            return false;
        }
    }

    return true;
}

void SubOption::apply(const std::string& value, ArgumentData& data) const {
    if (setter_) {
        setter_(value, data);
    }
}

void Option::parse(const std::vector<std::string>& params, ArgumentData& data) const {
    // Call the setup function before processing anything
    if (before_process_) {
        before_process_(data);
    }

    // Check if we have key=value parameters
    bool has_key_value = false;
    for (const auto& param : params) {
        if (param.find('=') != std::string::npos) {
            has_key_value = true;
            break;
        }
    }

    if (has_key_value) {
        // Process as key=value pairs
        InsertionOrderMap<std::string, std::string> values;
        for (const auto& param : params) {
            size_t pos = param.find('=');
            if (pos != std::string::npos) {
                std::string key   = param.substr(0, pos);
                std::string value = param.substr(pos + 1);
                values[key]       = value;
            }
        }

        // Check for required sub-options
        for (const auto& [sub_name, sub_option] : sub_options_) {
            if (sub_option.isRequired() && !values.has_key(sub_name) && sub_option.defaultValue().empty()) {
                throw std::runtime_error("Required sub-option '" + sub_name + "' missing for option '" + name_ + "'");
            }
        }

        // Process known sub-options
        for (const auto& [key, value] : values) {
            auto it = sub_options_.find(key);
            if (it != sub_options_.end()) {
                std::string error;
                if (!it->second.validate(value, error)) {
                    throw std::runtime_error(error);
                }

                it->second.apply(value, data);
            } else if (dynamic_handler_ && dynamic_handler_(key, value, data)) {
                // Successfully handled by the dynamic handler
            } else {
                throw std::runtime_error("Unknown sub-option '" + key + "' for option '" + name_ + "'");
            }
        }

        // Apply defaults for unspecified options
        for (const auto& [sub_name, sub_option] : sub_options_) {
            if (!values.has_key(sub_name) && !sub_option.defaultValue().empty()) {
                sub_option.apply(sub_option.defaultValue(), data);
            }
        }
    } else if (handler_) {
        // Use the custom handler for non-key=value parameters
        handler_(params, data);
    } else {
        throw std::runtime_error("No handler defined for option '" + name_ + "' with non-key=value parameters");
    }
}

ArgumentData CommandLineParser::parse(int argc, char const* argv[]) {
    return parse(std::vector<std::string>(argv, argv + argc));
}

ArgumentData CommandLineParser::parse(const std::vector<std::string>& args) {
    ArgumentData data;

    for (size_t i = 1; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg[0] != '-') {
            throw std::runtime_error("Expected option starting with '-', got: " + arg);
        }

        std::string option_name = arg.substr(1);  // Remove the leading '-'
        auto it                 = options_.find(option_name);
        if (it == options_.end()) {
            throw std::runtime_error("Unknown option: " + arg);
        }

        // Collect parameters until the next option
        std::vector<std::string> params;
        while (i + 1 < args.size() && args[i + 1][0] != '-') {
            params.push_back(args[++i]);
        }

        // Parse and apply the option
        try {
            it->second.parse(params, data);
        } catch (const std::exception& e) {
            auto err = fmt::format("Error while reading option \"{}\" with value \"{}\"", arg, std::string(args[i]));
            auto msg = fmt::format("Reason: {}", e.what());

            throw std::runtime_error(err + "\n" + msg);
        }
    }

    return data;
}

void CommandLineParser::printHelp(std::ostream& out) const {
    out << program_name_ << " - " << description_ << "\n\n";
    out << "Usage: " << program_name_ << " [options]\n\n";
    out << "Options:\n";

    for (const auto& [name, option] : options_) {
        out << "  -" << name << ": " << option.description() << "\n";

        const auto& sub_options = option.subOptions();
        if (!sub_options.empty()) {
            out << "    Sub-options:\n";
            for (const auto& [sub_name, sub_option] : sub_options) {
                out << "      " << sub_name << ": " << sub_option.description();
                if (!sub_option.defaultValue().empty()) {
                    out << " (default: " << sub_option.defaultValue() << ")";
                }
                if (sub_option.isRequired()) {
                    out << " (required)";
                }
                out << "\n";
            }
        }
    }
}

// Validators implementation
namespace validators {
bool is_boolean(const std::string& value, std::string& error) {
    if (value != "true" && value != "false" && value != "1" && value != "0" && value != "yes" && value != "no") {
        error = "Value must be true/false, 1/0, or yes/no";
        return false;
    }
    return true;
}

bool is_integer(const std::string& value, std::string& error) {
    try {
        std::stoi(value);
        return true;
    } catch (const std::exception&) {
        error = "Value must be an integer";
        return false;
    }
}

bool is_float(const std::string& value, std::string& error) {
    try {
        std::stof(value);
        return true;
    } catch (const std::exception&) {
        error = "Value must be a floating point number";
        return false;
    }
}

std::function<bool(const std::string&, std::string&)> integer_range(int min, int max) {
    return [min, max](const std::string& value, std::string& error) {
        try {
            int val = std::stoi(value);
            if (val < min || val > max) {
                error = "Value must be between " + std::to_string(min) + " and " + std::to_string(max);
                return false;
            }
            return true;
        } catch (const std::exception&) {
            error = "Value must be an integer";
            return false;
        }
    };
}
}  // namespace validators

// Converter implementation
namespace converters {
bool to_bool(const std::string& value) { return value == "true" || value == "1" || value == "yes"; }

int to_int(const std::string& value) { return std::stoi(value); }

float to_float(const std::string& value) { return std::stof(value); }

double to_double(const std::string& value) { return std::stod(value); }
}  // namespace converters

}  // namespace fastchess::cli
