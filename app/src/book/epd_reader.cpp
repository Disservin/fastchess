#include <book/epd_reader.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
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
}  // namespace

namespace fastchess::book {

EpdReader::EpdReader(const std::string& epd_file_path) : epd_file_(epd_file_path) {}

Openings EpdReader::getOpenings() {
    std::vector<std::string> openings;

    std::ifstream openingFile;
    openingFile.open(epd_file_);

    std::string line;
    while (safeGetline(openingFile, line))
        if (!line.empty()) openings.emplace_back(line);

    if (openings.empty()) {
        throw std::runtime_error("No openings found in file: " + epd_file_);
    }

    return Openings{openings};
}

}  // namespace fastchess::book
