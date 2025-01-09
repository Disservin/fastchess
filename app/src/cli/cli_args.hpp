#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace fastchess::cli {

class Args {
   private:
    std::vector<std::string> args_;

   public:
    Args(std::initializer_list<std::string> args) : args_(args) {}
    Args(int argc, const char* argv[]) {
        args_.reserve(argc);

        for (int i = 0; i < argc; ++i) {
            args_.push_back(argv[i]);
        }
    }

    int argc() const { return static_cast<int>(args_.size()); }

    const auto& args() const { return args_; }

    const std::string& operator[](int i) const { return args_[i]; }
};

}  // namespace fastchess::cli
