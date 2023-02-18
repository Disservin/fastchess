#include <string>

struct EngineProcess {

#ifdef _WIN64

#else
    int inPipe[2], outPipe[2];
#endif

    void sendCommand(const std::string &cmd);
    void initProcess(const std::string &cmd);
    void killProcess();
};

