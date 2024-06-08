#include <cli/cli.hpp>

#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/result.hpp>
#include <types/engine_config.hpp>
#include <types/tournament_options.hpp>
#include <util/file_system.hpp>
#include <util/logger/logger.hpp>

namespace {
// Parse -name key=value key=value
void parseDashOptions(const std::vector<std::string> &params,
                      const std::function<void(std::string, std::string)> &func) {
    for (const auto &param : params) {
        size_t pos = param.find('=');
        if (pos != std::string::npos) {
            std::string key   = param.substr(0, pos);
            std::string value = param.substr(pos + 1);
            func(key, value);
        } else {
            std::cerr << "Invalid parameter: " << param << std::endl;
        }
    }
}

// Parse a standalone value after a dash command. i.e -concurrency 10
template <typename T>
void parseValue(const std::vector<std::string> &params, T &value) {
    std::string str;

    for (const auto &param : params) {
        str += param;
    }

    // convert the string to the desired type
    if constexpr (std::is_same_v<T, int>)
        value = std::stoi(str);
    else if constexpr (std::is_same_v<T, uint32_t>)
        value = std::stoul(str);
    else if constexpr (std::is_same_v<T, float>)
        value = std::stof(str);
    else if constexpr (std::is_same_v<T, double>)
        value = std::stod(str);
    else if constexpr (std::is_same_v<T, bool>)
        value = str == "true";
    else
        value = str;
}

std::string concat(const std::vector<std::string> &params) {
    std::string str;

    for (const auto &param : params) {
        str += param;
    }

    return str;
}

bool is_number(const std::string &s) {
    static const auto is_digit = [](unsigned char c) { return !std::isdigit(c); };
    return !s.empty() && std::find_if(s.begin(), s.end(), is_digit) == s.end();
}

bool is_bool(const std::string &s) {
    return s == "true" || s == "false";
}

bool containsEqualSign(const std::vector<std::string> &params) {
    for (const auto &param : params) {
        if (param.find('=') != std::string::npos) {
            return true;
        }
    }
    return false;
}
}  // namespace

namespace fast_chess::cli {
using json = nlohmann::json;

namespace engine {
TimeControl::Limits parseTc(const std::string &tcString) {
    if (tcString == "infinite" || tcString == "inf") {
        return {};
    }

    TimeControl::Limits tc;

    std::string remainingStringVector = tcString;
    const bool has_moves              = str_utils::contains(tcString, "/");
    const bool has_inc                = str_utils::contains(tcString, "+");
    const bool has_minutes            = str_utils::contains(tcString, ":");

    if (has_moves) {
        const auto moves = str_utils::splitString(tcString, '/');
        if (moves[0] == "inf" || moves[0] == "infinite") {
            tc.moves = 0;
        } else {
            tc.moves = std::stoi(moves[0]);
        }
        remainingStringVector = moves[1];
    }

    if (has_inc) {
        const auto inc        = str_utils::splitString(remainingStringVector, '+');
        tc.increment          = static_cast<uint64_t>(std::stod(inc[1]) * 1000);
        remainingStringVector = inc[0];
    }

    if (has_minutes) {
        const auto clock_vector = str_utils::splitString(remainingStringVector, ':');
        tc.time                 = static_cast<int64_t>(std::stod(clock_vector[0]) * 1000 * 60) +
                  static_cast<int64_t>(std::stod(clock_vector[1]) * 1000);
    } else {
        tc.time = static_cast<int64_t>(std::stod(remainingStringVector) * 1000);
    }

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
    else if (key == "timemargin") {
        engineConfig.limit.tc.timemargin = std::stoi(value);
        if (engineConfig.limit.tc.timemargin < 0) {
            throw std::runtime_error("Error; timemargin cannot be a negative number");
        }
    } else if (key == "nodes")
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
        if (value != "uci") {
            throw std::runtime_error("Error; unsupported protocol");
        }
    } else
        OptionsParser::throwMissing("engine", key, value);
}

void validateEnginePath(std::string dir, std::string &cmd) {
    // engine path with dir
    auto engine_path = dir == "." ? "" : dir + cmd;

#ifndef NO_STD_FILESYSTEM
    if (!std::filesystem::exists(engine_path)) {
        // append .exe to cmd if it is missing on windows
#    ifdef _WIN64
        if (engine_path.find(".exe") == std::string::npos) {
            auto original_path = engine_path;

            cmd += ".exe";
            engine_path += ".exe";

            if (!std::filesystem::exists(engine_path)) {
                throw std::runtime_error("Error; Engine not found: " + original_path);
            }
        }
#    else
        throw std::runtime_error("Error; Engine not found: " + engine_path);

#    endif
    }
#endif
}

void parseEngine(const std::vector<std::string> &params, ArgumentData &argument_data) {
    argument_data.configs.emplace_back();

    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        engine::parseEngineKeyValues(argument_data.configs.back(), key, value);
    });

    engine::validateEnginePath(argument_data.configs.back().dir, argument_data.configs.back().cmd);
}

}  // namespace engine

void parseEach(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        for (auto &config : argument_data.configs) {
            engine::parseEngineKeyValues(config, key, value);
        }
    });
}

void parsePgnOut(const std::vector<std::string> &params, ArgumentData &argument_data) {
    if (containsEqualSign(params)) {
        parseDashOptions(params, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                argument_data.tournament_options.pgn.file = value;
            } else if (key == "nodes" && is_bool(value)) {
                argument_data.tournament_options.pgn.track_nodes = value == "true";
            } else if (key == "seldepth" && is_bool(value)) {
                argument_data.tournament_options.pgn.track_seldepth = value == "true";
            } else if (key == "nps" && is_bool(value)) {
                argument_data.tournament_options.pgn.track_nps = value == "true";
            } else if (key == "hashfull" && is_bool(value)) {
                argument_data.tournament_options.pgn.track_hashfull = value == "true";
            } else if (key == "tbhits" && is_bool(value)) {
                argument_data.tournament_options.pgn.track_tbhits = value == "true";
            } else if (key == "min" && is_bool(value)) {
                argument_data.tournament_options.pgn.min = value == "true";
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
    } else {
        // try to read as cutechess pgnout
        argument_data.tournament_options.pgn.file = params[0];
        argument_data.tournament_options.pgn.min =
            std::find(params.begin(), params.end(), "min") != params.end();
    }
    if (argument_data.tournament_options.pgn.file.empty()) 
        throw std::runtime_error("Error; Please specify filename for pgn output.");
}

void parseEpdOut(const std::vector<std::string> &params, ArgumentData &argument_data) {
    if (containsEqualSign(params)) {
        parseDashOptions(params, [&](const std::string &key, const std::string &value) {
            if (key == "file") {
                argument_data.tournament_options.epd.file = value;
            } else {
                OptionsParser::throwMissing("epdout", key, value);
            }
        });
    } else {
        // try to read as cutechess epdout
        parseValue(params, argument_data.tournament_options.epd.file);
    }
    if (argument_data.tournament_options.epd.file.empty()) 
        throw std::runtime_error("Error; Please specify filename for epd output.");
}

void parseOpening(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            argument_data.tournament_options.opening.file = value;
            if (str_utils::endsWith(value, ".epd")) {
                argument_data.tournament_options.opening.format = FormatType::EPD;
            } else if (str_utils::endsWith(value, ".pgn")) {
                argument_data.tournament_options.opening.format = FormatType::PGN;
            }

#ifndef NO_STD_FILESYSTEM
            if (!std::filesystem::exists(value)) {
                throw std::runtime_error("Opening file does not exist: " + value);
            }
#endif
        } else if (key == "format") {
            if (value == "epd") {
                argument_data.tournament_options.opening.format = FormatType::EPD;
            } else if (value == "pgn") {
                argument_data.tournament_options.opening.format = FormatType::PGN;
            } else {
                OptionsParser::throwMissing("openings format", key, value);
            }
        } else if (key == "order") {
            if (value == "sequential") {
                argument_data.tournament_options.opening.order = OrderType::SEQUENTIAL;
            } else if (value == "random") {
                argument_data.tournament_options.opening.order = OrderType::RANDOM;
            } else {
                OptionsParser::throwMissing("openings order", key, value);
            }
        } else if (key == "plies") {
            argument_data.tournament_options.opening.plies = std::stoi(value);
        } else if (key == "start") {
            argument_data.tournament_options.opening.start = std::stoi(value);
            if (argument_data.tournament_options.opening.start < 1)
                throw std::runtime_error("Starting offset must be at least 1!");
        } else if (key == "policy") {
            if (value != "round")
                throw std::runtime_error("Error; Unsupported opening book policy");
        } else {
            OptionsParser::throwMissing("openings", key, value);
        }
    });
}

void parseSprt(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        argument_data.tournament_options.sprt.enabled = true;
      
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
            argument_data.tournament_options.sprt.model = value;
        } else {
            OptionsParser::throwMissing("sprt", key, value);
        }
    });
}

void parseDraw(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        argument_data.tournament_options.draw.enabled = true;

        if (key == "movenumber") {
            argument_data.tournament_options.draw.move_number = std::stoi(value);
        } else if (key == "movecount") {
            argument_data.tournament_options.draw.move_count = std::stoi(value);
        } else if (key == "score") {
            if (std::stoi(value) >= 0) {
                argument_data.tournament_options.draw.score = std::stoi(value);
            } else {
                throw std::runtime_error("Score cannot be negative");
            }
        } else {
            OptionsParser::throwMissing("draw", key, value);
        }
    });
}

void parseResign(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        argument_data.tournament_options.resign.enabled = true;

        if (key == "movecount") {
            argument_data.tournament_options.resign.move_count = std::stoi(value);
        } else if (key == "twosided" && is_bool(value)) {
            argument_data.tournament_options.resign.twosided = value == "true";
        } else if (key == "score") {
            if (std::stoi(value) >= 0) {
                argument_data.tournament_options.resign.score = std::stoi(value);
            } else {
                throw std::runtime_error("Score cannot be negative");
            }
        } else {
            OptionsParser::throwMissing("resign", key, value);
        }
    });
}

void parseMaxMoves(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.maxmoves.move_count);
    argument_data.tournament_options.maxmoves.enabled = true;
}

void parseAutoSaveInterval(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.autosaveinterval);
}

void parseLog(const std::vector<std::string> &params, ArgumentData &) {
    std::string filename;
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            filename = value;
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
    if (filename.empty()) throw std::runtime_error("Error; Please specify filename for log output.");
    Logger::openFile(filename);
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

void parseConfig(const std::vector<std::string> &params, ArgumentData &argument_data) {
    bool drop_stats = false;

    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        if (key == "file") {
            loadJson(argument_data, value);
        } else if (key == "outname") {
            argument_data.tournament_options.config_name = value;
        } else if (key == "discard" && value == "true") {
            Logger::log<Logger::Level::INFO>("Discarding config file");
            argument_data.tournament_options = argument_data.old_tournament_options;
            argument_data.configs            = argument_data.old_configs;
            argument_data.stats.clear();
        } else if (key == "stats") {
            drop_stats = value == "false";
        }
    });

    if (drop_stats) {
        argument_data.stats.clear();
    }
}

}  // namespace config

void parseReport(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        if (key == "penta" && is_bool(value)) {
            argument_data.tournament_options.report_penta = value == "true";
        } else {
            OptionsParser::throwMissing("report", key, value);
        }
    });
}

void parseOutput(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
        if (key == "format" && (value == "cutechess" || value == "fastchess")) {
            argument_data.tournament_options.output = OutputFactory::getType(value);
        } else {
            OptionsParser::throwMissing("output", key, value);
        }
    });
}

void parseConcurrency(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.concurrency);
}

void parseEvent(const std::vector<std::string> &params, ArgumentData &argument_data) {
    argument_data.tournament_options.event_name = concat(params);
}

void parseSite(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.site);
}

void parseGames(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.games);
}

void parseRounds(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.rounds);
}

void parseRatinginterval(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.ratinginterval);
}

void parseScoreinterval(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.scoreinterval);
}

void parseSRand(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseValue(params, argument_data.tournament_options.seed);
}

void parseVersion(const std::vector<std::string> &, ArgumentData &) {
    OptionsParser::printVersion();
}

void parseHelp(const std::vector<std::string> &, ArgumentData &) { OptionsParser::printHelp(); }

void parseRecover(const std::vector<std::string> &, ArgumentData &argument_data) {
    argument_data.tournament_options.recover = true;
}

void parseRandomSeed(const std::vector<std::string> &, ArgumentData &argument_data) {
    argument_data.tournament_options.randomseed = true;
}

void parseRepeat(const std::vector<std::string> &params, ArgumentData &argument_data) {
    if (params.size() == 1 && is_number(params[0])) {
        parseValue(params, argument_data.tournament_options.games);
    } else {
        argument_data.tournament_options.games = 2;
    }
}

void parseVariant(const std::vector<std::string> &params, ArgumentData &argument_data) {
    std::string val;

    parseValue(params, val);

    if (val == "fischerandom") argument_data.tournament_options.variant = VariantType::FRC;
    if (val != "fischerandom" && val != "standard")
        throw std::runtime_error("Error; Unknown variant");
}

void parseTournament(const std::vector<std::string> &params, ArgumentData &) {
    std::string val;
    // Do nothing
    parseValue(params, val);
}

void parseQuick(const std::vector<std::string> &params, ArgumentData &argument_data) {
    parseDashOptions(params, [&](const std::string &key, const std::string &value) {
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

void parseAffinity(const std::vector<std::string> &, ArgumentData &argument_data) {
    argument_data.tournament_options.affinity = true;
}

void parseDebug(const std::vector<std::string> &, ArgumentData &) {
    // throw error
    std::string error_message =
        "Error; 'debug' option does not exist in fast-chess."
        " Use the 'log' option instead to write all engine input"
        " and output into a text file.";
    throw std::runtime_error(error_message);
}

OptionsParser::OptionsParser(int argc, char const *argv[]) {
    if (argc == 1) {
        printHelp();
    }

    addOption("engine", engine::parseEngine);
    addOption("each", parseEach);
    addOption("pgnout", parsePgnOut);
    addOption("epdout", parseEpdOut);
    addOption("openings", parseOpening);
    addOption("sprt", parseSprt);
    addOption("draw", parseDraw);
    addOption("resign", parseResign);
    addOption("maxmoves", parseMaxMoves);
    addOption("autosaveinterval", parseAutoSaveInterval);
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
    addOption("scoreinterval", parseScoreinterval);
    addOption("srand", parseSRand);
    addOption("version", parseVersion);
    addOption("-version", parseVersion);
    addOption("v", parseVersion);
    addOption("-v", parseVersion);
    addOption("help", parseHelp);
    addOption("-help", parseHelp);
    addOption("recover", parseRecover);
    addOption("randomseed", parseRandomSeed);
    addOption("repeat", parseRepeat);
    addOption("variant", parseVariant);
    addOption("tournament", parseTournament);
    addOption("quick", parseQuick);
    addOption("use-affinity", parseAffinity);
    addOption("debug", parseDebug);

    parse(argc, argv);

    // apply the variant type to all configs
    for (auto &config : argument_data_.configs) {
        config.variant = argument_data_.tournament_options.variant;
    }
}

}  // namespace fast_chess::cli
