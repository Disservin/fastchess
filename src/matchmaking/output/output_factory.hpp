#pragma once

#include "output_cutechess.hpp"
#include "output_fastchess.hpp"

namespace fast_chess {
inline std::unique_ptr<Output> getNewOutput(OutputType type) {
    switch (type) {
        case OutputType::FASTCHESS:
            return std::make_unique<Fastchess>();
        case OutputType::CUTECHESS:
            return std::make_unique<Cutechess>();
        default:
            return std::make_unique<Fastchess>();
    }
}

inline OutputType getOutputType(const std::string& type) {
    if (type == "fastchess") {
        return OutputType::FASTCHESS;
    }
    if (type == "cutechess") {
        return OutputType::CUTECHESS;
    }
    return OutputType::FASTCHESS;
}

}  // namespace fast_chess