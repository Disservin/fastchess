#pragma once

#include <config/types.hpp>
#include <matchmaking/output/output_cutechess.hpp>
#include <matchmaking/output/output_fastchess.hpp>

namespace fast_chess {

class OutputFactory {
   public:
    [[nodiscard]] static std::unique_ptr<IOutput> create(const config::Tournament& config) {
        switch (config.output) {
            case OutputType::FASTCHESS:
                return std::make_unique<Fastchess>(config.report_penta);
            case OutputType::CUTECHESS:
                return std::make_unique<Cutechess>();
            default:
                return std::make_unique<Fastchess>(config.report_penta);
        }
    }

    [[nodiscard]] static OutputType getType(const std::string& type) {
        if (type == "fastchess") {
            return OutputType::FASTCHESS;
        }

        if (type == "cutechess") {
            return OutputType::CUTECHESS;
        }

        return OutputType::FASTCHESS;
    }
};

}  // namespace fast_chess
