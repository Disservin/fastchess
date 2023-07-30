#include <cli.hpp>

#include <filesystem>

#include <logger.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>

namespace fast_chess::cmd {
using json = nlohmann::json;

namespace EngineParser {
TimeControl parseTc(const std::string &tcString) {
    if (tcString == "infinite" || tcString == "inf") {
        return TimeControl{.time = 0};
    }

    TimeControl tc;

    std::string remainingStringVector = tcString;
    const bool has_moves = str_utils::contains(tcString, "/");
    const bool has_inc = str_utils::contains(tcString, "+");

    if (has_moves) {
        const auto moves = str_utils::splitString(tcString, '/');
        tc.moves = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc) {
        const auto moves = str_utils::splitString(remainingStringVector, '+');
        tc.increment = static_cast<uint64_t>(std::stod(moves[1]) * 1000);
        remainingStringVector = moves[0];
    }

    tc.time = static_cast<int64_t>(std::stod(remainingStringVector) * 1000);

    return tc;
}

bool isEngineSettableOption(const std::string &stringFormat) {
    return str_utils::startsWith(stringFormat, "option.");
}

void parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                          const std::string &value) {
    if (key == "cmd")
        engineConfig.cmd = value;
    else if (key == "name")
        engineConfig.name = value;
    else if (key == "tc")
        engineConfig.limit.tc = parseTc(value);
    else if (key == "st")
        engineConfig.limit.tc.fixed_time = static_cast<int64_t>(std::stod(value) * 1000);
    else if (key == "nodes")
        engineConfig.limit.nodes = std::stoll(value);
    else if (key == "plies")
        engineConfig.limit.plies = std::stoll(value);
    else if (key == "dir")
        engineConfig.dir = value;
    else if (isEngineSettableOption(key)) {
        // Strip option.Name of the option. Part
        const std::size_t pos = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.emplace_back(strippedKey, value);
    } else if (key == "proto") {
        // silently ignore
    } else
        OptionsParser::throwMissing("engine", key, value);
}

}  // namespace EngineParser

class Engine : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        argument_data.configs.emplace_back();

        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            EngineParser::parseEngineKeyValues(argument_data.configs.back(), key, value);
        });
    }
};

class Each : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            for (auto &config : argument_data.configs) {
                EngineParser::parseEngineKeyValues(config, key, value);
            }
        });
    }
};

class Pgnout : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        const auto originalI = i;

        try {
            parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
                if (key == "file") {
                    argument_data.game_options.pgn.file = value;
                } else if (key == "nodes") {
                    argument_data.game_options.pgn.track_nodes = true;
                } else if (key == "seldepth") {
                    argument_data.game_options.pgn.track_seldepth = true;
                } else if (key == "nps") {
                    argument_data.game_options.pgn.track_nps = true;
                } else if (key == "notation") {
                    if (value == "san") {
                        argument_data.game_options.pgn.notation = NotationType::SAN;
                    } else if (value == "lan") {
                        argument_data.game_options.pgn.notation = NotationType::LAN;
                    } else if (value == "uci") {
                        argument_data.game_options.pgn.notation = NotationType::UCI;
                    } else {
                        OptionsParser::throwMissing("pgnout notation", key, value);
                    }
                } else {
                    OptionsParser::throwMissing("pgnout", key, value);
                }
            });
        } catch (const std::exception &e) {
            i = originalI;
            // try to read as cutechess pgnout
            parseValue(i, argc, argv, argument_data.game_options.pgn.file);
        }
    }
};

class Opening : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                argument_data.game_options.opening.file = value;
                if (str_utils::endsWith(value, ".epd")) {
                    argument_data.game_options.opening.format = FormatType::EPD;
                } else if (str_utils::endsWith(value, ".pgn")) {
                    argument_data.game_options.opening.format = FormatType::PGN;
                }

                if (!std::filesystem::exists(value)) {
                    throw std::runtime_error("Opening file does not exist: " + value);
                }
            } else if (key == "format") {
                if (value == "epd") {
                    argument_data.game_options.opening.format = FormatType::EPD;
                } else if (value == "pgn") {
                    argument_data.game_options.opening.format = FormatType::PGN;
                } else {
                    OptionsParser::throwMissing("openings format", key, value);
                }
            } else if (key == "order") {
                argument_data.game_options.opening.order =
                    value == "random" ? OrderType::RANDOM : OrderType::SEQUENTIAL;
            } else if (key == "plies") {
                argument_data.game_options.opening.plies = std::stoi(value);
            } else if (key == "start") {
                argument_data.game_options.opening.start = std::stoi(value);
            } else {
                OptionsParser::throwMissing("openings", key, value);
            }
        });
    }
};

class Sprt : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (argument_data.game_options.rounds == 0) {
                argument_data.game_options.rounds = 500000;
            }

            if (key == "elo0") {
                argument_data.game_options.sprt.elo0 = std::stod(value);
            } else if (key == "elo1") {
                argument_data.game_options.sprt.elo1 = std::stod(value);
            } else if (key == "alpha") {
                argument_data.game_options.sprt.alpha = std::stod(value);
            } else if (key == "beta") {
                argument_data.game_options.sprt.beta = std::stod(value);
            } else {
                OptionsParser::throwMissing("sprt", key, value);
            }
        });
    }
};

class Draw : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            argument_data.game_options.draw.enabled = true;

            if (key == "movenumber") {
                argument_data.game_options.draw.move_number = std::stoi(value);
            } else if (key == "movecount") {
                argument_data.game_options.draw.move_count = std::stoi(value);
            } else if (key == "score") {
                argument_data.game_options.draw.score = std::stoi(value);
            } else {
                OptionsParser::throwMissing("draw", key, value);
            }
        });
    }
};

class Resign : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            argument_data.game_options.resign.enabled = true;

            if (key == "movecount") {
                argument_data.game_options.resign.move_count = std::stoi(value);
            } else if (key == "score") {
                argument_data.game_options.resign.score = std::stoi(value);
            } else {
                OptionsParser::throwMissing("resign", key, value);
            }
        });
    }
};

class Log : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                Logger::openFile(value);
            } else {
                OptionsParser::throwMissing("log", key, value);
            }
        });
    }
};

class Config : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                loadJson(argument_data, value);
            } else if (key == "discard" && value == "true") {
                Logger::cout("Discarding config file");
                argument_data.game_options = argument_data.old_game_options;
                argument_data.configs = argument_data.old_configs;
                argument_data.stats.clear();
            }
        });
    }

   private:
    static void loadJson(ArgumentData &argument_data, const std::string &filename) {
        std::cout << "Loading config file: " << filename << std::endl;
        std::ifstream f(filename);
        json jsonfile = json::parse(f);

        argument_data.old_configs = argument_data.configs;
        argument_data.old_game_options = argument_data.game_options;

        argument_data.game_options = jsonfile.get<GameManagerOptions>();

        argument_data.configs.clear();

        for (const auto &engine : jsonfile["engines"]) {
            argument_data.configs.push_back(engine.get<EngineConfiguration>());
        }

        argument_data.stats = jsonfile["stats"].get<stats_map>();
    }
};

class Report : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "penta") {
                argument_data.game_options.report_penta = value == "true";
            } else {
                OptionsParser::throwMissing("report", key, value);
            }
        });
    }
};

class Output : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "format") {
                argument_data.game_options.output = getOutputType(value);
                if (value == "cutechess") argument_data.game_options.report_penta = false;
            } else {
                OptionsParser::throwMissing("output", key, value);
            }
        });
    }
};

class Concurrency : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.concurrency);
    }
};

class Event : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        argument_data.game_options.event_name = parseUntilDash(i, argc, argv);
    }
};

class Site : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.site);
    }
};

class Games : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.games);
    }
};

class Rounds : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.rounds);
    }
};

class Ratinginterval : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.ratinginterval);
    };
};

class SRand : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseValue(i, argc, argv, argument_data.game_options.seed);
    }
};

class Version : public Option {
   public:
    void parse(int &, int, char const *[], ArgumentData &) override {
        OptionsParser::printVersion();
    }
};

class Help : public Option {
   public:
    void parse(int &, int, char const *[], ArgumentData &) override { OptionsParser::printHelp(); }
};

class Recover : public Option {
   public:
    void parse(int &, int, char const *[], ArgumentData &argument_data) override {
        argument_data.game_options.recover = true;
    }
};

class Repeat : public Option {
   public:
    void parse(int &, int, char const *[], ArgumentData &argument_data) override {
        argument_data.game_options.games = 2;
    }
};

class Variant : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        std::string val;

        parseValue(i, argc, argv, val);

        if (val == "fischerandom") argument_data.game_options.variant = VariantType::FRC;
    }
};

class Tournament : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &) override {
        std::string val;
        // Do nothing
        parseValue(i, argc, argv, val);
    }
};

/**
 * Quick cmd parser,
 * .\fast-chess.exe -quick cmd=smallbrain.exe cmd=smallbrain-2.exe book="UHO_XXL_2022_+110_+139.epd"
 */
class Quick : public Option {
   public:
    void parse(int &i, int argc, char const *argv[], ArgumentData &argument_data) override {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "cmd") {
                argument_data.configs.emplace_back();
                argument_data.configs.back().cmd = value;
                argument_data.configs.back().name = value;

                argument_data.configs.back().limit.tc.time = 10 * 1000;
                argument_data.configs.back().limit.tc.increment = 100;

                argument_data.configs.back().recover = true;

            } else if (key == "book") {
                argument_data.game_options.opening.file = value;
                argument_data.game_options.opening.order = OrderType::RANDOM;
                argument_data.game_options.opening.format = FormatType::EPD;
            } else {
                OptionsParser::throwMissing("quick", key, value);
            }
        });

        if (argument_data.configs[0].name == argument_data.configs[1].name) {
            argument_data.configs[0].name += "1";
            argument_data.configs[1].name += "2";
        }

        argument_data.game_options.games = 2;
        argument_data.game_options.rounds = 25000;

        argument_data.game_options.concurrency =
            std::max(unsigned(1), std::thread::hardware_concurrency() - 2);

        argument_data.game_options.recover = true;

        argument_data.game_options.draw.enabled = true;
        argument_data.game_options.draw.move_number = 30;
        argument_data.game_options.draw.move_count = 8;
        argument_data.game_options.draw.score = 8;
    }
};

OptionsParser::OptionsParser(int argc, char const *argv[]) {
    if (argument_data_.game_options.output == OutputType::CUTECHESS) {
        argument_data_.game_options.ratinginterval = 1;
    }

    if (argc == 1) {
        printHelp();
    }

    addOption("engine", new Engine());
    addOption("each", new Each());
    addOption("pgnout", new Pgnout());
    addOption("openings", new Opening());
    addOption("sprt", new Sprt());
    addOption("draw", new Draw());
    addOption("resign", new Resign());
    addOption("log", new Log());
    addOption("config", new Config());
    addOption("report", new Report());
    addOption("output", new Output());
    addOption("concurrency", new Concurrency());
    addOption("event", new Event());
    addOption("site", new Site());
    addOption("games", new Games());
    addOption("rounds", new Rounds());
    addOption("ratinginterval", new Ratinginterval());
    addOption("srand", new SRand());
    addOption("version", new Version());
    addOption("-version", new Version());
    addOption("v", new Version());
    addOption("-v", new Version());
    addOption("help", new Help());
    addOption("-help", new Help());
    addOption("recover", new Recover());
    addOption("repeat", new Repeat());
    addOption("variant", new Variant());
    addOption("tournament", new Tournament());
    addOption("quick", new Quick());

    parse(argc, argv);

    // apply the variant type to all configs
    for (auto &config : argument_data_.configs) {
        config.variant = argument_data_.game_options.variant;
    }
}

}  // namespace fast_chess::cmd