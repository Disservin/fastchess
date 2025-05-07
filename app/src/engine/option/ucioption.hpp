#pragma once

#include <optional>
#include <string>

#include <expected.hpp>

namespace fastchess {

enum class option_error {
    not_numeric,
    min_greater_than_max,
    value_out_of_range,
    unsupported_value_conversion,
    invalid_check_option_value,
    invalid_button_option_value,
    invalid_combo_option_value,
    unknown_option_type,
};

class UCIOption {
   public:
    enum class Type { Button, Check, Combo, Spin, String };

    virtual ~UCIOption()                                                          = default;
    virtual std::string getName() const                                           = 0;
    virtual std::optional<option_error> setValue(const std::string& value)        = 0;
    virtual std::string getValue() const                                          = 0;
    virtual std::optional<option_error> isInvalid(const std::string& value) const = 0;
    virtual Type getType() const                                                  = 0;

    virtual std::optional<option_error> hasError() const { return std::nullopt; }
};

}  // namespace fastchess
