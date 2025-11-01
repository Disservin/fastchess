#pragma once

#include <fstream>
#include <mutex>
#include <optional>
#include <string>

#include <core/crc32.hpp>
#include <core/logger/logger.hpp>

namespace fastchess::util {

// Writes to a file in a thread safe manner.
class FileWriter {
   public:
    FileWriter(const std::string &filename, bool append = true, bool crc = false)
        : filename_(filename), calculate_crc_(crc) {
        file_.open(filename_, append ? std::ios::app | std::ios::binary : std::ios::binary);

        if (calculate_crc_) {
            initCrc();
        }
    }

    void write(const std::string &data) {
        std::lock_guard<std::mutex> lock(file_mutex_);

        file_ << data << std::flush;

        if (calculate_crc_) {
            crc32_ = crc::incremental_crc32(crc32_, data);
            Logger::print("File {} has CRC32: {:#x}", filename_, getCrc32().value());
        }
    }

    std::optional<std::uint32_t> getCrc32() const noexcept {
        if (!calculate_crc_) return std::nullopt;
        return crc::finalize_crc32(crc32_);
    }

   private:
    void initCrc() {
        std::ifstream checkFile(filename_);
        bool isEmpty = checkFile.peek() == std::ifstream::traits_type::eof();
        checkFile.close();

        // if the file is empty, we start with the initial crc32 value
        if (isEmpty) {
            crc32_ = crc::initial_crc32();
        } else {
            const auto c = crc::calculate_crc32(filename_);

            if (c.has_value()) {
                crc32_ = ~c.value();
            } else {
                crc32_ = crc::initial_crc32();
            }
        }
    }
    std::ofstream file_;
    std::mutex file_mutex_;
    std::uint32_t crc32_;

    const std::string filename_;
    const bool calculate_crc_;
};

}  // namespace fastchess::util
