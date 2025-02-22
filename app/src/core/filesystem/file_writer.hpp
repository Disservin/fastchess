#pragma once

#include <fstream>
#include <mutex>
#include <string>

#include <core/crc32.hpp>

namespace fastchess::util {

// Writes to a file in a thread safe manner.
class FileWriter {
   public:
    FileWriter(const std::string &filename) {
        std::ifstream checkFile(filename);
        bool isEmpty = checkFile.peek() == std::ifstream::traits_type::eof();
        checkFile.close();

        file_.open(filename, std::ios::app);

        // if the file is empty, we start with the initial crc32 value
        if (isEmpty) {
            crc32_ = crc::initial_crc32();
        } else {
            const auto c = crc::calculate_crc32(filename);

            if (c.has_value()) {
                crc32_ = ~c.value();
            } else {
                crc32_ = crc::initial_crc32();
            }
        }
    }

    void write(const std::string &data) {
        crc32_ = crc::incremental_crc32(crc32_, data);

        std::lock_guard<std::mutex> lock(file_mutex_);

        file_ << data;
    }

    std::uint32_t getCrc32() const noexcept { return crc::finalize_crc32(crc32_); }

   private:
    std::ofstream file_;
    std::mutex file_mutex_;

    std::uint32_t crc32_;
};

}  // namespace fastchess::util
