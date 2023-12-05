#pragma once

#include <optional>
#include <tuple>

namespace fast_chess {

template <typename... ARGS>
class Iterator {
   public:
    virtual ~Iterator() = default;

    virtual std::optional<std::tuple<ARGS...>> next() = 0;
    virtual bool hasNext() const                      = 0;
    virtual void reset()                              = 0;
};
}  // namespace fast_chess
