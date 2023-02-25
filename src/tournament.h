#pragma once

#include <utility>
#include <vector>

#include "board.h"
#include "engine_config.h"
#include "options.h"
#include "threadpool.h"
#include "uci_engine.h"

struct Match
{
    std::vector<Move> moves;
    EngineConfiguration whiteEngine;
    EngineConfiguration blackEngine;
    GameResult result;
    std::string termination;
    std::string startTime;
    std::string endTime;
    std::string duration;
    std::string date;
    Board board;
    int round;
};

class Tournament
{
  public:
    Tournament() = default;
    Tournament(bool saveTime) : saveTimeHeader(saveTime){};
    Tournament(const CMD::GameManagerOptions &mc);

    void loadConfig(const CMD::GameManagerOptions &mc);
    void startTournament(std::vector<EngineConfiguration> configs);

    Match startMatch(UciEngine &engine1, UciEngine &engine2, int round, std::string openingFen);
    std::vector<Match> runH2H(CMD::GameManagerOptions localMatchConfig, std::vector<EngineConfiguration> configs,
                              int gameId, std::string fen);

    std::string fetchNextFen();

    std::vector<std::string> getPGNS() const;

    template <typename T> static T findElement(const std::vector<std::string> &haystack, std::string_view needle)
    {
        int index = std::find(haystack.begin(), haystack.end(), needle) - haystack.begin();
        if constexpr (std::is_same_v<T, int>)
            return std::stoi(haystack[index + 1]);
        else
            return haystack[index + 1];
    }

  private:
    CMD::GameManagerOptions matchConfig;

    ThreadPool pool = ThreadPool(1);

    std::string getDateTime(std::string format = "%Y-%m-%dT%H:%M:%S %z");
    std::string formatDuration(std::chrono::seconds duration);

    std::vector<std::string> pgns;
    std::vector<std::string> openingBook;

    size_t startIndex = 0;

    bool saveTimeHeader = true;
};