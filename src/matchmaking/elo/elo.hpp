#pragma once

#include <string>

#include <types/stats.hpp>

namespace fast_chess {

class EloBase {
   public:
    EloBase() = default;

    virtual ~EloBase() = default;

    [[nodiscard]] std::string getElo() const noexcept;
    [[nodiscard]] virtual std::string getLos(const Stats& stats) const noexcept        = 0;
    [[nodiscard]] virtual std::string getDrawRatio(const Stats& stats) const noexcept  = 0;
    [[nodiscard]] virtual std::string getScoreRatio(const Stats& stats) const noexcept = 0;
    [[nodiscard]] virtual std::string getnElo() const noexcept                         = 0;

    double percToEloDiff(double percentage) noexcept {
        return -400.0 * std::log10(1.0 / percentage - 1.0);
    }

   protected:
    double diff_;
    double error_;
    double nelodiff_;
    double neloerror_;
};

}  // namespace fast_chess
