#pragma once

#include <string>
#include <vector>

namespace fast_chess {

class UCIOption {
   public:
    virtual ~UCIOption()                                 = default;
    virtual std::string getName() const                  = 0;
    virtual void setValue(const std::string& value)      = 0;
    virtual std::string getValue() const                 = 0;
    virtual bool isValid(const std::string& value) const = 0;
};

}  // namespace fast_chess
