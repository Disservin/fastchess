#pragma once

namespace fast_chess {

enum class NotationType { SAN, LAN, UCI };
enum class OrderType { RANDOM, SEQUENTIAL, NONE };
enum class FormatType { EPD, PGN, NONE };
enum class VariantType { STANDARD, FRC };
enum class OutputType {
    FASTCHESS,
    CUTECHESS,
};

}  // namespace fast_chess
