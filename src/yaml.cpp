#include "yaml.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <stack>

const std::string defaultYaml =
    "# Command line arguments have a higher priority.\n# Please do not try to change the structure "
    "of this file!\n\n# Name of the event\nevent: Event\n\n# Total rounds\n# Set to 0 for an "
    "infinite tournament\nrounds: 10000\n\n# Games per rounds\ngames: 2\n\n# True if the engines "
    "are able to recover from crashes\nrecover: false\n\n# Number of threads\nconcurrency: 1\n\n# "
    "Number of games between elo estimations\nrating-interval: 10\n\n# Opening book\nopenings:\n  "
    "enabled: false\n\n  # Path to the opening book\n  file: openings.epd\n\n  # Format of the "
    "opening book\n  # Only EPD is supported!\n  format: epd\n\n  # Order on how the openings "
    "should be selected\n  # Possible options are sequential and random\n  order: random\n\n  # "
    "Specifies the offset\n  start: 0\n\n  plies: 0\n\n# Game adjudication\nadjudication:\n\n  # "
    "Resign if movecount consecutive moves are played with score\n  resign:\n    enabled: false\n  "
    "  movecount: 3\n    score: 1000\n\n  # Draw if movecount consecutive moves are played with "
    "absolute scores less than score.\n  draw:\n    enabled: false\n    movecount: 5\n    "
    "movenumber: 40\n    score: 5\n\n# SPRT\nsprt:\n  enabled: false\n  elo0: 0.0\n  elo1: 5.0\n  "
    "alpha: 0.05\n  beta: 0.05\n\nsave:\n  wins: 0\n  draws: 0\n  losses: 0\n  pentaWW: 0\n  "
    "pentaWD: 0\n  pentaWL: 0\n  pentaLD: 0\n  pentaLL: 0\n";

void Yaml::loadFile(const std::string &filename)
{
    std::ifstream file;
    file.open(filename);

    if (!file.is_open())
    {
        std::cout
            << "No YAML input file was found...\nCreating default configuration at \"config.yml\"!"
            << std::endl;
        std::ofstream out;
        out.open(filename);

        out << defaultYaml;

        out.close();

        file.open(filename);
    }

    std::string line;

    Yaml *current = this;
    std::stack<int> indents;
    indents.push(0);
    while (std::getline(file, line))
    {

        std::string token;
        std::string buffer;

        bool escaped = false;
        bool string = false;
        int indent = -1;
        for (char c : line)
        {

            if (c != ' ')
            {
                indent = indent < 0 ? -indent - 1 : indent;
            }

            if (escaped || (string && c != '\"'))
            {
                buffer += c;
                escaped = false;
            }
            else if (c == '#')
            {
                break;
            }
            else if (c == ':')
            {
                token = buffer;
                buffer.clear();
            }
            else if (c == ' ')
            {
                if (indent < 0)
                    indent--;
                continue;
            }
            else if (c == '\\')
            {
                escaped = true;
            }
            else if (c == '\"')
            {
                string = !string;
            }
            else
            {
                buffer += c;
            }
        }

        // Only non-space characters
        if (indent < 0)
        {
            continue;
        }

        if (indents.top() == -1)
        {
            indents.pop();
            indents.push(indent);
        }

        while (indents.top() > indent)
        {
            current = current->parent;
            indents.pop();
        }

        if (!token.empty())
        {
            if (buffer.empty())
            {
                current->map[token] = Yaml(token, current);
                current = &current->map[token];
                indents.push(-1);
            }
            else
            {
                current->map[token] = Yaml(buffer, current);
            }
        }
    }

    file.close();
}
