#pragma once

#include <string>

#include <types/stats.hpp>

namespace fast_chess {

class EloBase {
   public:
    EloBase() = default;

    virtual ~EloBase() = default;

    [[nodiscard]] std::string getElo() const noexcept;
    [[nodiscard]] virtual std::string los(const Stats& stats) const noexcept        = 0;
    [[nodiscard]] virtual std::string drawRatio(const Stats& stats) const noexcept  = 0;
    [[nodiscard]] virtual std::string scoreRatio(const Stats& stats) const noexcept = 0;
    [[nodiscard]] virtual std::string nElo() const noexcept                         = 0;

   protected:
    double diff_;
    double error_;
    double nelodiff_;
    double neloerror_;
    double games_;
    double pairs_;
    double score_;
    double variance_;
    double variance_per_pair_;
    double scoreUpperBound_;
    double scoreLowerBound_;
};

}  // namespace fast_chess
