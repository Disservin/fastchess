#include <matchmaking/book/opening_book.hpp>

#include <fstream>
#include <sstream>
#include <string>

#include <util/safe_getline.hpp>

namespace fast_chess {

OpeningBook::OpeningBook(const options::Opening& opening) {
    start_ = opening.start;
    setup(opening.file, opening.format);
}

// Function to split a string by space
std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

void OpeningBook::setup(const std::string& file, FormatType type) {
    if (file.empty()) {
        return;
    }

    if (type == FormatType::PGN) {
        book_ = PgnReader(file).getOpenings();

        if (std::get<pgn_book>(book_).empty()) {
            throw std::runtime_error("No openings found in PGN file: " + file);
        }
    } else if (type == FormatType::EPD) {
        std::ifstream openingFile;
        openingFile.open(file);

        std::string line;
        std::vector<std::string> epd;

        while (safeGetline(openingFile, line)) {
            epd.emplace_back(line);
        }

        openingFile.close();
        
        std::vector<std::pair<std::string, std::pair<int, int>>> fenData;
        
        for (const auto& line : epd) {
            // Find the position of the first semicolon
            size_t pos = line.find(';');
            
            // Find the position of the 4th space
            size_t posspace = std::string::npos;
            for (std::string::size_type i = 0, spaceCount = 0; i < line.size(); ++i) {
                if (line[i] == ' ') {
                    ++spaceCount;
                    if (spaceCount == 4) {
                        posspace = i;
                        break;
                    }
                }
            }
        
            // Extract first four FEN fields
            std::string first_four_fen = line.substr(0, posspace);
        
            // Default values for hmvc and fmvn
            int hmvc = 0;
            int fmvn = 1;
        
            // Extract hmvc and fmvn values before the first semicolon
            if (pos != std::string::npos) {
                std::istringstream iss(line.substr(pos + 1));  // Start from after the first semicolon
                std::string part;
                while (std::getline(iss, part, ';')) {
                    if (part.find("hmvc") != std::string::npos) {
                        std::istringstream part_iss(part.substr(part.find("hmvc") + 4));
                        part_iss >> hmvc; // Reading the integer value after "hmvc"
                    }
                    if (part.find("fmvn") != std::string::npos) {
                        std::istringstream part_iss(part.substr(part.find("fmvn") + 4));
                        part_iss >> fmvn; // Reading the integer value after "fmvn"
                    }
                }
            }
        
            // Store in the vector
            fenData.push_back({first_four_fen, {hmvc, fmvn}});
        }
        
        std::vector<std::string> fen;
        
        for (const auto& data : fenData) {
            std::ostringstream oss;
            oss << data.first << " " << data.second.first << " " << data.second.second;
            fen.push_back(oss.str());
        }
        
        book_ = fen;

        if (std::get<epd_book>(book_).empty()) {
            throw std::runtime_error("No openings found in EPD file: " + file);
        }
    }
}

Opening OpeningBook::fetch() noexcept {
    static uint64_t opening_index = 0;
    const auto idx                = start_ + opening_index++;
    const auto book_size          = std::holds_alternative<epd_book>(book_)
                                        ? std::get<epd_book>(book_).size()
                                        : std::get<pgn_book>(book_).size();

    if (book_size == 0) {
        return {chess::constants::STARTPOS, {}};
    }

    if (std::holds_alternative<epd_book>(book_)) {
        const auto fen = std::get<epd_book>(book_)[idx % std::get<epd_book>(book_).size()];
        return {fen, {}, chess::Board(fen).sideToMove()};
    } else if (std::holds_alternative<pgn_book>(book_)) {
        return std::get<pgn_book>(book_)[idx % std::get<pgn_book>(book_).size()];
    }

    return {chess::constants::STARTPOS, {}};
}

}  // namespace fast_chess
