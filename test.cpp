#include <windows.h>
#include <thread>

void workerThread(int coreId) {
    DWORD_PTR affinityMask = 1ULL << coreId;
    
    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    
    SetProcessAffinityMask(GetCurrentProcess(), affinityMask);

    CreateProcessA(NULL, "stockfish.exe", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

int main() {
    std::thread t1(workerThread, 0); // Core 0
    
    t1.join();
    
    return 0;
}