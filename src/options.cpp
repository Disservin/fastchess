#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <type_traits>
#include <unordered_map>

#include "options.hpp"

namespace CMD
{

Options::Options(int argc, char const *argv[])
{
    // Name of the current exec program
    const std::string current_exec_name = argv[0];
    const std::string yamlConfiguration = "config.yml";
    Yaml yaml;
    yaml.loadFile(yamlConfiguration);

    parseYaml(yaml);

    for (int i = 1; i < argc; i++)
    {
        const std::string arg = argv[i];
        if (arg == "-engine")
        {
            configs.push_back(EngineConfiguration());
            parseEngineParams(i, argc, argv, configs.back());
        }
        else if (arg == "-each")
            parseEachOptions(i, argc, argv);
        else if (arg == "-concurrency")
            parseOption(i, argc, argv, gameOptions.concurrency);
        else if (arg == "-event")
            parseOption(i, argc, argv, gameOptions.eventName);
        else if (arg == "-site")
            parseOption(i, argc, argv, gameOptions.site);
        else if (arg == "-games")
            parseOption(i, argc, argv, gameOptions.games);
        else if (arg == "-rounds")
            parseOption(i, argc, argv, gameOptions.rounds);
        else if (arg == "-pgnout")
            parsePgnOptions(i, argc, argv);
        else if (arg == "-openings")
            parseOpeningOptions(i, argc, argv);
        else if (arg == "-recover")
            gameOptions.recover = true;
        else if (arg == "-repeat")
            gameOptions.games = 2;
        else if (arg == "-sprt")
            parseSprt(i, argc, argv);
        else if (arg == "-draw")
            parseDrawOptions(i, argc, argv);
        else if (arg == "-resign")
            parseResignOptions(i, argc, argv);
        else if (arg == "-ratinginterval")
            parseOption(i, argc, argv, gameOptions.ratinginterval);
        else if (arg == "-srand")
            parseOption(i, argc, argv, gameOptions.seed);
        else if (arg == "-version")
            printVersion(i);
        else if (arg == "-log")
            parseLog(i, argc, argv);
        else
        {
            i++;
            std::cout << "\nDash command: " << arg << " doesnt exist!" << std::endl;
        }
    }

    for (auto &config : configs)
    {
        if (config.name.empty())
        {
            throw std::runtime_error("Warning: Each engine must have a name!");
        }

        config.recover = gameOptions.recover;
    }
}

bool Options::isEngineSettableOption(const std::string &stringFormat) const
{
    if (startsWith(stringFormat, "option."))
        return true;
    return false;
}

TimeControl Options::parseTc(const std::string &tcString)
{
    TimeControl tc;

    std::string remainingStringVector = tcString;
    const bool has_moves = contains(tcString, "/");
    const bool has_inc = contains(tcString, "+");

    if (has_moves)
    {
        const auto moves = splitString(tcString, '/');
        tc.moves = std::stoi(moves[0]);
        remainingStringVector = moves[1];
    }

    if (has_inc)
    {
        const auto moves = splitString(remainingStringVector, '+');
        tc.increment = std::stod(moves[1].c_str()) * 1000;
        remainingStringVector = moves[0];
    }

    tc.time = std::stod(remainingStringVector.c_str()) * 1000;

    return tc;
}

void Options::printVersion(int &i)
{
    i++;
    std::unordered_map<std::string, std::string> months({{"Jan", "01"},
                                                         {"Feb", "02"},
                                                         {"Mar", "03"},
                                                         {"Apr", "04"},
                                                         {"May", "05"},
                                                         {"Jun", "06"},
                                                         {"Jul", "07"},
                                                         {"Aug", "08"},
                                                         {"Sep", "09"},
                                                         {"Oct", "10"},
                                                         {"Nov", "11"},
                                                         {"Dec", "12"}});

    std::string month, day, year;
    std::stringstream ss, date(__DATE__); // {month} {date} {year}

    ss << "fast-chess ";
#ifdef GIT_DATE
    ss << GIT_DATE;
#else

    date >> month >> day >> year;
    if (day.length() == 1)
        day = "0" + day;
    ss << year.substr(2) << months[month] << day;
#endif

#ifdef GIT_SHA
    ss << "-" << GIT_SHA;
#endif

    ss << "\n";

    std::cout << ss.str();
    exit(0);
}

void Options::parseLog(int &i, int argc, const char *argv[])
{
    i++;
    while (i < argc && argv[i][0] != '-')
    {
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);
        if (key == "file")
        {
            Logger::openFile(value);
        }
        i++;
    }
}

template <typename T>
void Options::parseOption(int &i, int argc, const char *argv[], T &optionValue)
{
    i++;
    if (i < argc && argv[i][0] != '-')
    {
        if constexpr (std::is_same_v<T, int>)
            optionValue = std::stoi(argv[i]);
        else if constexpr (std::is_same_v<T, uint32_t>)
            optionValue = std::stoul(argv[i]);
        else if constexpr (std::is_same_v<T, float>)
            optionValue = std::stof(argv[i]);
        else if constexpr (std::is_same_v<T, double>)
            optionValue = std::stod(argv[i]);
        else
            optionValue = argv[i];
    }
}

void Options::parseEngineKeyValues(EngineConfiguration &engineConfig, const std::string &key,
                                   const std::string &value)
{
    if (key == "cmd")
    {
        engineConfig.cmd = value;
    }
    else if (key == "name")
    {
        engineConfig.name = value;
    }
    else if (key == "tc")
    {
        engineConfig.tc = parseTc(value);
    }
    else if (key == "st")
    {
        engineConfig.tc.fixed_time = std::stod(value) * 1000;
    }
    else if (key == "nodes")
    {
        engineConfig.nodes = std::stoll(value);
    }
    else if (key == "plies")
    {
        engineConfig.plies = std::stoll(value);
    }
    else if (key == "dir")
    {
        engineConfig.dir = value;
    }
    else if (isEngineSettableOption(key))
    {
        // Strip option.Name of the option. Part
        const size_t pos = key.find('.');
        const std::string strippedKey = key.substr(pos + 1);
        engineConfig.options.push_back(std::make_pair(strippedKey, value));
    }
    else
    {
        std::cout << "\nUnrecognized engine option: " << key << " parsing failed." << std::endl;
    }
}

void Options::parseEachOptions(int &i, int argc, char const *argv[])
{
    i++;
    while (i < argc && argv[i][0] != '-')
    {
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);

        for (auto &config : configs)
        {
            parseEngineKeyValues(config, key, value);
        }

        i++;
    }
    i--;
}

void Options::parseEngineParams(int &i, int argc, char const *argv[],
                                EngineConfiguration &engineParams)
{
    i++;
    while (i < argc && argv[i][0] != '-')
    {
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);

        parseEngineKeyValues(engineParams, key, value);

        i++;
    }
    i--;
}

void Options::parseSprt(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        // If the user didn't set a game param just use a very big default
        if (gameOptions.rounds == 0)
            gameOptions.rounds = 500000;
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);
        if (key == "elo0")
        {
            gameOptions.sprt.elo0 = std::stod(value);
        }
        else if (key == "elo1")
        {
            gameOptions.sprt.elo1 = std::stod(value);
        }
        else if (key == "alpha")
        {
            gameOptions.sprt.alpha = std::stod(value);
        }
        else if (key == "beta")
        {
            gameOptions.sprt.beta = std::stod(value);
        }
        else
        {
            std::cout << "\nUnrecognized sprt option: " << key << " with value " << value
                      << " parsing failed." << std::endl;
        }

        i++;
    }

    i--;
}

void Options::parseDrawOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        gameOptions.draw.enabled = true;
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);
        if (key == "movenumber")
        {
            gameOptions.draw.moveNumber = std::stoi(value);
        }
        else if (key == "movecount")
        {
            gameOptions.draw.moveCount = std::stoi(value);
        }
        else if (key == "score")
        {
            gameOptions.draw.score = std::stoi(value);
        }
        else
        {
            std::cout << "\nUnrecognized draw option: " << key << " with value " << value
                      << " parsing failed." << std::endl;
        }

        i++;
    }

    i--;
}

void Options::parseResignOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        gameOptions.resign.enabled = true;
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);

        if (key == "movecount")
        {
            gameOptions.resign.moveCount = std::stoi(value);
        }
        else if (key == "score")
        {
            gameOptions.resign.score = std::stoi(value);
        }
        else
        {
            std::cout << "\nUnrecognized resign option: " << key << " with value " << value
                      << " parsing failed." << std::endl;
        }

        i++;
    }

    i--;
}

void Options::parseOpeningOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        const std::string param = argv[i];
        const size_t pos = param.find('=');
        const std::string key = param.substr(0, pos);
        const std::string value = param.substr(pos + 1);

        if (key == "file")
        {
            gameOptions.opening.file = value;
        }
        else if (key == "format")
        {
            gameOptions.opening.format = value;
        }
        else if (key == "order")
        {
            gameOptions.opening.order = value;
        }
        else if (key == "plies")
        {
            gameOptions.opening.plies = std::stoi(value);
        }
        else if (key == "start")
        {
            gameOptions.opening.start = std::stoi(value);
        }
        else
        {
            std::cout << "\nUnrecognized opening option: " << key << " with value " << value
                      << " parsing failed." << std::endl;
            return;
        }
        i++;
    }
    i--;
}

void Options::parsePgnOptions(int &i, int argc, char const *argv[])
{
    i++;

    while (i < argc && argv[i][0] != '-')
    {
        std::string param = argv[i];
        size_t pos = param.find('=');
        std::string key = param.substr(0, pos);
        std::string value = param.substr(pos + 1);

        if (key == "file")
        {
            gameOptions.pgn.file = value;
        }
        else if (key == "tracknodes")
        {
            gameOptions.pgn.trackNodes = true;
        }
        else if (key == "notation")
        {
            gameOptions.pgn.notation = value;
        }
        else
        {
            std::cout << "\nUnrecognized pgn option: " << key << " with value " << value
                      << " parsing failed." << std::endl;
            return;
        }
        i++;
    }
    i--;
}

std::vector<EngineConfiguration> Options::getEngineConfigs() const
{
    return configs;
}

GameManagerOptions Options::getGameOptions() const
{
    return gameOptions;
}

bool Options::startsWith(std::string_view haystack, std::string_view needle)
{
    if (needle.empty())
        return false;
    return (haystack.rfind(needle, 0) != std::string::npos);
}

bool Options::contains(std::string_view haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

bool Options::contains(const std::vector<std::string> &haystack, std::string_view needle)
{
    return std::find(haystack.begin(), haystack.end(), needle) != haystack.end();
}

std::vector<std::string> Options::splitString(const std::string &string, const char &delimiter)
{
    std::stringstream string_stream(string);
    std::string segment;
    std::vector<std::string> seglist;

    while (std::getline(string_stream, segment, delimiter))
    {
        seglist.emplace_back(segment);
    }

    return seglist;
}

void Options::parseYaml(Yaml &yaml)
{
    // TODO parameters pgn/log/engine config

    gameOptions.eventName = yaml["event"].get<std::string>();
    gameOptions.games = yaml["games"].get<int>();
    gameOptions.rounds = yaml["rounds"].get<int>();
    gameOptions.recover = yaml["recover"].get<bool>();
    gameOptions.concurrency = yaml["concurrency"].get<int>();
    gameOptions.ratinginterval = yaml["rating-interval"].get<int>();

    gameOptions.save.wins = yaml["save"]["wins"].get<int>();
    gameOptions.save.draws = yaml["save"]["draws"].get<int>();
    gameOptions.save.losses = yaml["save"]["losses"].get<int>();
    gameOptions.save.pentaWW = yaml["save"]["pentaWW"].get<int>();
    gameOptions.save.pentaWD = yaml["save"]["pentaWD"].get<int>();
    gameOptions.save.pentaWL = yaml["save"]["pentaWL"].get<int>();
    gameOptions.save.pentaLD = yaml["save"]["pentaLD"].get<int>();
    gameOptions.save.pentaLL = yaml["save"]["pentaLL"].get<int>();

    parseYamlSprt(yaml);
    parseYamlOpening(yaml);
    parseYamlDraw(yaml);
    parseYamlResign(yaml);
}

void Options::parseYamlSprt(Yaml &yaml)
{
    if (yaml["sprt"]["enabled"].get<bool>())
    {
        gameOptions.sprt.alpha = yaml["sprt"]["alpha"].get<double>();
        gameOptions.sprt.beta = yaml["sprt"]["beta"].get<double>();
        gameOptions.sprt.elo0 = yaml["sprt"]["elo0"].get<double>();
        gameOptions.sprt.elo1 = yaml["sprt"]["elo1"].get<double>();
    }
}

void Options::parseYamlOpening(Yaml &yaml)
{
    if (yaml["openings"]["enabled"].get<bool>())
    {
        gameOptions.opening.file = yaml["openings"]["file"].get<std::string>();
        gameOptions.opening.format = yaml["openings"]["format"].get<std::string>();
        gameOptions.opening.order = yaml["openings"]["order"].get<std::string>();
        gameOptions.opening.plies = yaml["openings"]["plies"].get<int>();
        gameOptions.opening.start = yaml["openings"]["start"].get<int>();
    }
}

void Options::parseYamlDraw(Yaml &yaml)
{
    gameOptions.draw.enabled = yaml["adjudication"]["draw"]["enabled"].get<bool>();
    gameOptions.draw.moveCount = yaml["adjudication"]["draw"]["movecount"].get<int>();
    gameOptions.draw.moveNumber = yaml["adjudication"]["draw"]["movenumber"].get<int>();
    gameOptions.draw.score = yaml["adjudication"]["draw"]["score"].get<int>();
}

void Options::parseYamlResign(Yaml &yaml)
{
    gameOptions.resign.enabled = yaml["adjudication"]["resign"]["enabled"].get<bool>();
    gameOptions.resign.moveCount = yaml["adjudication"]["resign"]["movecount"].get<int>();
    gameOptions.resign.score = yaml["adjudication"]["resign"]["score"].get<int>();
}

} // namespace CMD
