#include <cli/cli.hpp>

#include <cli/args.hpp>
#include <cli/cli_args.hpp>
#include <cli/sanitize.hpp>
#include <core/filesystem/file_system.hpp>
#include <core/logger/logger.hpp>
#include <core/rand.hpp>
#include <matchmaking/output/output_factory.hpp>
#include <matchmaking/scoreboard.hpp>
#include <types/engine_config.hpp>
#include <types/tournament.hpp>

namespace {
std::string concat(const std::vector<std::string> &params) {
    std::string str;

    for (const auto &param : params) {
        str += param;
    }

    return str;
}
}  // namespace

namespace fastchess::cli {

using json = nlohmann::json;
namespace json_config {
void loadJson(ArgumentData &argument_data, const std::string &filename) {
    Logger::print<Logger::Level::INFO>("Loading config file: {}", filename);

    std::ifstream f(filename);

    if (!f.is_open()) {
        throw std::runtime_error("File not found: " + filename);
    }

    json jsonfile = json::parse(f);

    argument_data.old_configs           = argument_data.configs;
    argument_data.old_tournament_config = argument_data.tournament_config;

    argument_data.tournament_config = jsonfile.get<config::Tournament>();

    argument_data.configs.clear();

    for (const auto &engine : jsonfile["engines"]) {
        argument_data.configs.push_back(engine.get<EngineConfiguration>());
    }

    argument_data.stats = jsonfile["stats"].get<stats_map>();
}

}  // namespace json_config

namespace engine {

TimeControl::Limits parseTc(const std::string &tcString) {
    if (str_utils::contains(tcString, "hg")) throw std::runtime_error("Hourglass time control not supported.");
    if (tcString == "infinite" || tcString == "inf") return {};

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

bool isEngineSettableOption(std::string_view stringFormat) { return str_utils::startsWith(stringFormat, "option."); }

void parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key, const std::string &value) {
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
            throw std::runtime_error("The value for timemargin cannot be a negative number.");
        }
    } else if (key == "nodes")
        engineConfig.limit.nodes = std::stoll(value);
    else if (key == "plies" || key == "depth")
        engineConfig.limit.plies = std::stoll(value);
    else if (key == "dir")
        engineConfig.dir = value;
    else if (key == "args")
        engineConfig.args = value;
    else if (key == "restart") {
        if (value != "on" && value != "off") {
            throw std::runtime_error("Invalid parameter (must be either \"on\" or \"off\"): " + value);
        }
        engineConfig.restart = value == "on";
    } else if (isEngineSettableOption(key)) {
        // Strip option.Name of the option. Part
        const std::size_t pos         = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.emplace_back(strippedKey, value);
    } else if (key == "proto") {
        if (value != "uci") {
            throw std::runtime_error("Unsupported protocol.");
        }
    } else
        throw std::runtime_error("Unknown engine key: " + key + " with value: " + value);
}
}  // namespace engine

CommandLineParser create_parser() {
    CommandLineParser parser("Chess Tournament", "A chess tournament management program");

    parser.addOption(
        Option("engine", "Configure an engine for the tournament")
            .setBeforeProcess([](ArgumentData &data) { data.configs.emplace_back(); })

            .addSubOption(
                SubOption("cmd", "Engine executable path")
                    .required()
                    .setSetter([](const std::string &value, ArgumentData &data) { data.configs.back().cmd = value; }))

            .addSubOption(SubOption("name", "Engine name").setSetter([](const std::string &value, ArgumentData &data) {
                data.configs.back().name = value;
            }))

            .addSubOption(SubOption("tc", "Time control (e.g., '40/60' or '5+0.1')")
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.configs.back().limit.tc = engine::parseTc(value);
                              }))

            .addSubOption(SubOption("st", "Fixed time (e.g., '5.0' for 5 seconds")
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.configs.back().limit.tc.fixed_time =
                                      static_cast<int64_t>(std::stod(value) * 1000);
                              }))

            .addSubOption(SubOption("timemargin", "Time margin in milliseconds")
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.configs.back().limit.tc.timemargin = std::stoi(value);
                                  if (data.configs.back().limit.tc.timemargin < 0) {
                                      throw std::runtime_error(
                                          "The value for timemargin cannot be "
                                          "a negative number.");
                                  }
                              }))

            .addSubOption(SubOption("nodes", "Node limit").setSetter([](const std::string &value, ArgumentData &data) {
                data.configs.back().limit.nodes = std::stoll(value);
            }))

            .addSubOption(SubOption("plies", "Ply limit").setSetter([](const std::string &value, ArgumentData &data) {
                data.configs.back().limit.plies = std::stoll(value);
            }))

            .addSubOption(
                SubOption("dir", "Engine directory").setSetter([](const std::string &value, ArgumentData &data) {
                    data.configs.back().dir = value;
                }))

            .addSubOption(
                SubOption("args", "Engine arguments").setSetter([](const std::string &value, ArgumentData &data) {
                    data.configs.back().args = value;
                }))

            .addSubOption(SubOption("restart", "Restart engine (on/off)")
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  if (value != "on" && value != "off") {
                                      throw std::runtime_error(
                                          "Invalid parameter (must be either "
                                          "\"on\" or \"off\"): " +
                                          value);
                                  }
                                  data.configs.back().restart = value == "on";
                              }))

            .addSubOption(
                SubOption("proto", "Protocol (e.g., 'uci')").setSetter([](const std::string &value, ArgumentData &) {
                    if (value != "uci") {
                        throw std::runtime_error("Unsupported protocol.");
                    }
                }))

            // Dynamic engine options handler (for option.* parameters)
            .setDynamicOptionHandler([](const std::string &key, const std::string &value, ArgumentData &data) -> bool {
                if (engine::isEngineSettableOption(key)) {
                    // Strip option.Name of the option part
                    const std::size_t pos         = key.find('.');
                    const std::string strippedKey = key.substr(pos + 1);
                    data.configs.back().options.emplace_back(strippedKey, value);
                    return true;
                }
                return false;
            }));

    parser.addOption(
        Option("each", "Apply engine settings to all engines")
            .setDynamicOptionHandler([](const std::string &key, const std::string &value, ArgumentData &data) -> bool {
                for (auto &config : data.configs) {
                    engine::parseEngineKeyValues(config, key, value);
                }
                return true;
            }));

    parser.addOption(Option("pgnout", "Output the games to a PGN file")
                         .addSubOption(SubOption("file", "PGN output filename")
                                           .required()
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.file = value;
                                           }))
                         .addSubOption(SubOption("nodes", "Track nodes")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_nodes = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("seldepth", "Track selective depth")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_seldepth = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("nps", "Track nodes per second")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_nps = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("hashfull", "Track hash fullness")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_hashfull = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("tbhits", "Track tablebase hits")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_tbhits = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("timeleft", "Track time left")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_timeleft = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("latency", "Track latency")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.track_latency = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("min", "Minimal PGN format")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.min = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("notation", "Move notation format")
                                           .allowValues({"san", "lan", "uci"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               if (value == "san") {
                                                   data.tournament_config.pgn.notation = NotationType::SAN;
                                               } else if (value == "lan") {
                                                   data.tournament_config.pgn.notation = NotationType::LAN;
                                               } else if (value == "uci") {
                                                   data.tournament_config.pgn.notation = NotationType::UCI;
                                               }
                                           }))
                         .addSubOption(SubOption("match_line", "Additional regex pattern to match lines")
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.additional_lines_rgx.push_back(value);
                                           }))
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             // Handle legacy format: pgnout filename [min]
                             if (!params.empty() && params[0].find('=') == std::string::npos) {
                                 data.tournament_config.pgn.file = params[0];
                                 data.tournament_config.pgn.min =
                                     std::find(params.begin(), params.end(), "min") != params.end();
                             }
                         }));

    parser.addOption(Option("epdout", "Output the games to an EPD file")
                         .addSubOption(SubOption("file", "EPD output filename")
                                           .required()
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.epd.file = value;
                                           }))
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             // Handle legacy format: epdout filename
                             if (!params.empty() && params[0].find('=') == std::string::npos) {
                                 data.tournament_config.epd.file = params[0];
                             }
                         }));

    parser.addOption(Option("openings", "Configure opening book settings")
                         .addSubOption(SubOption("file", "Opening book file")
                                           .required()
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               std::cout << "file:" << value << std::endl;
                                               data.tournament_config.opening.file = value;
                                               if (str_utils::endsWith(value, ".epd")) {
                                                   data.tournament_config.opening.format = FormatType::EPD;
                                               } else if (str_utils::endsWith(value, ".pgn")) {
                                                   data.tournament_config.opening.format = FormatType::PGN;
                                               }

#ifndef NO_STD_FILESYSTEM
                                               if (!std::filesystem::exists(value)) {
                                                   throw std::runtime_error("Opening file does not exist: " + value);
                                               }
#endif
                                           }))
                         .addSubOption(SubOption("format", "Opening book format")
                                           .allowValues({"epd", "pgn"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               if (value == "epd") {
                                                   data.tournament_config.opening.format = FormatType::EPD;
                                               } else if (value == "pgn") {
                                                   data.tournament_config.opening.format = FormatType::PGN;
                                               }
                                           }))
                         .addSubOption(SubOption("order", "Opening selection order")
                                           .allowValues({"sequential", "random"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               if (value == "sequential") {
                                                   data.tournament_config.opening.order = OrderType::SEQUENTIAL;
                                               } else if (value == "random") {
                                                   data.tournament_config.opening.order = OrderType::RANDOM;
                                               }
                                           }))
                         .addSubOption(SubOption("plies", "Number of plies to play from openings")
                                           .addValidator(validators::is_integer)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.opening.plies = converters::to_int(value);
                                           }))
                         .addSubOption(SubOption("start", "Starting offset in the opening book")
                                           .addValidator(validators::integer_range(1, INT_MAX))
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.opening.start = converters::to_int(value);
                                           }))
                         .addSubOption(SubOption("policy", "Opening book policy")
                                           .allowValues({"round"})
                                           .setSetter([](const std::string &value, ArgumentData &) {
                                               if (value != "round") {
                                                   throw std::runtime_error("Unsupported opening book policy.");
                                               }
                                           })));

    parser.addOption(Option("sprt", "Configure Sequential Probability Ratio Test")
                         .setBeforeProcess([](ArgumentData &data) {
                             data.tournament_config.sprt.enabled = true;
                             if (data.tournament_config.rounds == 0) {
                                 data.tournament_config.rounds = 500000;
                             }
                         })
                         .addSubOption(SubOption("elo0", "Null hypothesis ELO difference")
                                           .required()
                                           .addValidator(validators::is_float)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.sprt.elo0 = converters::to_float(value);
                                           }))
                         .addSubOption(SubOption("elo1", "Alternative hypothesis ELO difference")
                                           .required()
                                           .addValidator(validators::is_float)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.sprt.elo1 = converters::to_float(value);
                                           }))
                         .addSubOption(SubOption("alpha", "Type I error rate")
                                           .addValidator(validators::is_float)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.sprt.alpha = converters::to_float(value);
                                           }))
                         .addSubOption(SubOption("beta", "Type II error rate")
                                           .addValidator(validators::is_float)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.sprt.beta = converters::to_float(value);
                                           }))
                         .addSubOption(SubOption("model", "Statistical model to use")
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.sprt.model = value;
                                           })));

    parser.addOption(Option("draw", "Configure draw adjudication rules")
                         .setBeforeProcess([](ArgumentData &data) { data.tournament_config.draw.enabled = true; })
                         .addSubOption(SubOption("movenumber", "Minimum move number before draw adjudication")
                                           .addValidator(validators::is_integer)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.draw.move_number = converters::to_int(value);
                                           }))
                         .addSubOption(SubOption("movecount", "Required number of moves within score threshold")
                                           .addValidator(validators::is_integer)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.draw.move_count = converters::to_int(value);
                                           }))
                         .addSubOption(SubOption("score", "Score threshold in centipawns")
                                           .addValidator(validators::integer_range(0, INT_MAX))
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.draw.score = converters::to_int(value);
                                           })));

    parser.addOption(Option("resign", "Configure resignation adjudication rules")
                         .setBeforeProcess([](ArgumentData &data) { data.tournament_config.resign.enabled = true; })
                         .addSubOption(SubOption("movecount", "Required number of moves with score below threshold")
                                           .addValidator(validators::is_integer)
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.resign.move_count = converters::to_int(value);
                                           }))
                         .addSubOption(SubOption("twosided", "Require both engines to agree on resignation")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.resign.twosided = converters::to_bool(value);
                                           }))
                         .addSubOption(SubOption("score", "Score threshold in centipawns")
                                           .addValidator(validators::integer_range(0, INT_MAX))
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.resign.score = converters::to_int(value);
                                           })));

    parser.addOption(Option("maxmoves", "Maximum moves before game is drawn")
                         .setBeforeProcess([](ArgumentData &data) { data.tournament_config.maxmoves.enabled = true; })
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.maxmoves.move_count = converters::to_int(params[0]);
                             }
                         })

    );

    parser.addOption(
        Option("tb", "Configure tablebase adjudication")
            .setBeforeProcess([](ArgumentData &data) { data.tournament_config.tb_adjudication.enabled = true; })
            .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                if (!params.empty()) {
                    data.tournament_config.tb_adjudication.syzygy_dirs = params[0];
                }
            }));

    parser.addOption(Option("tbpieces", "Maximum number of pieces for tablebase adjudication")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.tb_adjudication.max_pieces = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("tbignore50", "Ignore 50-move rule in tablebase adjudication")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.tb_adjudication.ignore_50_move_rule = true;
                         }));

    parser.addOption(Option("autosaveinterval", "Automatically save tournament state at specified interval")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.autosaveinterval = converters::to_int(params[0]);
                             }
                         })

    );

    parser.addOption(
        Option("log", "Configure logging settings")
            .addSubOption(SubOption("file", "Log filename").setSetter([](const std::string &value, ArgumentData &data) {
                data.tournament_config.log.file = value;
            }))
            .addSubOption(SubOption("level", "Log level")
                              .allowValues({"trace", "warn", "info", "err", "fatal"})
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  Logger::Level level = Logger::Level::WARN;
                                  if (value == "trace") {
                                      level = Logger::Level::TRACE;
                                  } else if (value == "warn") {
                                      level = Logger::Level::WARN;
                                  } else if (value == "info") {
                                      level = Logger::Level::INFO;
                                  } else if (value == "err") {
                                      level = Logger::Level::ERR;
                                  } else if (value == "fatal") {
                                      level = Logger::Level::FATAL;
                                  }
                                  data.tournament_config.log.level = level;
                              }))
            .addSubOption(SubOption("compress", "Compress log files")
                              .allowValues({"true", "false"})
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.tournament_config.log.compress = converters::to_bool(value);
                              }))
            .addSubOption(SubOption("realtime", "Write logs in real-time")
                              .allowValues({"true", "false"})
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.tournament_config.log.realtime = converters::to_bool(value);
                              }))
            .addSubOption(SubOption("engine", "Log engine communications")
                              .allowValues({"true", "false"})
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.tournament_config.log.engine_coms = converters::to_bool(value);
                              })));

    parser.addOption(Option("config", "Load/save tournament configuration")
                         .addSubOption(SubOption("file", "Configuration file path")
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               json_config::loadJson(data, value);
                                           }))
                         .addSubOption(SubOption("outname", "Output configuration name")
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.config_name = value;
                                           }))
                         .addSubOption(SubOption("discard", "Discard loaded configuration")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               if (value == "true") {
                                                   Logger::print<Logger::Level::INFO>("Discarding config file");
                                                   data.tournament_config = data.old_tournament_config;
                                                   data.configs           = data.old_configs;
                                                   data.stats.clear();
                                               }
                                           }))
                         .addSubOption(SubOption("stats", "Load/save statistics")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               if (value == "false") {
                                                   data.stats.clear();
                                               }
                                           })));

    parser.addOption(Option("report", "Configure reporting options")
                         .addSubOption(SubOption("penta", "Enable pentanomial reporting")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.report_penta = converters::to_bool(value);
                                           }))

    );

    parser.addOption(Option("output", "Configure output format")
                         .addSubOption(SubOption("format", "Output format type")
                                           .allowValues({"cutechess", "fastchess"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.output = OutputFactory::getType(value);
                                           }))

    );

    parser.addOption(Option("concurrency", "Number of games to play simultaneously")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.concurrency = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("crc32", "Configure CRC checksumming")
                         .addSubOption(SubOption("pgn", "Enable CRC checksumming for PGN output")
                                           .allowValues({"true", "false"})
                                           .setSetter([](const std::string &value, ArgumentData &data) {
                                               data.tournament_config.pgn.crc = converters::to_bool(value);
                                           })));

    parser.addOption(Option("-force-concurrency", "Force specified concurrency even if it exceeds CPU count")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.force_concurrency = true;
                         })

    );

    parser.addOption(Option("event", "Set event name for PGN header")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             data.tournament_config.pgn.event_name = concat(params);
                         }));

    parser.addOption(Option("site", "Set site name for PGN header")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.pgn.site = params[0];
                             }
                         }));

    parser.addOption(Option("games", "Number of games to play per pairing")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.games = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("rounds", "Number of rounds to play")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.rounds = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("wait", "Delay between games in milliseconds")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.wait = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("noswap", "Don't swap sides between games")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.noswap = true;
                         }));

    parser.addOption(Option("reverse", "Start with reversed sides")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.reverse = true;
                         }));

    parser.addOption(Option("ratinginterval", "Rating report interval")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.ratinginterval = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("scoreinterval", "Score report interval")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.scoreinterval = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(Option("srand", "Random seed for the tournament")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 data.tournament_config.seed = converters::to_int(params[0]);
                             }
                         }));

    parser.addOption(
        Option("version", "Display program version").setHandler([](const std::vector<std::string> &, ArgumentData &) {
            std::cout << "FastChess Version " << Version << std::endl;
            std::exit(0);
        }));

    parser.addOption(
        Option("-version", "Display program version").setHandler([](const std::vector<std::string> &, ArgumentData &) {
            std::cout << "FastChess Version " << Version << std::endl;
            std::exit(0);
        }));

    parser.addOption(
        Option("v", "Display program version").setHandler([](const std::vector<std::string> &, ArgumentData &) {
            std::cout << "FastChess Version " << Version << std::endl;
            std::exit(0);
        }));

    parser.addOption(
        Option("-v", "Display program version").setHandler([](const std::vector<std::string> &, ArgumentData &) {
            std::cout << "FastChess Version " << Version << std::endl;
            std::exit(0);
        }));

    parser.addOption(Option("help", "Display help information")
                         .setHandler([&parser](const std::vector<std::string> &, ArgumentData &) {
                             parser.printHelp();
                             std::exit(0);
                         }));

    parser.addOption(Option("-help", "Display help information")
                         .setHandler([&parser](const std::vector<std::string> &, ArgumentData &) {
                             parser.printHelp();
                             std::exit(0);
                         }));

    parser.addOption(Option("recover", "Recover from a saved state")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.recover = true;
                         }));

    parser.addOption(Option("repeat", "Play each opening twice with colors reversed")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty() && std::all_of(params[0].begin(), params[0].end(), ::isdigit)) {
                                 data.tournament_config.games = converters::to_int(params[0]);
                             } else {
                                 data.tournament_config.games = 2;
                             }
                         }));

    parser.addOption(Option("variant", "Chess variant to play")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &data) {
                             if (!params.empty()) {
                                 std::string val = params[0];
                                 if (val == "fischerandom") {
                                     data.tournament_config.variant = VariantType::FRC;
                                 } else if (val != "standard") {
                                     throw std::runtime_error("Unknown variant.");
                                 }
                             }
                         }));

    parser.addOption(Option("tournament", "Tournament format")
                         .setHandler([](const std::vector<std::string> &params, ArgumentData &) {
                             if (!params.empty()) {
                                 std::string val = params[0];
                                 if (val != "roundrobin") {
                                     throw std::runtime_error(
                                         "Unsupported tournament format. Only "
                                         "supports roundrobin.");
                                 }
                             }
                         }));

    parser.addOption(
        Option("quick", "Quick setup for engine testing")
            .addSubOption(SubOption("cmd", "Engine executable path")
                              .required()
                              .setSetter([](const std::string &value, ArgumentData &data) {
                                  data.configs.emplace_back();
                                  data.configs.back().cmd  = value;
                                  data.configs.back().name = value;

                                  data.configs.back().limit.tc.time      = 10 * 1000;
                                  data.configs.back().limit.tc.increment = 100;

                                  data.tournament_config.recover = true;
                              }))
            .addSubOption(
                SubOption("book", "Opening book file").setSetter([](const std::string &value, ArgumentData &data) {
                    data.tournament_config.opening.file  = value;
                    data.tournament_config.opening.order = OrderType::RANDOM;
                    if (str_utils::endsWith(value, ".pgn")) {
                        data.tournament_config.opening.format = FormatType::PGN;
                    } else if (str_utils::endsWith(value, ".epd")) {
                        data.tournament_config.opening.format = FormatType::EPD;
                    } else {
                        throw std::runtime_error(
                            "Please include the .png or .epd file "
                            "extension for the opening book.");
                    }
                }))
            .setBeforeProcess([](ArgumentData &data) {
                data.tournament_config.games  = 2;
                data.tournament_config.rounds = 25000;
                data.tournament_config.concurrency =
                    std::max(static_cast<int>(1), static_cast<int>(std::thread::hardware_concurrency() - 2));
                data.tournament_config.recover = true;

                data.tournament_config.draw.enabled     = true;
                data.tournament_config.draw.move_number = 30;
                data.tournament_config.draw.move_count  = 8;
                data.tournament_config.draw.score       = 8;

                data.tournament_config.output = OutputType::CUTECHESS;
            }));

    parser.addOption(Option("use-affinity", "Pin engines to specific CPU cores")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.affinity = true;
                         }));

    parser.addOption(Option("show-latency", "Display engine communication latency")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.show_latency = true;
                         }));

    parser.addOption(
        Option("debug", "Debug mode (not supported)").setHandler([](const std::vector<std::string> &, ArgumentData &) {
            std::string error_message =
                "The 'debug' option does not exist in fastchess."
                " Use the 'log' option instead to write all engine input"
                " and output into a text file.";
            throw std::runtime_error(error_message);
        }));

    parser.addOption(Option("testEnv", "Run in test environment mode")
                         .setHandler([](const std::vector<std::string> &, ArgumentData &data) {
                             data.tournament_config.test_env = true;
                         }));

    return parser;
}
}  // namespace fastchess::cli