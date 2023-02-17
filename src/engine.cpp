#include "engine.h"

#include <string.h>

void Engine::setName(const std::string &name)
{
    this->name = name;
}

void Engine::setCmd(const std::string &cmd)
{
    this->cmd = cmd;
}

void Engine::setArgs(const std::string &args)
{
    this->args = args;
}

void Engine::setOptions(const std::vector<std::string> &options)
{
    this->options = options;
}

void Engine::setTc(const TimeControl &tc)
{
    this->tc = tc;
}

std::string Engine::getName() const
{
    return name;
}

std::string Engine::getCmd() const
{
    return cmd;
}

std::string Engine::getArgs() const
{
    return args;
}

std::vector<std::string> Engine::getOptions() const
{
    return options;
}

TimeControl Engine::getTc() const
{
    return tc;
}

void Engine::startProcess() {

    if (pipe(inPipe) == -1) {
        perror("Failed to create input pipe!");
        exit(1);
    }

    if (pipe(outPipe) == -1) {
        perror("Failed to create output pipe!");
        exit(1);
    }

    processPid = fork();
    if (processPid < 0) {
        perror("Fork failed!");
        exit(1);
    }

    if (processPid == 0) {
        dup2(outPipe[0], 0);
        close(outPipe[0]);
        close(outPipe[1]);

        dup2(inPipe[1], 1);
        close(inPipe[0]);
        close(inPipe[1]);
        execlp(cmd.c_str(), cmd.c_str(), (char *)0);
        perror("Failed to create child process!");
        exit(1);
    } else {

        sendCommand("uci");

        sleep(1);
        char buffer[500];
        close(inPipe[1]);
        read(inPipe[0], buffer, 500);
        printf("%s\n", buffer);
    }
}

void Engine::sendCommand(const std::string &command) {
    constexpr char endLine = '\n';
    close(outPipe[0]);
    write(outPipe[1], command.c_str(), command.size());
    write(outPipe[1], &endLine, 1);
}

void Engine::killProcess() {
    sendCommand("quit");
    close(outPipe[1]);
}