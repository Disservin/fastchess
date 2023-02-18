#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace std;

int main() {

    while (true) {

        string cmd;
        getline(cin, cmd);

        if (cmd == "quit") {
            break;
        } else if (cmd == "isready") {
            cout << "readyok" << std::endl;
        } else if (cmd == "uci") {
            cout << "line0" << std::endl;
            cout << "line1" << std::endl;
            cout << "uciok" << std::endl;
        } else if (cmd == "sleep") {
#ifdef _WIN32
            Sleep(1);
#else
            sleep(1);
#endif
            cout << "done" << std::endl;
        }
    }

    return 0;
}