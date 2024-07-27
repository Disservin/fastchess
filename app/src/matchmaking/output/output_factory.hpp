#pragma once

#include <matchmaking/output/output_cutechess.hpp>
#include <matchmaking/output/output_fastchess.hpp>
#include <types/tournament.hpp>

namespace fastchess {

class OutputFactory {
   public:
    [[nodiscard]] static std::unique_ptr<IOutput> create(OutputType type, bool report_penta) {
        switch (type) {
            case OutputType::FASTCHESS:
                return std::make_unique<Fastchess>(report_penta);
            case OutputType::CUTECHESS:
                return std::make_unique<Cutechess>();
            default:
                return std::make_unique<Fastchess>(report_penta);
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

}  // namespace fastchess
