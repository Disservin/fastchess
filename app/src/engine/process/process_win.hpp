#pragma once

#ifdef _WIN64

#    include <engine/process/iprocess.hpp>

#    include <algorithm>
#    include <cassert>
#    include <chrono>
#    include <mutex>
#    include <string>
#    include <thread>
#    include <vector>

#    include <windows.h>

#    include <affinity/affinity.hpp>
#    include <core/globals/globals.hpp>
#    include <core/logger/logger.hpp>
#    include <core/threading/thread_vector.hpp>
#    include <engine/process/anon_pipe.hpp>
#    include <engine/process/line_buffer.hpp>

namespace fastchess {

extern util::ThreadVector<ProcessInformation> process_list;

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

    Result init(const std::string& dir, const std::string& path, const std::string& args,
                const std::string& log_name) override {
        wd_       = dir;
        command_  = getPath(dir, path);
        args_     = args;
        log_name_ = log_name;

        current_line_.reserve(300);
        pi_ = PROCESS_INFORMATION{};

        output_.clear();

        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength              = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle       = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipeEx(&hChildStdoutRead, &hChildStdoutWrite, &saAttr)) {
            return Result::Error("failed to create stdout pipe");
        }

        if (!CreatePipeEx(&hChildStdinRead, &hChildStdinWrite, &saAttr)) {
            CloseHandle(hChildStdoutRead);
            CloseHandle(hChildStdoutWrite);
            return Result::Error("failed to create stdin pipe");
        }

        if (!SetHandleInformation(hChildStdoutRead, HANDLE_FLAG_INHERIT, 0)) {
            closesHandles();
            return Result::Error("failed to set stdout handle information");
        }

        if (!SetHandleInformation(hChildStdinWrite, HANDLE_FLAG_INHERIT, 0)) {
            closesHandles();
            return Result::Error("failed to set stdin handle information");
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
                return Result::OK();
            }
        } catch (const std::exception& e) {
            return Result::Error(e.what());
        }

        return Result::Error("process creation failed");
    }

    void closesHandles() {
        if (hChildStdoutRead) CloseHandle(hChildStdoutRead);
        if (hChildStdoutWrite) CloseHandle(hChildStdoutWrite);
        if (hChildStdinRead) CloseHandle(hChildStdinRead);
        if (hChildStdinWrite) CloseHandle(hChildStdinWrite);
        hChildStdoutRead = hChildStdoutWrite = hChildStdinRead = hChildStdinWrite = NULL;
    }

    [[nodiscard]] Result alive() noexcept override {
        assert(is_initialized_);

        DWORD exitCode = 0;

        return GetExitCodeProcess(pi_.hProcess, &exitCode) && exitCode == STILL_ACTIVE
                   ? Result::OK()
                   : Result::Error("process terminated");
    }

    bool setAffinity(const std::vector<int>& cpus) noexcept override {
        assert(is_initialized_);
        return affinity::setProcessAffinity(cpus, pi_.hProcess);
    }

    void terminate() {
        closesHandles();

        if (!is_initialized_) {
            return;
        }

        process_list.remove_if([this](const auto& pi) { return pi.identifier == pi_.hProcess; });

        DWORD exitCode        = 0;
        const auto start_time = std::chrono::steady_clock::now();

        // give the process time to die gracefully
        while (std::chrono::steady_clock::now() - start_time < IProcess::kill_timeout) {
            GetExitCodeProcess(pi_.hProcess, &exitCode);

            if (exitCode != STILL_ACTIVE) {
                // Process has terminated
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (exitCode == STILL_ACTIVE) {
            LOG_TRACE_THREAD("Force terminating process with pid: {}", pi_.hProcess);
            // the job takes the processes the engine started with it
            if (!job_ || !TerminateJobObject(job_, 1)) TerminateProcess(pi_.hProcess, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            GetExitCodeProcess(pi_.hProcess, &exitCode);
        } else {
            LOG_TRACE_THREAD("Process with pid: {} terminated with status: {}", pi_.hProcess, exitCode);
        }

        // ends the processes the engine started and left running
        if (job_) {
            CloseHandle(job_);
            job_ = NULL;
        }

        if (pi_.hProcess) CloseHandle(pi_.hProcess);
        pi_ = PROCESS_INFORMATION{};

        is_initialized_ = false;
    }

    void setupRead() override { output_.dropPartial(); }

    // Read stdout until the line matches last_word or timeout is reached
    // 0 means no timeout
    Result readOutput(std::vector<Line>& lines, std::string_view last_word,
                      std::chrono::milliseconds threshold) override {
        assert(is_initialized_);

        lines.clear();

        // lines a previous call read after its searchword
        if (takeLines(lines, last_word)) return Result::OK();

        const auto startTime = std::chrono::steady_clock::now();
        // WaitForMultipleObjects takes a DWORD, INFINITE being its largest value: a longer wait is capped rather
        // than truncated (2^32 ms + 1 would be 1 ms).
        constexpr auto max_wait = static_cast<std::chrono::milliseconds::rep>(INFINITE - 1);
        const auto timeout =
            threshold.count() == 0 ? INFINITE : static_cast<DWORD>((std::min)(threshold.count(), max_wait));

        while (true) {
            auto currentTime = std::chrono::steady_clock::now();
            auto elapsedMs   = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();

            if (elapsedMs >= timeout) {
                return Result{Status::TIMEOUT, "timeout"};
            }

            if (atomic::stop) {
                return Result::Error("stop requested");
            }

            DWORD bytesRead;
            OVERLAPPED overlapped = {};
            overlapped.hEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);

            if (!overlapped.hEvent) {
                LOG_FATAL_THREAD("Failed to create event for overlapped I/O");
                return Result::Error("failed to create event for overlapped I/O");
            }

            if (!ReadFile(hChildStdoutRead, buffer.data(), buffer.size(), &bytesRead, &overlapped)) {
                if (GetLastError() != ERROR_IO_PENDING) {
                    CloseHandle(overlapped.hEvent);

                    if (!alive()) {
                        return processTerminated(lines);
                    }

                    LOG_FATAL_THREAD("Failed to read from pipe");
                    return Result::Error("failed to read from pipe");
                }

                HANDLE wait_handles[] = {overlapped.hEvent, pi_.hProcess};
                DWORD waitResult      = WaitForMultipleObjects(2, wait_handles, FALSE, timeout);
                if (waitResult == WAIT_TIMEOUT) {
                    // what was read before the read was cancelled is taken by the next call
                    cancelRead(overlapped);
                    CloseHandle(overlapped.hEvent);
                    return Result{Status::TIMEOUT, "timeout"};
                }

                if (waitResult == WAIT_OBJECT_0 + 1) {
                    cancelRead(overlapped);
                    CloseHandle(overlapped.hEvent);

                    if (!alive()) {
                        return processTerminated(lines);
                    }

                    LOG_FATAL_THREAD("Process handle signaled but process is still alive");
                    return Result::Error("wait failed");
                }

                if (waitResult != WAIT_OBJECT_0) {
                    cancelRead(overlapped);
                    CloseHandle(overlapped.hEvent);
                    LOG_FATAL_THREAD("Wait failed");
                    return Result::Error("wait failed");
                }

                if (!GetOverlappedResult(hChildStdoutRead, &overlapped, &bytesRead, FALSE)) {
                    CloseHandle(overlapped.hEvent);

                    if (!alive()) {
                        return processTerminated(lines);
                    }

                    LOG_FATAL_THREAD("Failed to get overlapped result");
                    return Result::Error("failed to get overlapped result");
                }
            }

            CloseHandle(overlapped.hEvent);

            if (bytesRead == 0) {
                if (!alive()) {
                    return processTerminated(lines);
                }

                continue;
            }

            output_.append({buffer.data(), bytesRead});

            if (takeLines(lines, last_word)) return Result::OK();
        }
    }

    Result writeInput(const std::string& input) noexcept override {
        assert(is_initialized_);

        if (!alive()) {
            return Result::Error("process not alive");
        }

        DWORD bytes_written;
        auto res = WriteFile(hChildStdinWrite, input.c_str(), input.length(), &bytes_written, nullptr);

        return res ? Result::OK() : Result::Error("failed to write to stdin");
    }

   private:
    // The engine has exited, a last line without a newline is still part of its output.
    Result processTerminated(std::vector<Line>& lines) {
        (void)takeLines(lines, "");
        if (output_.takePartial(current_line_)) addLine(lines);

        return Result::Error("process terminated");
    }

    // Cancels a pending read and waits until it has ended, keeping what it read before.
    // Until then Windows may still write to buffer and to overlapped, which lives on the stack of readOutput():
    // the parent keeps a handle to the write end of the pipe, so the read only ends when it is cancelled, at the
    // latest when terminate() closes the pipe, long after readOutput() has returned.
    void cancelRead(OVERLAPPED& overlapped) {
        CancelIoEx(hChildStdoutRead, &overlapped);

        DWORD bytes_read = 0;
        if (GetOverlappedResult(hChildStdoutRead, &overlapped, &bytes_read, TRUE) && bytes_read > 0) {
            output_.append({buffer.data(), bytes_read});
        }
    }

    // Takes the complete lines out of output_, up to the first one starting with searchword. Returns true if it was
    // found, the lines after it stay for the next call.
    [[nodiscard]] bool takeLines(std::vector<Line>& lines, std::string_view searchword) {
        while (output_.next(current_line_)) {
            addLine(lines);

            if (!searchword.empty() && current_line_.rfind(searchword, 0) == 0) return true;
        }

        return false;
    }

    [[nodiscard]] bool createProcess(STARTUPINFOA& si) {
        // Unquoted, Windows splits the path at its spaces: "C:\My Engines\engine.exe" may start C:\My.exe, and the
        // engine gets "C:\My" as argv[0]. A path cannot contain '"', so one that starts with it is already quoted.
        const bool quoted = !command_.empty() && command_.front() == '"';
        const auto cmd    = fmt::format("{} {}", quoted ? command_ : fmt::format("\"{}\"", command_), args_);

        // suspended until it is in its job, so that nothing it starts escapes the job
        constexpr DWORD flags = CREATE_NEW_PROCESS_GROUP | CREATE_SUSPENDED;

        // The engine inherits the error mode: with these flags, an engine that is missing a DLL or crashes exits at
        // once, instead of waiting for someone to close a system error dialog. The error mode of fastchess itself is
        // restored right after; the lock keeps engines started at the same time from restoring it too early.
        static std::mutex error_mode_mutex;
        std::lock_guard<std::mutex> lock(error_mode_mutex);

        const UINT error_mode = GetErrorMode();
        SetErrorMode(error_mode | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

        const auto success = CreateProcessA(      //
            nullptr,                              //
            const_cast<char*>(cmd.c_str()),       //
            nullptr,                              //
            nullptr,                              //
            TRUE,                                 //
            flags,                                //
            nullptr,                              //
            wd_.empty() ? nullptr : wd_.c_str(),  //
            &si,                                  //
            &pi_                                  //
        );

        SetErrorMode(error_mode);

        is_initialized_ = true;

        if (!success) return false;

        assignJob();

        const bool resumed = ResumeThread(pi_.hThread) != static_cast<DWORD>(-1);

        // only needed to resume the process
        CloseHandle(pi_.hThread);
        pi_.hThread = NULL;

        if (!resumed) {
            // a process that never runs would only show up as a timeout later
            if (!job_ || !TerminateJobObject(job_, 1)) TerminateProcess(pi_.hProcess, 1);
            WaitForSingleObject(pi_.hProcess, 1000);

            if (job_) CloseHandle(job_);
            job_ = NULL;

            CloseHandle(pi_.hProcess);
            pi_.hProcess = NULL;

            return false;
        }

        return true;
    }

    // The engine and every process it starts are put in a job of their own, killed when the job handle is closed:
    // by terminate(), or by Windows when fastchess exits, even if it crashes or is killed.
    void assignJob() {
        job_ = CreateJobObjectA(nullptr, nullptr);

        DWORD error = 0;

        if (!job_) {
            error = GetLastError();
        } else {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
            limits.BasicLimitInformation.LimitFlags     = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

            if (SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) &&
                AssignProcessToJobObject(job_, pi_.hProcess)) {
                return;
            }

            error = GetLastError();
            CloseHandle(job_);
            job_ = NULL;
        }

        // the engine still runs, only what it starts may outlive it
        LOG_WARN_THREAD("Failed to put engine {} in a job object (error {}), processes it starts may keep running",
                        log_name_, error);
    }

    void addLine(std::vector<Line>& lines) const {
        const auto timestamp = Logger::should_log_ ? time::datetime_precise() : "";

        lines.emplace_back(Line{current_line_, timestamp});

        if (realtime_logging_) {
            Logger::readFromEngine(current_line_, timestamp, log_name_, false, thread_id);
        }

        if (line_callback_) line_callback_(current_line_);
    }

    // The working directory
    std::string wd_;
    // The command to execute
    std::string command_;
    // The arguments for the engine
    std::string args_;
    // The name in the log file
    std::string log_name_;
    // What was read from the engine and not taken out yet
    LineBuffer output_{"\r\n"};
    // The line taken out of output_ last, avoids reallocations
    std::string current_line_;

    // True if the process has been initialized
    bool is_initialized_ = false;

    PROCESS_INFORMATION pi_;
    HANDLE job_ = NULL;

    const std::thread::id thread_id = std::this_thread::get_id();
};

}  // namespace engine::process
}  // namespace fastchess

#endif
