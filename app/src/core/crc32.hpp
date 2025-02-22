#pragma once

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>

namespace fastchess::crc {

constexpr std::array<std::uint32_t, 256> generate_crc_table() {
    std::array<std::uint32_t, 256> table{};

    for (std::uint32_t i = 0; i < 256; i++) {
        std::uint32_t crc = i;

        for (std::uint32_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 * (crc & 1));
        }

        table[i] = crc;
    }

    return table;
}

static constexpr auto crc_table = generate_crc_table();

inline std::optional<std::uint32_t> calculate_crc32(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);

    if (!file.is_open()) {
        return std::nullopt;
    }

    std::uint32_t crc = 0xFFFFFFFF;
    char buffer[4096];

    do {
        file.read(buffer, sizeof(buffer));
        size_t count = file.gcount();
        for (size_t i = 0; i < count; i++) {
            crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ static_cast<unsigned char>(buffer[i])];
        }
    } while (file);

    return ~crc;
}

inline std::uint32_t initial_crc32() { return 0xFFFFFFFF; }

/**
 * @brief incremental crc32 calculation
 * @param crc
 * @param input
 * @return
 */
inline std::uint32_t incremental_crc32(std::uint32_t crc, const std::string& input) {
    for (unsigned char c : input) {
        crc = (crc >> 8) ^ crc_table[(crc & 0xFF) ^ c];
    }

    return crc;
}

inline std::uint32_t finalize_crc32(std::uint32_t crc) { return ~crc; }

}  // namespace fastchess::crc
