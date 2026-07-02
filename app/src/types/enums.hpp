#pragma once

namespace fastchess {

enum class NotationType { SAN, LAN, UCI };
enum class OrderType { RANDOM, RANDOM_IID, SEQUENTIAL };
enum class FormatType { EPD, PGN, NONE };
enum class VariantType { STANDARD, FRC };
enum class TournamentType { ROUNDROBIN, GAUNTLET };
enum class OutputType { FASTCHESS, CUTECHESS, NONE };

}  // namespace fastchess
