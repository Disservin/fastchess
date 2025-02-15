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

struct AsyncReadContext {
    OVERLAPPED overlapped;
    DWORD bytes_read{0};
    DWORD error_code{0};
    HANDLE completion_event{nullptr};

    AsyncReadContext() {
        ZeroMemory(&overlapped, sizeof(OVERLAPPED));
        completion_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    }

    ~AsyncReadContext() {
        if (completion_event) {
            CloseHandle(completion_event);
        }
    }
};

class Process : public IProcess {
    constexpr static int buffer_size = 4096;

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

        try {
            in_pipe_.create();
            out_pipe_.create();

            auto si = createStartupInfo();

            if (createProcess(si)) {
                process_list.push(ProcessInformation{pi_.hProcess, in_pipe_.write_end()});
                return Status::OK;
            }

        } catch (const std::exception &e) {
            LOG_FATAL_THREAD("Process creation failed: {}", e.what());
        }

        return Status::ERR;
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
        if (!is_initialized_) return;

        process_list.remove_if([this](const auto &pi) { return pi.identifier == pi_.hProcess; });

        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);

        if (exitCode == STILL_ACTIVE) {
            TerminateProcess(pi_.hProcess, 0);
        }

        closeHandles();

        is_initialized_ = false;
    }

    void setupRead() override {
        current_line_.clear();

        async_read_context_ = AsyncReadContext();
    }

    // Read stdout until the line matches last_word or timeout is reached
    // 0 means no timeout
    Status readOutput(std::vector<Line> &lines, std::string_view last_word,
                      std::chrono::milliseconds threshold) override {
        assert(is_initialized_);

        lines.clear();

        if (!async_read_context_.completion_event) {
            return Status::ERR;
        }

        std::array<char, buffer_size> buffer;

        const auto timeout = threshold.count() == 0 ? INFINITE : static_cast<DWORD>(threshold.count());

        while (true) {
            // Start asynchronous read
            BOOL read_result = ReadFileEx(in_pipe_.read_end(), buffer.data(), static_cast<DWORD>(buffer_size),
                                          &async_read_context_.overlapped, nullptr);

            if (!read_result) {
                return Status::ERR;
            }

            DWORD bytes_transferred = 0;

            // Wait for completion or timeout
            BOOL completion_result = GetOverlappedResultEx(in_pipe_.read_end(), &async_read_context_.overlapped,
                                                           &bytes_transferred, timeout, FALSE);

            if (!completion_result) {
                DWORD error = GetLastError();

                if (error == WAIT_TIMEOUT) {
                    CancelIo(in_pipe_.read_end());
                    return Status::TIMEOUT;
                }

                return Status::ERR;
            }

            if (atomic::stop) {
                return Status::ERR;
            }

            if (bytes_transferred == 0) {
                continue;
            }

            if (readBytes(buffer, bytes_transferred, lines, last_word)) {
                return Status::OK;
            }

            // Reset for next iteration
            ResetEvent(async_read_context_.completion_event);
        }
    }

    Status writeInput(const std::string &input) noexcept override {
        assert(is_initialized_);

        if (alive() != Status::OK) terminate();

        DWORD bytes_written;
        auto res = WriteFile(out_pipe_.write_end(), input.c_str(), input.length(), &bytes_written, nullptr);

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

    [[nodiscard]] STARTUPINFOA createStartupInfo() const {
        STARTUPINFOA si = STARTUPINFOA{};
        si.dwFlags      = STARTF_USESTDHANDLES;

        SetHandleInformation(in_pipe_.read_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = in_pipe_.write_end();

        SetHandleInformation(out_pipe_.write_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = out_pipe_.read_end();

        return si;
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

        out_pipe_.close_read();
        is_initialized_ = true;

        return success;
    }

    void closeHandles() const {
        assert(is_initialized_);

        CloseHandle(pi_.hThread);
        CloseHandle(pi_.hProcess);
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

    AsyncReadContext async_read_context_;

    Pipe in_pipe_ = {}, out_pipe_ = {};

    const std::thread::id thread_id = std::this_thread::get_id();
};

}  // namespace engine::process
}  // namespace fastchess

#endif
