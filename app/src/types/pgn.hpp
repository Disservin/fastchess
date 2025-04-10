#pragma once

#include <string>

#include <core/helper.hpp>
#include <types/enums.hpp>

namespace fastchess::config {

struct Pgn {
    std::vector<std::string> additional_lines_rgx;
    std::string event_name = "Fastchess Tournament";
    std::string site       = "?";

    std::string file;
    NotationType notation = NotationType::SAN;
    bool track_nodes      = false;
    bool track_seldepth   = false;
    bool track_nps        = false;
    bool track_hashfull   = false;
    bool track_tbhits     = false;
    bool track_timeleft   = false;
    bool track_latency    = false;
    bool min              = false;
    bool crc              = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Pgn, additional_lines_rgx, event_name, site, file, notation, track_nodes,
                                   track_seldepth, track_nps, track_hashfull, track_tbhits, track_timeleft,
                                   track_latency, min, crc)

}  // namespace fastchess::config
