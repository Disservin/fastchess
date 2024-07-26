#pragma once

#include <string>

#include <types/enums.hpp>
#include <util/helper.hpp>

namespace mercury::config {

struct Pgn {
    std::string event_name = "Fast-Chess Tournament";
    std::string site       = "?";

    std::string file;
    NotationType notation = NotationType::SAN;
    bool track_nodes      = false;
    bool track_seldepth   = false;
    bool track_nps        = false;
    bool track_hashfull   = false;
    bool track_tbhits     = false;
    bool min              = false;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_ORDERED_JSON(Pgn, event_name, site, file, notation, track_nodes, track_seldepth,
                                                track_nps, track_hashfull, track_tbhits, min)

}  // namespace mercury::config