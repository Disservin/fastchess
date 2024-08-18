#pragma once

namespace fastchess {

enum class NotationType { SAN, LAN, UCI };
enum class OrderType { RANDOM, SEQUENTIAL };
enum class FormatType { EPD, PGN, NONE };
enum class VariantType { STANDARD, FRC };
enum class OutputType {
    FASTCHESS,
    CUTECHESS,
};

}  // namespace fastchess