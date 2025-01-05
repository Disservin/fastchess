

#include "./printing.h"

#include <iostream>

#ifdef _WIN64
#    include <windows.h>
#endif

namespace fastchess {

void setTerminalOutput() {
#ifdef _WIN64
    if (!IsValidCodePage(65001)) {
        std::cerr << "Error: UTF-8 code page (65001) is not valid on this system." << std::endl;
        return;
    }

    if (!SetConsoleOutputCP(65001)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to set console output code page. Error code: " << error << std::endl;
        return;
    }

    if (!SetConsoleCP(65001)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to set console input code page. Error code: " << error << std::endl;
        return;
    }

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Unable to get console handle." << std::endl;
        return;
    }

    DWORD consoleMode;
    if (!GetConsoleMode(hConsole, &consoleMode)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to get console mode. Error code: " << error << std::endl;
        return;
    }

    // Add the ENABLE_VIRTUAL_TERMINAL_PROCESSING flag to support ANSI colors
    consoleMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hConsole, consoleMode)) {
        DWORD error = GetLastError();
        std::cerr << "Failed to set console mode. Error code: " << error << std::endl;
        return;
    }
#endif
}

}  // namespace fastchess