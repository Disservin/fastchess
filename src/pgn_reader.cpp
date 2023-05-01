#include <pgn_reader.hpp>

#include <iostream>
#include <regex>

#include <helper.hpp>

namespace fast_chess {

PgnReader::PgnReader(const std::string& pgn_file_path) {
    // first we divide the file into single pgn objects
    // then we analyse each pgn object and extract the fen and moves

    pgn_file_.open(pgn_file_path);
    analyseFile();
}

std::vector<Opening> PgnReader::getPgns() const { return pgns_; }

std::string PgnReader::extractHeader(const std::string& line) {
    bool in_value = false;
    std::string header;
    for (std::size_t i = 1; i < line.size(); ++i) {
        if (line[i] == '"') {
            in_value = !in_value;
            continue;
        }

        if (in_value) {
            header += line[i];
        }
    }

    return header;
}

std::vector<std::string> PgnReader::extractMoves(std::string line) {
    // Define the regular expression
    std::regex pattern(
        "(?:[PNBRQK]?[a-h]?[1-8]?x?[a-h][1-8](?:\\=[PNBRQK])?|O(-?O){1,2})[\\+#]?(\\s*[\\!\\?]+)?");

    // Use std::regex_search to check if the input matches the pattern
    std::smatch match;

    std::vector<std::string> moves;

    while (std::regex_search(line, match, pattern)) {
        moves.push_back(match.str());

        // Remove the matched substring from the input
        line = match.suffix().str();
    }

    return moves;
}

void PgnReader::analyseFile() {
    // read the file line by line
    // if the line starts with a [ then we have a header

    bool in_header = true;

    Opening pgn;

    std::string line;
    while (std::getline(pgn_file_, line)) {
        if (line[0] == '[') {
            // we have a header
            in_header = true;
            if (StrUtil::contains(StrUtil::toLower(line), "fen")) {
                pgn.fen = extractHeader(line);
            }
        } else if (line.empty() && in_header) {
            in_header = false;
        } else if (line.empty() && !in_header) {
            // we have a new pgn object
            pgns_.push_back(pgn);
            pgn = Opening();
        } else if (!in_header) {
            if (line.empty()) {
                continue;
            }

            const auto moves = extractMoves(line);
            // add the vectors together
            pgn.moves.insert(pgn.moves.end(), moves.begin(), moves.end());
        }
    }
    pgns_.push_back(pgn);
}
}  // namespace fast_chess