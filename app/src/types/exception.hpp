#pragma once

#include <exception>
#include <string>


namespace fastchess {

class FastChessException : public std::exception {
   public:
    explicit FastChessException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }

   private:
    std::string message_;
};


}  // namespace fastchess