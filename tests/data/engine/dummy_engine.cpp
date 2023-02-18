#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace std;

int main()
{

    while (true)
    {
        string cmd;
        getline(cin, cmd);

        if (cmd == "quit")
        {
            break;
        }
        else if (cmd == "isready")
        {
            cout << "readyok" << endl;
        }
        else if (cmd == "uci")
        {
            cout << "line0" << endl;
            cout << "line1" << endl;
            cout << "uciok" << endl;
        }
        else if (cmd == "sleep")
        {
            this_thread::sleep_for(chrono::milliseconds(1000));

            cout << "done" << endl;
        }
    }

    return 0;
}