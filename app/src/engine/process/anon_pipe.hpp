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

class Pipe {
   public:
    Pipe() : handles_{INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE} {}

    ~Pipe() {
        close_read();
        close_write();
    }

    void create() {
        close_read();
        close_write();

        SECURITY_ATTRIBUTES sa{};

        sa.nLength        = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        if (!CreatePipeEx(&handles_[0], &handles_[1], &sa)) {
            throw std::runtime_error("CreatePipeEx() failed");
        }
    }

    void close_read() {
        if (handles_[0] != INVALID_HANDLE_VALUE) {
            CloseHandle(handles_[0]);
            handles_[0] = INVALID_HANDLE_VALUE;
        }
    }

    void close_write() {
        if (handles_[1] != INVALID_HANDLE_VALUE) {
            CloseHandle(handles_[1]);
            handles_[1] = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE read_end() const {
        assert(handles_[0] != INVALID_HANDLE_VALUE);
        return handles_[0];
    }

    HANDLE write_end() const {
        assert(handles_[1] != INVALID_HANDLE_VALUE);
        return handles_[1];
    }

   private:
    std::array<HANDLE, 2> handles_;
};

}  // namespace engine::process
}  // namespace fastchess

#endif
