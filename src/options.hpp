#pragma once

#include <iostream>
#include <string>
#include <vector>

#include "engines/engine_config.hpp"
#include "logger.hpp"
#include "yaml.hpp"

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
    std::string notation = "san";
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
    int moveNumber = 0;
    int moveCount = 0;
    int score = 0;

    bool enabled = false;
};

struct ResignAdjudication
{
    int moveCount = 0;
    int score = 0;

    bool enabled = false;
};

struct SavedData
{
    int wins = 0;
    int draws = 0;
    int losses = 0;

    int pentaWW = 0;
    int pentaWD = 0;
    int pentaWL = 0;
    int pentaLD = 0;
    int pentaLL = 0;
};

struct GameManagerOptions
{
    ResignAdjudication resign = {};
    DrawAdjudication draw = {};

    OpeningOptions opening = {};
    PgnOptions pgn = {};

    SprtOptions sprt = {};

    SavedData save = {};

    std::string eventName = "?";
    std::string site = "?";

    uint32_t seed = 951356066;

    int ratinginterval = 10;

    int games = 2;
    int rounds = 0;

    int concurrency = 1;

    int overhead = 0;

    bool recover = false;
};

inline std::ostream &operator<<(std::ostream &os, const Parameter &param)
{
    os << "longName" << param.longName << "shortName" << param.shortName << "default"
       << param.defaultValue << "min" << param.minLimit << "max" << param.maxLimit;

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

    bool isEngineSettableOption(const std::string &stringFormat) const;

    TimeControl parseTc(const std::string &tcString);

    // Generic function to parse option
    template <typename T> void parseOption(int &i, int argc, const char *argv[], T &optionValue);

    void parseDrawOptions(int &i, int argc, char const *argv[]);

    void parseResignOptions(int &i, int argc, char const *argv[]);

    void parseOpeningOptions(int &i, int argc, char const *argv[]);

    void parsePgnOptions(int &i, int argc, char const *argv[]);

    void parseSprt(int &i, int argc, char const *argv[]);

    void printVersion(int &i);

    void parseLog(int &i, int argc, const char *argv[]);

    void parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                              const std::string &value);

    void parseEachOptions(int &i, int argc, char const *argv[]);

    void parseEngineParams(int &i, int argc, char const *argv[], EngineConfiguration &engineParams);

    void parseYaml(Yaml &yaml);

    void parseYamlSprt(Yaml &yaml);

    void parseYamlOpening(Yaml &yaml);

    void parseYamlDraw(Yaml &yaml);

    void parseYamlResign(Yaml &yaml);
};

} // namespace CMD
