#pragma once

#include <string>

namespace fastchess::config {

// Machine readable event stream (-events). Intentionally not serialized into
// the config file, the stream is a property of a single run.
struct Events {
    // output path, "-" for stdout, empty when disabled
    std::string file;
    // emit throttled "thinking" events while an engine searches
    bool info = false;
    // minimum interval between two thinking events of one game, in milliseconds
    int info_interval_ms = 250;

    [[nodiscard]] bool enabled() const noexcept { return !file.empty(); }
};

}  // namespace fastchess::config
