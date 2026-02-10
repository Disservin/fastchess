#pragma once

#include <string_view>
#include <unordered_map>

namespace fastchess::pgn {
/*
 * Data source:
 *   https://github.com/lichess-org/chess-openings
 *
 * License:
 *   CC0 1.0 Universal
 *   https://creativecommons.org/publicdomain/zero/1.0/
 *
 * The following data was adapted from the Lichess "chess-openings" project.
 * As stated in their repository, the content is released under the CC0 license,
 * meaning it is dedicated to the public domain and may be freely used, modified,
 * and distributed without restriction.
 */

struct Opening {
    std::string_view eco;
    std::string_view name;
};

extern std::unordered_map<std::string_view, Opening> EPD_TO_OPENING;
}  // namespace fastchess::pgn