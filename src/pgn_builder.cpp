#include "pgn_builder.h"

#include <sstream>

PgnBuilder::PgnBuilder(std::vector<Match> matches)
{

    // [Event "?"]
    // [Site "?"]
    // [Date "2023.01.24"]
    // [Round "1"]
    // [White ".\smallbrain.exe"]
    // [Black ".\Caissa_1.5_AVX2_BMI2.exe"]
    // [Result "*"]
    // [FEN "r1b1kbnr/1p3ppp/p2p4/q3p3/2PQP3/P1N5/1P3PPP/R1B1KB1R w KQkq - 0 1"]
    // [GameDuration "00:00:10"]
    // [GameEndTime "2023-01-24T23:06:07.769 Mitteleuropäische Zeit"]
    // [GameStartTime "2023-01-24T23:05:57.204 Mitteleuropäische Zeit"]
    // [PlyCount "37"]
    // [SetUp "1"]
    // [Termination "unterminated"]
    // [TimeControl "8+0.08"]

    for (auto match : matches)
    {
        std::stringstream ss;

        // clang-format off
        ss << "[Event " << "\"?\"" << "]" << "\n";
        ss << "[Site " << "\"?\"" << "]" << "\n";
        ss << "[Date " << "\"?\"" << "]" << "\n";
        ss << "[Round " << "\"?\"" << "]" << "\n";
        ss << "[White " << "\"?\"" << "]" << "\n";
        ss << "[Black " << "\"?\"" << "]" << "\n";
        ss << "[Result " << "\"?\"" << "]" << "\n";
        ss << "[FEN " << "\"" << match.board.getFen() << "\""<< "]" << "\n";
        ss << "[GameDuration " << "\"?\"" << "]" << "\n";
        ss << "[GameEndTime " << "\"?\"" << "]" << "\n";
        ss << "[GameStartTime " << "\"?\"" << "]" << "\n";
        ss << "[PlyCount " << "\"?\"" << "]" << "\n";
        ss << "[SetUp " << "\"?\"" << "]" << "\n";
        ss << "[Termination " << "\"?\"" << "]" << "\n";
        ss << "[TimeControl " << "\"?\"" << "]" << "\n";
        // clang-format on

        Board b = match.board;

        int i = 3;
        for (auto move : match.moves)
        {
            if (i % 2 != 0)
            {
                ss << i / 2 << "."
                   << " " << MoveToSan(b, move);
            }
            else
            {
                ss << " " << MoveToSan(b, move) << "\n";
            }
            b.makeMove(move);

            i++;
        }
        std::cout << ss.str() << std::endl;
    }
}

PgnBuilder::~PgnBuilder()
{
}
