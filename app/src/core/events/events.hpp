#pragma once

#include <string>

#include <json.hpp>

namespace fastchess::events {

using json = nlohmann::ordered_json;

// Bump when a field is removed or renamed. Adding fields keeps the schema.
inline constexpr int SCHEMA = 1;

// Writes one JSON object per line (NDJSON). Events are queued by the game
// threads and written + flushed by a dedicated writer thread, so a slow
// reader never stalls a game.
class EventWriter {
   public:
    // Opens the output, "-" means stdout. Throws if the file can't be opened.
    static void open(const std::string& file);

    [[nodiscard]] static bool enabled() noexcept;

    // Queue an event. The object must have "event" as its first key.
    static void emit(const json& event);

    // Writes all queued events and stops the writer thread.
    static void close();

    // Remember why the tournament ended (e.g. "sprt_h1"), the first reason wins.
    static void setFinishReason(const std::string& reason);
    [[nodiscard]] static std::string finishReason();

    // Emits tournament_finished (at most once) and closes the stream.
    static void finishTournament(const std::string& reason, bool resumable);
};

}  // namespace fastchess::events
