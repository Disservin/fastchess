#pragma once

#ifdef _WIN64

#    include <engine/process/iprocess.hpp>

#    include <cassert>
#    include <chrono>
#    include <string>
#    include <thread>
#    include <vector>

#    include <windows.h>

#    include <affinity/affinity.hpp>
#    include <core/globals/globals.hpp>
#    include <core/logger/logger.hpp>
#    include <core/threading/thread_vector.hpp>
#    include <engine/process/anon_pipe.hpp>

namespace fastchess {

extern util::ThreadVector<ProcessInformation> process_list;

namespace atomic {
extern std::atomic_bool stop;
}

namespace engine::process {

class Process : public IProcess {
    constexpr static int buffer_size = 4096;

    HANDLE hChildStdoutRead  = NULL;
    HANDLE hChildStdoutWrite = NULL;
    HANDLE hChildStdinRead   = NULL;
    HANDLE hChildStdinWrite  = NULL;
    std::array<char, buffer_size> buffer;

   public:
    ~Process() override {
        LOG_TRACE_THREAD("Process destructor called for {} with pid {}", log_name_, pi_.dwProcessId);
        terminate();
    }

    Status init(const std::string &dir, const std::string &path, const std::string &args,
                const std::string &log_name) override {
        wd_       = dir;
        command_  = getPath(dir, path);
        args_     = args;
        log_name_ = log_name;

        current_line_.reserve(300);
        pi_ = PROCESS_INFORMATION{};

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle       = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipeEx(&hChildStdoutRead, &hChildStdoutWrite, &saAttr)) {
            throw std::runtime_error("Failed to create stdout pipe");
        }

        if (!CreatePipeEx(&hChildStdinRead, &hChildStdinWrite, &saAttr)) {
            CloseHandle(hChildStdoutRead);
            CloseHandle(hChildStdoutWrite);
            throw std::runtime_error("Failed to create stdin pipe");
        }

        if (!SetHandleInformation(hChildStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
            closeHandles();
            throw std::runtime_error("Failed to set stdout handle information");
        }

        if (!SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
            closeHandles();
            throw std::runtime_error("Failed to set stdin handle information");
        }

        try {
            STARTUPINFOA si = {};
            si.cb           = sizeof(STARTUPINFOA);
            si.hStdOutput   = hChildStdoutWrite;
            si.hStdInput    = hChildStdinRead;
            si.hStdError    = hChildStdoutWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;

            if (createProcess(si)) {
                process_list.push(ProcessInformation{pi_.hProcess, hChildStdoutWrite});
                return Status::OK;
            }

        } catch (const std::exception &e) {
            LOG_FATAL_THREAD("Process creation failed: {}", e.what());
        }

        return Status::ERR;
    }

    void closeHandles() {
        if (hChildStdoutRead) CloseHandle(hChildStdoutRead);
        if (hChildStdoutWrite) CloseHandle(hChildStdoutWrite);
        if (hChildStdinRead) CloseHandle(hChildStdinRead);
        if (hChildStdinWrite) CloseHandle(hChildStdinWrite);
        hChildStdoutRead = hChildStdoutWrite = hChildStdinRead = hChildStdinWrite = NULL;
    }

    [[nodiscard]] Status alive() noexcept override {
        assert(is_initialized_);

        DWORD exitCode = 0;

        return GetExitCodeProcess(pi_.hProcess, &exitCode) && exitCode == STILL_ACTIVE ? Status::OK : Status::ERR;
    }

    void setAffinity(const std::vector<int> &cpus) noexcept override {
        assert(is_initialized_);
        if (!cpus.empty()) affinity::setAffinity(cpus, pi_.hProcess);
    }

    void terminate() {
        closeHandles();

        if (!is_initialized_) {
            return;
        }

        process_list.remove_if([this](const auto &pi) { return pi.identifier == pi_.hProcess; });

        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);

        if (exitCode == STILL_ACTIVE) {
            TerminateProcess(pi_.hProcess, 0);
        }

        is_initialized_ = false;
    }

    void setupRead() override { current_line_.clear(); }

    // Read stdout until the line matches last_word or timeout is reached
    // 0 means no timeout
    Status readOutput(std::vector<Line> &lines, std::string_view last_word,
                      std::chrono::milliseconds threshold) override {
        assert(is_initialized_);

        lines.clear();

        const auto startTime = std::chrono::steady_clock::now();
        const auto timeout   = threshold.count() == 0 ? INFINITE : static_cast<DWORD>(threshold.count());

        while (true) {
            if (std::chrono::steady_clock::now() - startTime > threshold) {
                return Status::TIMEOUT;
            }

            HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
            if (!hEvent) {
                throw std::runtime_error("Failed to create event");
            }

            OVERLAPPED overlapped = {};
            overlapped.hEvent     = hEvent;

            DWORD bytesRead = 0;
            BOOL readResult = ReadFile(hChildStdoutRead, buffer.data(), buffer_size, &bytesRead, &overlapped);

            if (atomic::stop) goto cleanup_return_err;
            if (readResult) goto process_data;

            switch (GetLastError()) {
                case ERROR_IO_PENDING:
                    if (WaitForSingleObject(hEvent, timeout) != WAIT_OBJECT_0) {
                        if (GetLastError() == WAIT_TIMEOUT) CancelIo(hChildStdoutRead);
                        goto cleanup_continue;
                    }

                    if (!GetOverlappedResult(hChildStdoutRead, &overlapped, &bytesRead, FALSE)) {
                        if (GetLastError() == ERROR_BROKEN_PIPE) goto cleanup_continue;
                        goto cleanup_throw;
                    }

                    break;

                case ERROR_BROKEN_PIPE:
                    goto cleanup_continue;

                default:
                    goto cleanup_throw;
            }

        process_data:
            CloseHandle(hEvent);
            if (bytesRead > 0 && readBytes(buffer, bytesRead, lines, last_word)) {
                return Status::OK;
            }
            continue;

        cleanup_continue:
            CloseHandle(hEvent);
            continue;

        cleanup_return_err:
            CloseHandle(hEvent);
            return Status::ERR;

        cleanup_throw:
            CloseHandle(hEvent);
            throw std::runtime_error("Read operation failed");
        }
    }

    Status writeInput(const std::string &input) noexcept override {
        assert(is_initialized_);

        if (alive() != Status::OK) terminate();

        DWORD bytes_written;
        auto res = WriteFile(hChildStdinWrite, input.c_str(), input.length(), &bytes_written, nullptr);

        return res ? Status::OK : Status::ERR;
    }

   private:
    [[nodiscard]] bool readBytes(const std::array<char, buffer_size> &buffer, DWORD bytes_read,
                                 std::vector<Line> &lines, std::string_view searchword) {
        for (DWORD i = 0; i < bytes_read; ++i) {
            // Check for newline characters; Windows uses \r\n as a line delimiter
            if (buffer[i] != '\n' && buffer[i] != '\r') {
                current_line_ += buffer[i];
                continue;
            }

            if (current_line_.empty()) {
                continue;
            }

            addLine(lines);

            // Check if the current line starts with the search word
            if (current_line_.rfind(searchword, 0) == 0) {
                return true;
            }

            current_line_.clear();
        }

        return false;
    }

    [[nodiscard]] bool createProcess(STARTUPINFOA &si) {
        const auto cmd = command_ + " " + args_;

        const auto success = CreateProcessA(      //
            nullptr,                              //
            const_cast<char *>(cmd.c_str()),      //
            nullptr,                              //
            nullptr,                              //
            TRUE,                                 //
            CREATE_NEW_PROCESS_GROUP,             //
            nullptr,                              //
            wd_.empty() ? nullptr : wd_.c_str(),  //
            &si,                                  //
            &pi_                                  //
        );

        is_initialized_ = true;

        return success;
    }

    void addLine(std::vector<Line> &lines) const {
        const auto timestamp = Logger::should_log_ ? util::time::datetime_precise() : "";

        lines.emplace_back(Line{current_line_, timestamp});

        if (realtime_logging_) {
            Logger::readFromEngine(current_line_, timestamp, log_name_, false, thread_id);
        }
    }

    // The working directory
    std::string wd_;
    // The command to execute
    std::string command_;
    // The arguments for the engine
    std::string args_;
    // The name in the log file
    std::string log_name_;
    // The current line read from the engine, avoids reallocations
    std::string current_line_;

    // True if the process has been initialized
    bool is_initialized_ = false;

    PROCESS_INFORMATION pi_;

    const std::thread::id thread_id = std::this_thread::get_id();
};

}  // namespace engine::process
}  // namespace fastchess

#endif
