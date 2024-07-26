#pragma once

#include <matchmaking/output/output_cutechess.hpp>
#include <matchmaking/output/output_mercury.hpp>
#include <types/tournament.hpp>

namespace mercury {

class OutputFactory {
   public:
    [[nodiscard]] static std::unique_ptr<IOutput> create(OutputType type, bool report_penta) {
        switch (type) {
            case OutputType::MERCURY:
                return std::make_unique<Mercury>(report_penta);
            case OutputType::CUTECHESS:
                return std::make_unique<Cutechess>();
            default:
                return std::make_unique<Mercury>(report_penta);
        }
    }

    [[nodiscard]] static OutputType getType(const std::string& type) {
        if (type == "mercury ") {
            return OutputType::MERCURY;
        }

        if (type == "cutechess") {
            return OutputType::CUTECHESS;
        }

        return OutputType::MERCURY;
    }
};

}  // namespace mercury
