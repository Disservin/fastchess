#include <pgn_reader.hpp>

#include <iostream>
#include <regex>

#include <helper.hpp>

namespace fast_chess {

std::istream& safeGetline(std::istream& is, std::string& t);

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
    while (!safeGetline(pgn_file_, line).eof()) {
        if (line[0] == '[') {
            // we have a header
            in_header = true;
            if (str_utils::contains(str_utils::toLower(line), "fen")) {
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

// https://stackoverflow.com/questions/6089231/getting-std-ifstream-to-handle-lf-cr-and-crlf
std::istream& safeGetline(std::istream& is, std::string& t) {
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
            case '\n':
                return is;
            case '\r':
                if (sb->sgetc() == '\n') sb->sbumpc();
                return is;
            case std::streambuf::traits_type::eof():
                // Also handle the case when the last line has no line ending
                if (t.empty()) is.setstate(std::ios::eofbit);
                return is;
            default:
                t += (char)c;
        }
    }
}
}  // namespace fast_chess