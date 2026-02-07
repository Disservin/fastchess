//! UCI option types and parsing.
//!
//! Ports the C++ `engine/option/` module. The C++ code uses virtual dispatch
//! (UCIOption base class + derived classes). In Rust, we use an enum for the
//! different option types, which is more natural and avoids heap allocation.

use std::fmt;

/// UCI option type.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum OptionType {
    Button,
    Check,
    Combo,
    Spin,
    String,
}

/// A single UCI option as reported by the engine.
#[derive(Debug, Clone)]
pub enum UCIOption {
    Button {
        name: String,
        pressed: bool,
    },
    Check {
        name: String,
        value: bool,
    },
    Combo {
        name: String,
        value: String,
        options: Vec<String>,
    },
    SpinInt {
        name: String,
        value: i64,
        min: i64,
        max: i64,
    },
    SpinFloat {
        name: String,
        value: f64,
        min: f64,
        max: f64,
    },
    Str {
        name: String,
        value: String,
    },
}

impl UCIOption {
    pub fn name(&self) -> &str {
        match self {
            UCIOption::Button { name, .. } => name,
            UCIOption::Check { name, .. } => name,
            UCIOption::Combo { name, .. } => name,
            UCIOption::SpinInt { name, .. } => name,
            UCIOption::SpinFloat { name, .. } => name,
            UCIOption::Str { name, .. } => name,
        }
    }

    pub fn option_type(&self) -> OptionType {
        match self {
            UCIOption::Button { .. } => OptionType::Button,
            UCIOption::Check { .. } => OptionType::Check,
            UCIOption::Combo { .. } => OptionType::Combo,
            UCIOption::SpinInt { .. } => OptionType::Spin,
            UCIOption::SpinFloat { .. } => OptionType::Spin,
            UCIOption::Str { .. } => OptionType::String,
        }
    }

    pub fn value_string(&self) -> String {
        match self {
            UCIOption::Button { pressed, .. } => {
                if *pressed { "true" } else { "false" }.to_string()
            }
            UCIOption::Check { value, .. } => if *value { "true" } else { "false" }.to_string(),
            UCIOption::Combo { value, .. } => value.clone(),
            UCIOption::SpinInt { value, .. } => value.to_string(),
            UCIOption::SpinFloat { value, .. } => value.to_string(),
            UCIOption::Str { value, .. } => {
                if value.is_empty() {
                    "<empty>".to_string()
                } else {
                    value.clone()
                }
            }
        }
    }

    /// Check if a value is valid for this option.
    pub fn is_valid(&self, value: &str) -> bool {
        match self {
            UCIOption::Button { .. } => value == "true",
            UCIOption::Check { .. } => value == "true" || value == "false",
            UCIOption::Combo { options, .. } => options.iter().any(|o| o == value),
            UCIOption::SpinInt { min, max, .. } => {
                if let Ok(v) = value.parse::<i64>() {
                    v >= *min && v <= *max
                } else {
                    false
                }
            }
            UCIOption::SpinFloat { min, max, .. } => {
                if let Ok(v) = value.parse::<f64>() {
                    v >= *min && v <= *max
                } else {
                    false
                }
            }
            UCIOption::Str { .. } => true,
        }
    }

    /// Set the value of this option.
    pub fn set_value(&mut self, value: &str) {
        match self {
            UCIOption::Button { pressed, .. } => {
                if value == "true" {
                    *pressed = true;
                }
            }
            UCIOption::Check { value: current, .. } => {
                if value == "true" || value == "false" {
                    *current = value == "true";
                }
            }
            UCIOption::Combo {
                value: current,
                options,
                ..
            } => {
                if options.iter().any(|o| o == value) {
                    *current = value.to_string();
                }
            }
            UCIOption::SpinInt {
                value: current,
                min,
                max,
                ..
            } => {
                if let Ok(v) = value.parse::<i64>() {
                    if v >= *min && v <= *max {
                        *current = v;
                    }
                }
            }
            UCIOption::SpinFloat {
                value: current,
                min,
                max,
                ..
            } => {
                if let Ok(v) = value.parse::<f64>() {
                    if v >= *min && v <= *max {
                        *current = v;
                    }
                }
            }
            UCIOption::Str { value: current, .. } => {
                *current = value.to_string();
            }
        }
    }
}

impl fmt::Display for UCIOption {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}: {}", self.name(), self.value_string())
    }
}

/// Collection of UCI options reported by an engine.
#[derive(Debug, Clone, Default)]
pub struct UCIOptions {
    options: Vec<UCIOption>,
}

impl UCIOptions {
    pub fn new() -> Self {
        Self {
            options: Vec::new(),
        }
    }

    pub fn add_option(&mut self, option: UCIOption) {
        self.options.push(option);
    }

    pub fn get_option(&self, name: &str) -> Option<&UCIOption> {
        self.options.iter().find(|o| o.name() == name)
    }

    pub fn get_option_mut(&mut self, name: &str) -> Option<&mut UCIOption> {
        self.options.iter_mut().find(|o| o.name() == name)
    }

    pub fn options(&self) -> &[UCIOption] {
        &self.options
    }
}

/// Parse a UCI `option` line into an `UCIOption`.
///
/// Example input:
///   `option name Hash type spin default 16 min 1 max 33554432`
///   `option name Threads type spin default 1 min 1 max 1024`
///   `option name Ponder type check default false`
///   `option name Style type combo default Normal var Solid var Normal var Risky`
pub fn parse_uci_option_line(line: &str) -> Option<UCIOption> {
    let tokens: Vec<&str> = line.split_whitespace().collect();

    // Find "name" token
    let name_start = tokens.iter().position(|t| *t == "name")?;

    // Find "type" token - everything between "name" and "type" is the option name
    let type_pos = tokens[name_start + 1..]
        .iter()
        .position(|t| *t == "type")
        .map(|p| p + name_start + 1)?;

    let name = tokens[name_start + 1..type_pos].join(" ");
    if name.is_empty() {
        return None;
    }

    let type_str = tokens.get(type_pos + 1)?;

    // Collect keyword-value pairs after the type
    let mut params = std::collections::HashMap::<&str, String>::new();
    let mut i = type_pos + 2;
    while i < tokens.len() {
        match tokens[i] {
            "default" => {
                if i + 1 < tokens.len() && !matches!(tokens[i + 1], "min" | "max" | "var" | "type")
                {
                    params.insert("default", tokens[i + 1].to_string());
                    i += 2;
                } else {
                    // empty default
                    params.insert("default", String::new());
                    i += 1;
                }
            }
            "min" => {
                if i + 1 < tokens.len() {
                    params.insert("min", tokens[i + 1].to_string());
                    i += 2;
                } else {
                    i += 1;
                }
            }
            "max" => {
                if i + 1 < tokens.len() {
                    params.insert("max", tokens[i + 1].to_string());
                    i += 2;
                } else {
                    i += 1;
                }
            }
            "var" => {
                // Collect all var values
                let existing = params.entry("var").or_insert_with(String::new);
                if i + 1 < tokens.len() {
                    if !existing.is_empty() {
                        existing.push(' ');
                    }
                    existing.push_str(tokens[i + 1]);
                    i += 2;
                } else {
                    i += 1;
                }
            }
            _ => {
                i += 1;
            }
        }
    }

    match *type_str {
        "check" => {
            let default_val = params.get("default").map_or(false, |v| v == "true");
            Some(UCIOption::Check {
                name,
                value: default_val,
            })
        }
        "spin" => {
            let default_str = params.get("default").map_or("", |v| v.as_str());
            let min_str = params.get("min").map_or("", |v| v.as_str());
            let max_str = params.get("max").map_or("", |v| v.as_str());

            // Try integer first, then float
            if let (Ok(default_val), Ok(min_val), Ok(max_val)) = (
                default_str.parse::<i64>(),
                min_str.parse::<i64>(),
                max_str.parse::<i64>(),
            ) {
                Some(UCIOption::SpinInt {
                    name,
                    value: default_val,
                    min: min_val,
                    max: max_val,
                })
            } else if let (Ok(default_val), Ok(min_val), Ok(max_val)) = (
                default_str.parse::<f64>(),
                min_str.parse::<f64>(),
                max_str.parse::<f64>(),
            ) {
                Some(UCIOption::SpinFloat {
                    name,
                    value: default_val,
                    min: min_val,
                    max: max_val,
                })
            } else {
                None // Non-numeric spin values
            }
        }
        "combo" => {
            let default_val = params.get("default").cloned().unwrap_or_default();
            let options: Vec<String> = params
                .get("var")
                .map(|v| v.split_whitespace().map(|s| s.to_string()).collect())
                .unwrap_or_default();
            Some(UCIOption::Combo {
                name,
                value: default_val,
                options,
            })
        }
        "button" => Some(UCIOption::Button {
            name,
            pressed: false,
        }),
        "string" => {
            let default_val = params.get("default").cloned().unwrap_or_default();
            Some(UCIOption::Str {
                name,
                value: default_val,
            })
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_spin_int() {
        let line = "option name Hash type spin default 16 min 1 max 33554432";
        let opt = parse_uci_option_line(line).unwrap();
        assert_eq!(opt.name(), "Hash");
        assert_eq!(opt.option_type(), OptionType::Spin);
        assert_eq!(opt.value_string(), "16");
        assert!(opt.is_valid("1024"));
        assert!(!opt.is_valid("0"));
        assert!(!opt.is_valid("-1"));
    }

    #[test]
    fn test_parse_check() {
        let line = "option name Ponder type check default false";
        let opt = parse_uci_option_line(line).unwrap();
        assert_eq!(opt.name(), "Ponder");
        assert_eq!(opt.option_type(), OptionType::Check);
        assert_eq!(opt.value_string(), "false");
        assert!(opt.is_valid("true"));
        assert!(opt.is_valid("false"));
        assert!(!opt.is_valid("maybe"));
    }

    #[test]
    fn test_parse_combo() {
        let line = "option name Style type combo default Normal var Solid var Normal var Risky";
        let opt = parse_uci_option_line(line).unwrap();
        assert_eq!(opt.name(), "Style");
        assert_eq!(opt.option_type(), OptionType::Combo);
        assert_eq!(opt.value_string(), "Normal");
        assert!(opt.is_valid("Solid"));
        assert!(!opt.is_valid("Aggressive"));
    }

    #[test]
    fn test_parse_button() {
        let line = "option name Clear Hash type button";
        let opt = parse_uci_option_line(line).unwrap();
        assert_eq!(opt.name(), "Clear Hash");
        assert_eq!(opt.option_type(), OptionType::Button);
    }

    #[test]
    fn test_parse_string() {
        let line = "option name SyzygyPath type string default <empty>";
        let opt = parse_uci_option_line(line).unwrap();
        assert_eq!(opt.name(), "SyzygyPath");
        assert_eq!(opt.option_type(), OptionType::String);
    }

    #[test]
    fn test_set_value() {
        let mut opt = UCIOption::SpinInt {
            name: "Hash".to_string(),
            value: 16,
            min: 1,
            max: 1024,
        };
        opt.set_value("256");
        assert_eq!(opt.value_string(), "256");

        // Out of range - should not change
        opt.set_value("2048");
        assert_eq!(opt.value_string(), "256");
    }

    #[test]
    fn test_uci_options_collection() {
        let mut options = UCIOptions::new();
        options.add_option(UCIOption::SpinInt {
            name: "Hash".to_string(),
            value: 16,
            min: 1,
            max: 1024,
        });
        options.add_option(UCIOption::Check {
            name: "Ponder".to_string(),
            value: false,
        });

        assert!(options.get_option("Hash").is_some());
        assert!(options.get_option("Ponder").is_some());
        assert!(options.get_option("Unknown").is_none());

        let hash = options.get_option_mut("Hash").unwrap();
        hash.set_value("128");
        assert_eq!(options.get_option("Hash").unwrap().value_string(), "128");
    }
}
