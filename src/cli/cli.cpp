#include <cli/cli.hpp>

#include <filesystem>

#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>
#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>
#include <util/logger/logger.hpp>

namespace fast_chess::cli {
using json = nlohmann::json;

/// @brief Parse -name key=value key=value
/// @param i
/// @param argc
/// @param argv
/// @param func
void parseDashOptions(int &i, int argc, char const *argv[],
                      const std::function<void(std::string, std::string)> &func) {
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        std::string param = argv[i];
        std::size_t pos   = param.find('=');
        std::string key   = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        func(key, value);
    }
}

/// @brief Parse a standalone value after a dash command. i.e -concurrency 10
/// @tparam T
/// @param i
/// @param argc
/// @param argv
/// @param optionValue
template <typename T>
void parseValue(int &i, int argc, const char *argv[], T &optionValue) {
    i++;
    if (i < argc && argv[i][0] != '-') {
        if constexpr (std::is_same_v<T, int>)
            optionValue = std::stoi(argv[i]);
        else if constexpr (std::is_same_v<T, uint32_t>)
            optionValue = std::stoul(argv[i]);
        else if constexpr (std::is_same_v<T, float>)
            optionValue = std::stof(argv[i]);
        else if constexpr (std::is_same_v<T, double>)
            optionValue = std::stod(argv[i]);
        else if constexpr (std::is_same_v<T, bool>)
            optionValue = std::string(argv[i]) == "true";
        else
            optionValue = argv[i];
    }
}

/// @brief Reads the entire line until a dash is found
/// @param i
/// @param argc
/// @param argv
/// @return
[[nodiscard]] inline std::string readUntilDash(int &i, int argc, char const *argv[]) {
    std::string result;
    while (i + 1 < argc && argv[i + 1][0] != '-' && i++) {
        result += argv[i] + std::string(" ");
    }
    return result.erase(result.size() - 1);
}

namespace engine {
TimeControl parseTc(const std::string &tcString) {
    if (tcString == "infinite" || tcString == "inf") {
        return {};
    }

    TimeControl tc;

    std::string remainingStringVector = tcString;
    const bool has_moves              = str_utils::contains(tcString, "/");
    const bool has_inc                = str_utils::contains(tcString, "+");

    if (has_moves) {
        const auto moves      = str_utils::splitString(tcString, '/');
        tc.moves              = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc) {
        const auto moves      = str_utils::splitString(remainingStringVector, '+');
        tc.increment          = static_cast<uint64_t>(std::stod(moves[1]) * 1000);
        remainingStringVector = moves[0];
    }

    tc.time = static_cast<int64_t>(std::stod(remainingStringVector) * 1000);

    return tc;
}

bool isEngineSettableOption(std::string_view stringFormat) {
    return str_utils::startsWith(stringFormat, "option.");
}

void parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                          const std::string &value) {
    if (key == "cmd") {
        engineConfig.cmd = value;
    } else if (key == "name")
        engineConfig.name = value;
    else if (key == "tc")
        engineConfig.limit.tc = parseTc(value);
    else if (key == "st")
        engineConfig.limit.tc.fixed_time = static_cast<int64_t>(std::stod(value) * 1000);
    else if (key == "nodes")
        engineConfig.limit.nodes = std::stoll(value);
    else if (key == "plies" || key == "depth")
        engineConfig.limit.plies = std::stoll(value);
    else if (key == "dir")
        engineConfig.dir = value;
    else if (key == "args")
        engineConfig.args = value;
    else if (isEngineSettableOption(key)) {
        // Strip option.Name of the option. Part
        const std::size_t pos         = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.emplace_back(strippedKey, value);
    } else if (key == "proto") {
        // silently ignore
    } else
        OptionsParser::throwMissing("engine", key, value);
}

void validateEnginePath(std::string dir, std::string &cmd) {
    // engine path with dir
    dir              = dir == "." ? "" : dir;
    auto engine_path = dir + cmd;

// append .exe to cmd if it is missing on windows
#ifdef _WIN64
    if (engine_path.find(".exe") == std::string::npos) {
        cmd += ".exe";
        engine_path += ".exe";
    }
#endif

    // throw if cmd does not exist
    if (!std::filesystem::exists(engine_path)) {
        throw std::runtime_error("Error; Engine not found: " + engine_path);
    }
}

void parseEngine(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    argument_data.configs.emplace_back();

    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        engine::parseEngineKeyValues(argument_data.configs.back(), key, value);
    });

    engine::validateEnginePath(argument_data.configs.back().dir, argument_data.configs.back().cmd);
}

}  // namespace engine

void parseEach(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        for (auto &config : argument_data.configs) {
            engine::parseEngineKeyValues(config, key, value);
        }
    });
}

void parsePgnOut(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    const auto originalI = i;

    try {
        parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                argument_data.tournament_options.pgn.file = value;
            } else if (key == "nodes") {
                argument_data.tournament_options.pgn.track_nodes = true;
            } else if (key == "seldepth") {
                argument_data.tournament_options.pgn.track_seldepth = true;
            } else if (key == "nps") {
                argument_data.tournament_options.pgn.track_nps = true;
            } else if (key == "notation") {
                if (value == "san") {
                    argument_data.tournament_options.pgn.notation = NotationType::SAN;
                } else if (value == "lan") {
                    argument_data.tournament_options.pgn.notation = NotationType::LAN;
                } else if (value == "uci") {
                    argument_data.tournament_options.pgn.notation = NotationType::UCI;
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
        parseValue(i, argc, argv, argument_data.tournament_options.pgn.file);
    }
}

void parseOpening(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            argument_data.tournament_options.opening.file = value;
            if (str_utils::endsWith(value, ".epd")) {
                argument_data.tournament_options.opening.format = FormatType::EPD;
            } else if (str_utils::endsWith(value, ".pgn")) {
                argument_data.tournament_options.opening.format = FormatType::PGN;
            }

            if (!std::filesystem::exists(value)) {
                throw std::runtime_error("Opening file does not exist: " + value);
            }
        } else if (key == "format") {
            if (value == "epd") {
                argument_data.tournament_options.opening.format = FormatType::EPD;
            } else if (value == "pgn") {
                argument_data.tournament_options.opening.format = FormatType::PGN;
            } else {
                OptionsParser::throwMissing("openings format", key, value);
            }
        } else if (key == "order") {
            argument_data.tournament_options.opening.order =
                value == "random" ? OrderType::RANDOM : OrderType::SEQUENTIAL;
        } else if (key == "plies") {
            argument_data.tournament_options.opening.plies = std::stoi(value);
        } else if (key == "start") {
            argument_data.tournament_options.opening.start = std::stoi(value);
        } else {
            OptionsParser::throwMissing("openings", key, value);
        }
    });
}

void parseSprt(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (argument_data.tournament_options.rounds == 0) {
            argument_data.tournament_options.rounds = 500000;
        }

        if (key == "elo0") {
            argument_data.tournament_options.sprt.elo0 = std::stod(value);
        } else if (key == "elo1") {
            argument_data.tournament_options.sprt.elo1 = std::stod(value);
        } else if (key == "alpha") {
            argument_data.tournament_options.sprt.alpha = std::stod(value);
        } else if (key == "beta") {
            argument_data.tournament_options.sprt.beta = std::stod(value);
        } else if (key == "model") {
            if (value == "logistic") {
                argument_data.tournament_options.sprt.logistic_bounds = true;
            } else if (value == "normalized") {
                argument_data.tournament_options.sprt.logistic_bounds = false;
            } else {
                OptionsParser::throwMissing("sprt", key, value);
            }
        } else {
            OptionsParser::throwMissing("sprt", key, value);
        }
    });
}

void parseDraw(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        argument_data.tournament_options.draw.enabled = true;

        if (key == "movenumber") {
            argument_data.tournament_options.draw.move_number = std::stoi(value);
        } else if (key == "movecount") {
            argument_data.tournament_options.draw.move_count = std::stoi(value);
        } else if (key == "score") {
            if (std::stoi(value) >= 0) {
              argument_data.tournament_options.draw.score = std::stoi(value);
            } else{
              throw std::runtime_error("Score cannot be negative");
            }
        } else {
            OptionsParser::throwMissing("draw", key, value);
        }
    });
}

void parseResign(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        argument_data.tournament_options.resign.enabled = true;

        if (key == "movecount") {
            argument_data.tournament_options.resign.move_count = std::stoi(value);
        } else if (key == "score") {
            if (std::stoi(value) >= 0) {
              argument_data.tournament_options.draw.score = std::stoi(value);
            } else{
              throw std::runtime_error("Score cannot be negative");
            }
        } else {
            OptionsParser::throwMissing("resign", key, value);
        }
    });
}

void parseLog(int &i, int argc, char const *argv[], ArgumentData &) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            Logger::openFile(value);
        } else if (key == "level") {
            if (value == "trace") {
                Logger::setLevel(Logger::Level::TRACE);
            } else if (value == "warn") {
                Logger::setLevel(Logger::Level::WARN);
            } else if (value == "info") {
                Logger::setLevel(Logger::Level::INFO);
            } else if (value == "err") {
                Logger::setLevel(Logger::Level::ERR);
            } else if (value == "fatal") {
                Logger::setLevel(Logger::Level::FATAL);
            } else {
                OptionsParser::throwMissing("log level", key, value);
            }
        } else {
            OptionsParser::throwMissing("log", key, value);
        }
    });
}

namespace config {
void loadJson(ArgumentData &argument_data, const std::string &filename) {
    Logger::log<Logger::Level::INFO>("Loading config file: ", filename);
    std::ifstream f(filename);
    json jsonfile = json::parse(f);

    argument_data.old_configs            = argument_data.configs;
    argument_data.old_tournament_options = argument_data.tournament_options;

    argument_data.tournament_options = jsonfile.get<options::Tournament>();

    argument_data.configs.clear();

    for (const auto &engine : jsonfile["engines"]) {
        argument_data.configs.push_back(engine.get<EngineConfiguration>());
    }

    argument_data.stats = jsonfile["stats"].get<stats_map>();
}

void parseConfig(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            loadJson(argument_data, value);
        } else if (key == "discard" && value == "true") {
            Logger::log<Logger::Level::INFO>("Discarding config file");
            argument_data.tournament_options = argument_data.old_tournament_options;
            argument_data.configs            = argument_data.old_configs;
            argument_data.stats.clear();
        }
    });
}
}  // namespace config

void parseReport(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "penta") {
            argument_data.tournament_options.report_penta = value == "true";
        } else {
            OptionsParser::throwMissing("report", key, value);
        }
    });
}

void parseOutput(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "format") {
            argument_data.tournament_options.output = OutputFactory::getType(value);
            if (value == "cutechess") argument_data.tournament_options.report_penta = false;
        } else {
            OptionsParser::throwMissing("output", key, value);
        }
    });
}

void parseConcurrency(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.concurrency);
}

void parseEvent(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    argument_data.tournament_options.event_name = readUntilDash(i, argc, argv);
}

void parseSite(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.site);
}

void parseGames(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.games);
}

void parseRounds(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.rounds);
}

void parseRatinginterval(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.ratinginterval);
}

void parseSRand(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseValue(i, argc, argv, argument_data.tournament_options.seed);
}

void parseVersion(int &, int, char const *[], ArgumentData &) { OptionsParser::printVersion(); }

void parseHelp(int &, int, char const *[], ArgumentData &) { OptionsParser::printHelp(); }

void parseRecover(int &, int, char const *[], ArgumentData &argument_data) {
    argument_data.tournament_options.recover = true;
}

void parseRepeat(int &, int, char const *[], ArgumentData &argument_data) {
    argument_data.tournament_options.games = 2;
}

void parseVariant(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    std::string val;

    parseValue(i, argc, argv, val);

    if (val == "fischerandom") argument_data.tournament_options.variant = VariantType::FRC;
}

void parseTournament(int &i, int argc, char const *argv[], ArgumentData &) {
    std::string val;
    // Do nothing
    parseValue(i, argc, argv, val);
}

/// @brief .\fast-chess.exe -quick cmd=smallbrain.exe cmd=smallbrain-2.exe
/// book="UHO_XXL_2022_+110_+139.epd"
/// @param i
/// @param argc
/// @param argv
/// @param argument_data
void parseQuick(int &i, int argc, char const *argv[], ArgumentData &argument_data) {
    parseDashOptions(i, argc, argv, [&](const std::string &key, const std::string &value) {
        if (key == "cmd") {
            argument_data.configs.emplace_back();
            argument_data.configs.back().cmd  = value;
            argument_data.configs.back().name = value;

            argument_data.configs.back().limit.tc.time      = 10 * 1000;
            argument_data.configs.back().limit.tc.increment = 100;

            argument_data.configs.back().recover = true;

            engine::validateEnginePath(argument_data.configs.back().dir,
                                       argument_data.configs.back().cmd);
        } else if (key == "book") {
            argument_data.tournament_options.opening.file   = value;
            argument_data.tournament_options.opening.order  = OrderType::RANDOM;
            argument_data.tournament_options.opening.format = FormatType::EPD;
        } else {
            OptionsParser::throwMissing("quick", key, value);
        }
    });

    if (argument_data.configs[0].name == argument_data.configs[1].name) {
        argument_data.configs[0].name += "1";
        argument_data.configs[1].name += "2";
    }

    argument_data.tournament_options.games  = 2;
    argument_data.tournament_options.rounds = 25000;

    argument_data.tournament_options.concurrency =
        std::max(static_cast<int>(1), static_cast<int>(std::thread::hardware_concurrency() - 2));

    argument_data.tournament_options.recover = true;

    argument_data.tournament_options.draw.enabled     = true;
    argument_data.tournament_options.draw.move_number = 30;
    argument_data.tournament_options.draw.move_count  = 8;
    argument_data.tournament_options.draw.score       = 8;

    argument_data.tournament_options.output = OutputType::CUTECHESS;
}

void parseAffinity(int &, int, char const *[], ArgumentData &argument_data) {
    argument_data.tournament_options.affinity = false;
}

OptionsParser::OptionsParser(int argc, char const *argv[]) {
    if (argument_data_.tournament_options.output == OutputType::CUTECHESS) {
        argument_data_.tournament_options.ratinginterval = 1;
    }

    if (argc == 1) {
        printHelp();
    }

    addOption("engine", engine::parseEngine);
    addOption("each", parseEach);
    addOption("pgnout", parsePgnOut);
    addOption("openings", parseOpening);
    addOption("sprt", parseSprt);
    addOption("draw", parseDraw);
    addOption("resign", parseResign);
    addOption("log", parseLog);
    addOption("config", config::parseConfig);
    addOption("report", parseReport);
    addOption("output", parseOutput);
    addOption("concurrency", parseConcurrency);
    addOption("event", parseEvent);
    addOption("site", parseSite);
    addOption("games", parseGames);
    addOption("rounds", parseRounds);
    addOption("ratinginterval", parseRatinginterval);
    addOption("srand", parseSRand);
    addOption("version", parseVersion);
    addOption("-version", parseVersion);
    addOption("v", parseVersion);
    addOption("-v", parseVersion);
    addOption("help", parseHelp);
    addOption("-help", parseHelp);
    addOption("recover", parseRecover);
    addOption("repeat", parseRepeat);
    addOption("variant", parseVariant);
    addOption("tournament", parseTournament);
    addOption("quick", parseQuick);
    addOption("no-affinity", parseAffinity);

    parse(argc, argv);

    // apply the variant type to all configs
    for (auto &config : argument_data_.configs) {
        config.variant = argument_data_.tournament_options.variant;
    }
}

}  // namespace fast_chess::cli
