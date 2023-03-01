#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "engine_config.h"

namespace CMD
{

struct Parameter
{
    std::string longName;
    std::string shortName;
    std::string defaultValue;
    std::string minLimit;
    std::string maxLimit;
};

struct OpeningOptions
{
    std::string file;
    // TODO use enums for this
    std::string format;
    std::string order;
    int plies = 0;
    int start = 0;
};

struct PgnOptions
{
    std::string file;
    bool trackNodes = false;
};

struct SprtOptions
{
    double alpha = 0.0;
    double beta = 0.0;
    double elo0 = 0.0;
    double elo1 = 0.0;
};

struct DrawAdjudication
{
    bool enabled = false;
    int moveNumber = 0;
    int moveCount = 0;
    int score = 0;
};

struct ResignAdjudication
{
    bool enabled = false;
    int moveCount = 0;
    int score = 0;
};

struct GameManagerOptions
{
    int games = 2;
    int rounds = 0;
    bool recover = false;
    bool repeat = false;
    int concurrency = 1;
    int ratinginterval = 10;
    SprtOptions sprt = {};
    std::string eventName = {};
    OpeningOptions opening = {};
    PgnOptions pgn = {};
    DrawAdjudication draw = {};
    ResignAdjudication resign = {};
};

inline std::ostream &operator<<(std::ostream &os, const Parameter param)
{
    os << "longName" << param.longName << "shortName" << param.shortName << "default" << param.defaultValue << "min"
       << param.minLimit << "max" << param.maxLimit;

    return os;
}

class Options
{
  public:
    Options(int argc, char const *argv[]);

    std::vector<EngineConfiguration> getEngineConfigs() const;

    GameManagerOptions getGameOptions() const;

    static bool startsWith(std::string_view haystack, std::string_view needle);

    static bool contains(std::string_view haystack, std::string_view needle);

    static bool contains(const std::vector<std::string> &haystack, std::string_view needle);

    static std::vector<std::string> splitString(const std::string &string, const char &delimiter);

  private:
    // Holds all the relevant settings for the handling of the games
    GameManagerOptions gameOptions;

    // Holds all the engines with their options
    std::vector<EngineConfiguration> configs;

    bool isEngineSettableOption(std::string stringFormat) const;

    TimeControl parseTc(const std::string tcString);

    // Generic function to parse option
    template <typename T> void parseOption(int &i, int argc, const char *argv[], T &optionValue);

    void parseEachOptions(int &i, int argc, char const *argv[]);
    void parseDrawOptions(int &i, int argc, char const *argv[]);
    void parseResignOptions(int &i, int argc, char const *argv[]);
    void parseOpeningOptions(int &i, int argc, char const *argv[]);
    void parsePgnOptions(int &i, int argc, char const *argv[]);
    void parseSprt(int &i, int argc, char const *argv[]);
    void parseEngineParams(int &i, int argc, char const *argv[], EngineConfiguration &engineParams);
    void printVersion(int &i);
};

} // namespace CMD