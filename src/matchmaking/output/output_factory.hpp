#pragma once

#include <matchmaking/output/output_cutechess.hpp>
#include <matchmaking/output/output_fastchess.hpp>

namespace fast_chess {
[[nodiscard]] inline std::unique_ptr<IOutput> getNewOutput(OutputType type) {
    switch (type) {
        case OutputType::FASTCHESS:
            return std::make_unique<Fastchess>();
        case OutputType::CUTECHESS:
            return std::make_unique<Cutechess>();
        default:
            return std::make_unique<Fastchess>();
    }
}

[[nodiscard]] inline OutputType getOutputType(const std::string& type) {
    if (type == "fastchess") {
        return OutputType::FASTCHESS;
    }
    if (type == "cutechess") {
        return OutputType::CUTECHESS;
    }
    return OutputType::FASTCHESS;
}

}  // namespace fast_chess