

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
#endif
}

}  // namespace fastchess