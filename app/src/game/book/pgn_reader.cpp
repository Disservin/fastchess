#include <game/book/pgn_reader.hpp>

#include <iostream>
#include <memory>

#include <chess.hpp>

#include <game/book/opening.hpp>

namespace fastchess::book {

class PGNVisitor : public chess::pgn::Visitor {
   public:
    PGNVisitor(std::vector<Opening>& pgns, int plies_limit) : pgns_(pgns), plies_limit_(plies_limit) {}
    virtual ~PGNVisitor() {}

    void startPgn() override {
        board_.setFen(chess::constants::STARTPOS);

        pgn_.fen_epd = chess::constants::STARTPOS;
        pgn_.moves.clear();
        plie_count_ = 0;
        early_stop_ = false;
    }

    void header(std::string_view key, std::string_view value) override {
        if (key == "FEN") {
            board_.setFen(value);
            pgn_.fen_epd = value;
        }
    }

    void startMoves() override {}

    void move(std::string_view move, std::string_view) override {
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

        board_.makeMove(move_i);

        if (board_.isGameOver().second != chess::GameResult::NONE) {
            early_stop_ = true;
            return;
        }

        pgn_.moves.push_back(move_i);
    }

    void endPgn() override {
        pgn_.stm = board_.sideToMove();
        pgns_.push_back(pgn_);
    }

   private:
    std::vector<Opening>& pgns_;
    Opening pgn_;
    chess::Board board_;
    const int plies_limit_;
    int plie_count_  = 0;
    bool early_stop_ = false;
};

PgnReader::PgnReader(const std::string& pgn_file_path, int plies_limit)
    : file_name_(pgn_file_path), plies_limit_(plies_limit) {
    std::ifstream file(file_name_);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + file_name_);
    }

    auto vis = std::make_unique<PGNVisitor>(pgns_, plies_limit_);
    chess::pgn::StreamParser parser(file);
    parser.readGames(*vis);

    if (pgns_.empty()) {
        throw std::runtime_error("No openings found in file: " + file_name_);
    }
}

}  // namespace fastchess::book
