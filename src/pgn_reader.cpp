#include <pgn_reader.hpp>

#include <iostream>

namespace fast_chess {

PgnReader::PgnReader(const std::string& pgn_file_path) {
    pgn_file_.open(pgn_file_path);
    analyseFile();
}

std::vector<Opening> PgnReader::getPgns() const { return pgns_; }

void PgnReader::analyseFile() {
    std::string line;

    while (true) {
        auto game = chess::pgn::readGame(pgn_file_);

        if (!game.has_value()) {
            break;
        }

        Opening pgn;

        if (game->headers().find("FEN") != game->headers().end()) {
            pgn.fen = game->headers().at("FEN");
        } else {
            pgn.fen = chess::STARTPOS;
        }

        for (const auto& move : game->moves()) {
            pgn.moves.push_back(move.move);
        }

        pgns_.push_back(pgn);
    }
}

}  // namespace fast_chess