#include <string>
#include <map>
#include <cassert>
#include <iostream>
#include <utility>

class Yaml {
  private:
    std::map<std::string, Yaml> map;

  public:
    std::string value;
    Yaml *parent = nullptr;

    Yaml() = default;
    explicit Yaml(std::string buffer, Yaml *p) : value(std::move(buffer)), parent(p) {}

    Yaml& operator[](std::string x) {
        return map[x];
    }

    void loadFile(const std::string &filename);

    template <typename T>
    T get()
    {
        assert(!value.empty());

        if constexpr (std::is_same_v<T, int>)
            return std::stoi(value);
        else if constexpr (std::is_same_v<T, float>)
            return std::stof(value);
        else if constexpr (std::is_same_v<T, double>)
            return std::stod(value);
        else
            return value;
    }
};

const std::string defaultYaml =
    "aaa: \"#bbb\" # Comment\n"
    "bbb: ddd\n"
    "asd: dsa\n"
    "ccc: true\n";