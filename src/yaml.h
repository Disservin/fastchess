#pragma once

#include <string>
#include <map>
#include <cassert>
#include <iostream>
#include <utility>

class Yaml {
  private:
    std::map<std::string, Yaml> map;

  public:
    std::string value;
    Yaml *parent = nullptr;

    Yaml() = default;
    explicit Yaml(std::string buffer, Yaml *p) : value(std::move(buffer)), parent(p) {}

    Yaml& operator[](std::string x) {
        return map[x];
    }

    void loadFile(const std::string &filename);

    template <typename T>
    T get()
    {
        assert(!value.empty());

        if constexpr (std::is_same_v<T, int>)
            return std::stoi(value);
        else if constexpr (std::is_same_v<T, float>)
            return std::stof(value);
        else if constexpr (std::is_same_v<T, double>)
            return std::stod(value);
        else if constexpr (std::is_same_v<T, bool>)
            return value == "true";
        else
            return value;
    }
};

const std::string defaultYaml = "# Command line arguments have a higher priority.\n"
                                "# Please do not try to change the structure of this file!\n"
                                "\n"
                                "# Name of the event\n"
                                "event: Event\n"
                                "\n"
                                "# Total rounds\n"
                                "# Set to 0 for an infinite tournament\n"
                                "rounds: 10000\n"
                                "\n"
                                "# Games per rounds\n"
                                "games: 2\n"
                                "\n"
                                "# True if the engines are able to recover from crashes\n"
                                "recover: false\n"
                                "\n"
                                "# Number of threads\n"
                                "concurrency: 1\n"
                                "\n"
                                "# Number of games between elo estimations\n"
                                "rating-interval: 10\n"
                                "\n"
                                "# Opening book\n"
                                "openings:\n"
                                "  enabled: false\n"
                                "\n"
                                "  # Path to the opening book\n"
                                "  file: \"openings.epd\"\n"
                                "\n"
                                "  # Format of the opening book\n"
                                "  # Only EPD is supported!\n"
                                "  format: epd\n"
                                "\n"
                                "  # Order on how the openings should be selected\n"
                                "  # Possible options are sequential and random\n"
                                "  order: random\n"
                                "\n"
                                "  # Specifies the offset\n"
                                "  start: 0\n"
                                "\n"
                                "  plies: 0\n"
                                "\n"
                                "# Game adjudication\n"
                                "adjudication:\n"
                                "\n"
                                "  # Resign if movecount consecutive moves are played with score\n"
                                "  resign:\n"
                                "    enabled: false\n"
                                "    movecount: 3\n"
                                "    score: 1000\n"
                                "\n"
                                "  # Draw if movecount consecutive moves are played with absolute scores less than score.\n"
                                "  draw:\n"
                                "    enabled: false\n"
                                "    movecount: 5\n"
                                "    movenumber: 40\n"
                                "    score: 5\n"
                                "\n"
                                "# SPRT\n"
                                "sprt:\n"
                                "  enabled: false\n"
                                "  elo0: 0.0\n"
                                "  elo1: 5.0\n"
                                "  alpha: 0.05\n"
                                "  beta: 0.05";