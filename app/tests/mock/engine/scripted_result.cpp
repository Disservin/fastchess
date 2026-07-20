#include <iostream>
#include <string>
#include <thread>

#include "../../../third_party/chess.hpp"

namespace {

bool contains(std::string_view haystack, std::string_view needle) { return haystack.find(needle) != std::string::npos; }

void updatePosition(chess::Board& board, const std::string& line) {
    const auto tokens = chess::utils::splitString(line, ' ');
    if (tokens.size() < 2 || tokens[0] != "position") return;

    if (tokens[1] == "startpos") {
        board = chess::Board();

        if (tokens.size() > 2 && tokens[2] == "moves") {
            for (std::size_t i = 3; i < tokens.size(); ++i) {
                board.makeMove(chess::uci::uciToMove(board, std::string(tokens[i])));
            }
        }

        return;
    }

    if (tokens[1] != "fen") return;

    std::string fen;
    std::size_t i = 2;
    for (; i < tokens.size(); ++i) {
        if (tokens[i] == "moves") {
            ++i;
            break;
        }

        fen += std::string(tokens[i]) + " ";
    }

    board.setFen(fen);

    for (; i < tokens.size(); ++i) {
        board.makeMove(chess::uci::uciToMove(board, std::string(tokens[i])));
    }
}

std::string firstLegalMove(const chess::Board& board) {
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, board);

    return chess::uci::moveToUci(moves[0]);
}

}  // namespace

int main(int argc, char* argv[]) {
    bool play_illegal   = false;
    bool crash_on_go    = false;
    bool stall_after_go = false;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        play_illegal |= arg == "--illegal";
        crash_on_go |= arg == "--crash-on-go";
        stall_after_go |= arg == "--stall-after-go";
    }

    chess::Board board;
    bool stalled = false;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") {
            return 0;
        }

        if (line == "uci") {
            std::cout << "id name scripted_result" << std::endl;
            std::cout << "id author fastchess" << std::endl;
            std::cout << "option name Hash type spin default 16 min 1 max 33554432" << std::endl;
            std::cout << "uciok" << std::endl;
        } else if (line == "isready") {
            if (stalled) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            std::cout << "readyok" << std::endl;
        } else if (line == "ucinewgame") {
            board = chess::Board();
        } else if (contains(line, "position")) {
            updatePosition(board, line);
        } else if (contains(line, "go")) {
            if (crash_on_go) return 1;

            const auto move = play_illegal ? "a1a1" : firstLegalMove(board);
            std::cout << "info depth 1 pv " << move << " score cp 0" << std::endl;
            std::cout << "bestmove " << move << std::endl;
            stalled = stall_after_go;
        }
    }

    return 0;
}
