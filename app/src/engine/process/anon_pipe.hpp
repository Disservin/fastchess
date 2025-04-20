#pragma once

#ifdef _WIN64

#    include <cassert>
#    include <stdexcept>
#    include <string>

#    include <windows.h>

#    include <core/logger/logger.hpp>

namespace fastchess {

namespace engine::process {

inline bool CreatePipeEx(LPHANDLE lpReadPipe, LPHANDLE lpWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes) {
    static std::atomic<long> pipeSerialNumber{0};

    constexpr auto mode      = FILE_FLAG_OVERLAPPED;
    constexpr DWORD pipeSize = 4096;

    const auto id       = GetCurrentProcessId();
    const auto serial   = pipeSerialNumber.fetch_add(1);
    const auto pipeName = fmt::format("\\\\.\\Pipe\\RemoteExeAnon.{:08x}.{:08x}", id, serial);

    assert(pipeName.size() < MAX_PATH);

    HANDLE readPipeHandle = CreateNamedPipeA(  //
        pipeName.c_str(),                      //
        PIPE_ACCESS_INBOUND | mode,            //
        PIPE_TYPE_BYTE | PIPE_WAIT,            //
        1,                                     // Number of pipes
        pipeSize,                              // Output buffer size
        pipeSize,                              // Input buffer size
        120 * 1000,                            // Timeout in ms
        lpPipeAttributes                       //
    );

    if (readPipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    // write pipe
    HANDLE writePipeHandle = CreateFileA(  //
        pipeName.c_str(),                  //
        GENERIC_WRITE,                     //
        0,                                 // No sharing
        lpPipeAttributes,                  //
        OPEN_EXISTING,                     //
        FILE_ATTRIBUTE_NORMAL | mode,      //
        nullptr                            // Template file
    );

    if (writePipeHandle == INVALID_HANDLE_VALUE) {
        CloseHandle(readPipeHandle);
        return false;
    }

    *lpReadPipe  = readPipeHandle;
    *lpWritePipe = writePipeHandle;

    return true;
}

}  // namespace engine::process
}  // namespace fastchess

#endif
