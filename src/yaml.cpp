#include "yaml.h"

#include <iostream>
#include <stack>
#include <cassert>
#include <fstream>

void Yaml::loadFile(const std::string &filename)
{

    std::ifstream file;
    file.open(filename);

    if (file.is_open()) {
        std::string line;

        Yaml *current = this;
        std::stack<int> indents;
        indents.push(0);
        while (std::getline(file, line)) {

            std::string token;
            std::string buffer;

            bool escaped = false;
            bool string = false;
            int indent = -1;
            for (char c : line) {

                if (c != ' ') {
                    indent = indent < 0 ? -indent - 1 : indent;
                }

                if (escaped || (string && c != '\"')) {
                    buffer += c;
                    escaped = false;
                } else if (c == '#') {
                    break;
                } else if (c == ':') {
                    token = buffer;
                    buffer.clear();
                } else if (c == ' ') {
                    if (indent < 0)
                        indent--;
                    continue;
                } else if (c == '\\') {
                    escaped = true;
                } else if (c == '\"') {
                    string = !string;
                } else {
                    buffer += c;
                }
            }

            // Only non-space characters
            if (indent < 0) {
                continue;
            }

            if (indents.top() == -1) {
                indents.pop();
                indents.push(indent);
            }

            while (indents.top() > indent) {
                current = current->parent;
                indents.pop();
            }

            if (!token.empty()) {
                if (buffer.empty()) {
                    current->map[token] = Yaml(token, current);
                    current = &current->map[token];
                    indents.push(-1);
                } else {
                    current->map[token] = Yaml(buffer, current);
                }
            }
        }
    } else {

        std::cout << "No YAML input file was found!\nCreating default configuration!" << std::endl;
        std::ofstream out;
        out.open(filename);

        out << defaultYaml;

        out.close();
    }

    file.close();
}

bool Yaml::getBool()
{
    assert(!value.empty());
    return value == "true";
}

int Yaml::getInt()
{
    assert(!value.empty());
    return std::stoi(value);
}

double Yaml::getDouble()
{
    assert(!value.empty());
    return std::stod(value);
}

std::string Yaml::getString()
{
    assert(!value.empty());
    return value;
}