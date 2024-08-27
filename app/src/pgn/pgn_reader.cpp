#include <pgn/pgn_reader.hpp>

#include <iostream>
#include <memory>

#include <chess.hpp>

namespace fastchess::pgn {

class PGNVisitor : public chess::pgn::Visitor {
   public:
    PGNVisitor(std::vector<Opening>& pgns, int plies_limit) : pgns_(pgns), plies_limit_(plies_limit) {}
    virtual ~PGNVisitor() {}

    void startPgn() {
        board_.setFen(chess::constants::STARTPOS);

        pgn_.fen = chess::constants::STARTPOS;
        pgn_.moves.clear();
        plie_count_ = 0;
        early_stop_ = false;
    }

    void header(std::string_view key, std::string_view value) {
        if (key == "FEN") {
            board_.setFen(value);
            pgn_.fen = value;
        }
    }

    void startMoves() {}

    void move(std::string_view move, std::string_view) {
        if (early_stop_) return;
        if (plie_count_++ >= plies_limit_ && plies_limit_ != -1) return;

        chess::Move move_i;

        try {
            move_i = chess::uci::parseSan(board_, move);
        } catch (const chess::uci::SanParseError& e) {
            std::cerr << e.what() << '\n';
            early_stop_ = true;
            return;
        }

        board_.makeMove<true>(move_i);

        if (board_.isGameOver().second != chess::GameResult::NONE) {
            early_stop_ = true;
            return;
        }

        pgn_.moves.push_back(move_i);
    }

    void endPgn() {
        pgn_.stm = board_.sideToMove();
        pgns_.push_back(pgn_);
    }

   private:
    std::vector<pgn::Opening>& pgns_;
    pgn::Opening pgn_;
    chess::Board board_;
    const int plies_limit_;
    int plie_count_  = 0;
    bool early_stop_ = false;
};

PgnReader::PgnReader(const std::string& pgn_file_path, int plies_limit) : plies_limit_(plies_limit) {
    pgn_file_.open(pgn_file_path);
}

std::vector<pgn::Opening> PgnReader::getOpenings() { return analyseFile(); }

std::vector<pgn::Opening> PgnReader::analyseFile() {
    std::vector<pgn::Opening> pgns_;

    auto vis = std::make_unique<PGNVisitor>(pgns_, plies_limit_);
    chess::pgn::StreamParser parser(pgn_file_);
    parser.readGames(*vis);

    return pgns_;
}

}  // namespace fastchess::pgn
