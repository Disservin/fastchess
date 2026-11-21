#include <game/book/pgn_reader.hpp>

#include <fstream>
#include <iostream>
#include <memory>

#include <chess.hpp>

#include <core/logger/logger.hpp>
#include <game/book/opening.hpp>
#include <types/exception.hpp>

#ifdef USE_ZLIB
#    include <gzip/gzstream.h>
#endif

namespace fastchess::book {

class PGNVisitor : public chess::pgn::Visitor {
   public:
    PGNVisitor(std::vector<Opening>& pgns, int plies_limit, bool is_frc)
        : pgns_(pgns), plies_limit_(plies_limit), is_frc_(is_frc) {
        board_.set960(is_frc_);
    }
    virtual ~PGNVisitor() {}

    void startPgn() override {
        board_.setFen(chess::constants::STARTPOS);

        pgn_.fen_epd = chess::constants::STARTPOS;
        pgn_.moves.clear();
        ply_count_  = 0;
        early_stop_ = false;
        invalid_    = false;
    }

    void header(std::string_view key, std::string_view value) override {
        if (key == "FEN") {
            pgn_.fen_epd = value;

            // a game from a position that can't be played is left out
            if (!setPlayablePosition(board_, value)) {
                LOG_WARN("Invalid FEN in opening book, game left out: {}", value);
                invalid_ = true;
                skipPgn(true);
            }
        }
    }

    void startMoves() override {}

    void move(std::string_view move, std::string_view) override {
        // a comment before the first move comes without a move
        if (move.empty()) return;

        if (early_stop_) return;
        if (ply_count_++ >= plies_limit_ && plies_limit_ != -1) return;

        chess::Move move_i;

        try {
            move_i = chess::uci::parseSan(board_, move);
        } catch (const chess::uci::SanParseError& e) {
            LOG_ERR("Failed to parse move: {}", e.what());
            early_stop_ = true;
            return;
        } catch (const chess::uci::AmbiguousMoveError& e) {
            LOG_ERR("Failed to parse move: {}", e.what());
            early_stop_ = true;
            return;
        }

        if (move_i == chess::Move::NO_MOVE) {
            LOG_ERR("Failed to parse move: {}", move);
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

    void endPgn() override {
        if (invalid_) {
            skipped_++;
            return;
        }

        pgns_.push_back(pgn_);
    }

    [[nodiscard]] std::size_t skipped() const noexcept { return skipped_; }

   private:
    std::vector<Opening>& pgns_;
    Opening pgn_;
    chess::Board board_;
    const int plies_limit_;
    int ply_count_       = 0;
    bool is_frc_         = false;
    bool early_stop_     = false;
    bool invalid_        = false;
    std::size_t skipped_ = 0;
};

PgnReader::PgnReader(const std::string& pgn_file_path, int plies_limit, bool is_frc)
    : file_name_(pgn_file_path), plies_limit_(plies_limit) {
    auto vis        = std::make_unique<PGNVisitor>(pgns_, plies_limit_, is_frc);
    bool is_gzipped = file_name_.size() >= 3 && file_name_.substr(file_name_.size() - 3) == ".gz";

    std::unique_ptr<std::istream> input_stream;

    if (is_gzipped) {
#ifdef USE_ZLIB
        input_stream = std::make_unique<igzstream>(file_name_.c_str());

        if (dynamic_cast<igzstream*>(input_stream.get())->rdbuf()->is_open() == false) {
            throw fastchess_exception::format("Failed to open file: {}", file_name_);
        }
#else
        throw fastchess_exception("Compressed book is provided but program wasn't compiled with zlib.");
#endif
    } else {
        input_stream = std::make_unique<std::ifstream>(file_name_);

        if (dynamic_cast<std::ifstream*>(input_stream.get())->is_open() == false) {
            throw fastchess_exception::format("Failed to open file: {}", file_name_);
        }
    }

    chess::pgn::StreamParser parser(*input_stream);
    parser.readGames(*vis);

    skipped_ = vis->skipped();

    if (skipped_ > 0) {
        Logger::print<Logger::Level::WARN>("Warning; Skipped {} opening(s) with an invalid FEN in file: {}", skipped_,
                                           file_name_);
    }

    if (pgns_.empty()) {
        throw fastchess_exception::format("No openings found in file: {}", file_name_);
    }
}

}  // namespace fastchess::book
