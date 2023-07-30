#pragma once

namespace fast_chess {
enum class NotationType { SAN, LAN, UCI };
enum class OrderType { RANDOM, SEQUENTIAL };
enum class FormatType { EPD, PGN, NONE };
enum class VariantType { STANDARD, FRC };
}  // namespace fast_chess