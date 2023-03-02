#include <string>
#include <map>
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

    bool getBool();

    int getInt();

    double getDouble();

    std::string getString();
};

const std::string defaultYaml =
    "aaa: \"#bbb\" # Comment\n"
    "bbb: ddd\n"
    "asd: dsa\n"
    "ccc: true\n";