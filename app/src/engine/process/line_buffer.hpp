#pragma once

#include <string>
#include <string_view>

namespace fastchess::engine::process {

// The bytes read from an engine, split into lines. What has not been taken out yet, lines that follow the one a
// read stopped at or an incomplete last line, stays here for the next read.
class LineBuffer {
   public:
    explicit LineBuffer(std::string_view delimiters) : delimiters_(delimiters) {}

    void append(std::string_view bytes) {
        data_.erase(0, pos_);
        pos_ = 0;
        data_.append(bytes);
    }

    // Moves the next complete line into line, empty lines are skipped. Returns false if there is none yet.
    [[nodiscard]] bool next(std::string& line) {
        while (true) {
            const auto end = data_.find_first_of(delimiters_, pos_);
            if (end == std::string::npos) return false;

            line.assign(data_, pos_, end - pos_);
            pos_ = end + 1;

            if (!line.empty()) return true;
        }
    }

    // Moves what is left, a last line without a delimiter, into line. Returns false if nothing is left.
    // Call it once next() has returned false.
    [[nodiscard]] bool takePartial(std::string& line) {
        line.assign(data_, pos_);
        clear();
        return !line.empty();
    }

    // Drops an incomplete last line, complete lines are kept.
    void dropPartial() {
        const auto last = data_.find_last_of(delimiters_);
        data_.erase(last == std::string::npos || last < pos_ ? pos_ : last + 1);
    }

    void clear() {
        data_.clear();
        pos_ = 0;
    }

   private:
    std::string_view delimiters_;
    std::string data_;
    // start of what has not been taken out of data_ yet
    size_t pos_ = 0;
};

}  // namespace fastchess::engine::process
