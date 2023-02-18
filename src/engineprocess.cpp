#include "engineprocess.h"

#ifdef _WIN64

#else

#include <unistd.h>
#include <sys/types.h>

void EngineProcess::initProcess(const std::string &cmd) {

    if (pipe(inPipe) == -1) {
        perror("Failed to create input pipe");
        exit(1);
    }

    if (pipe(outPipe) == -1) {
        perror("Failed to create output pipe");
        exit(1);
    }

    pid_t processPid = fork();
    if (processPid < 0) {
        perror("Fork failed");
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
        perror("Failed to create child process");
        exit(1);
    } else {

        sendCommand("uci");

        /*sleep(1);
        char buffer[500];
        close(inPipe[1]);
        read(inPipe[0], buffer, 500);
        printf("%s\n", buffer);*/
    }
}

void EngineProcess::sendCommand(const std::string &cmd) {
    constexpr char endLine = '\n';
    close(outPipe[0]);
    write(outPipe[1], cmd.c_str(), cmd.size());
    write(outPipe[1], &endLine, 1);
}

void EngineProcess::killProcess() {
    sendCommand("quit");
    close(outPipe[1]);
}

#endif