#pragma once

#ifdef _WIN64

#    include <engine/process/iprocess.hpp>

#    include <cassert>
#    include <chrono>
#    include <cstdint>
#    include <future>
#    include <iostream>
#    include <stdexcept>
#    include <string>
#    include <thread>
#    include <vector>

#    include <windows.h>

#    include <affinity/affinity.hpp>
#    include <globals/globals.hpp>
#    include <util/logger/logger.hpp>
#    include <util/thread_vector.hpp>

namespace fastchess {

extern util::ThreadVector<ProcessInformation> process_list;

namespace atomic {
extern std::atomic_bool stop;
}

namespace engine::process {

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    Status init(const std::string &command, const std::string &args, const std::string &log_name) override {
        command_  = command;
        args_     = args;
        log_name_ = log_name;

        current_line_.reserve(300);

        pi_ = PROCESS_INFORMATION();

        in_pipe_.create();
        out_pipe_.create();

        STARTUPINFOA si = STARTUPINFOA{};
        si.dwFlags      = STARTF_USESTDHANDLES;

        SetHandleInformation(in_pipe_.read_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = in_pipe_.write_end();

        SetHandleInformation(out_pipe_.write_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = out_pipe_.read_end();

        /*
        CREATE_NEW_PROCESS_GROUP flag is important here
        to disable all CTRL+C signals for the new process
        */
        const auto success = CreateProcessA(nullptr, const_cast<char *>((command + " " + args).c_str()), nullptr,
                                            nullptr, TRUE, CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi_);

        // not needed
        out_pipe_.close_read();

        startup_error_ = !success;
        is_initalized_ = true;

        process_list.push(ProcessInformation{pi_.hProcess, in_pipe_.write_end()});

        return success ? Status::OK : Status::ERR;
    }

    [[nodiscard]] Status alive() const noexcept override {
        assert(is_initalized_);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);

        return exitCode == STILL_ACTIVE ? Status::OK : Status::ERR;
    }

    void setAffinity(const std::vector<int> &cpus) noexcept override {
        assert(is_initalized_);
        if (!cpus.empty()) affinity::setAffinity(cpus, pi_.hProcess);
    }

    void killProcess() {
        if (!is_initalized_) return;

        process_list.remove_if([this](const ProcessInformation &pi) { return pi.identifier == pi_.hProcess; });

        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);

        if (exitCode == STILL_ACTIVE) {
            TerminateProcess(pi_.hProcess, 0);
        }

        closeHandles();

        is_initalized_ = false;
    }

    void restart() override {
        Logger::trace<true>("Restarting {}", log_name_);
        killProcess();

        is_initalized_ = false;
        startup_error_ = false;

        init(command_, args_, log_name_);
    }

   protected:
    // Read stdout until the line matches last_word or timeout is reached
    // 0 means no timeout
    Status readProcess(std::vector<Line> &lines, std::string_view last_word,
                       std::chrono::milliseconds threshold) override {
        assert(is_initalized_);

        lines.clear();
        current_line_.clear();

        auto id = std::this_thread::get_id();

        auto read_future =
            std::async(std::launch::async, [this, &last_word, &lines, id, &atomic_stop = atomic::stop]() {
                char buffer[4096];
                DWORD bytes_read;

                while (true) {
                    if (!ReadFile(in_pipe_.read_end(), buffer, sizeof(buffer), &bytes_read, nullptr)) {
                        return Status::ERR;
                    }

                    if (atomic_stop) return Status::ERR;
                    if (bytes_read <= 0) continue;

                    // Iterate over each character in the buffer
                    for (DWORD i = 0; i < bytes_read; i++) {
                        // If we encounter a newline, add the current line to the vector and reset
                        // the current_line_ on Windows newlines are \r\n. Otherwise, append the
                        // character to the current line
                        if (buffer[i] != '\n' && buffer[i] != '\r') {
                            current_line_ += buffer[i];
                            continue;
                        }

                        // don't add empty lines
                        if (current_line_.empty()) continue;

                        const auto time = Logger::should_log_ ? util::time::datetime_precise() : "";

                        lines.emplace_back(Line{current_line_, time});

                        if (realtime_logging_) Logger::readFromEngine(current_line_, time, log_name_, false, id);

                        if (current_line_.rfind(last_word, 0) == 0) return Status::OK;

                        current_line_.clear();
                    }
                }
            });

        // Check if the asynchronous operation is completed
        const auto fut = read_future.wait_for(threshold);

        if (fut == std::future_status::timeout) return Status::TIMEOUT;
        return Status::OK;
    }

    Status writeProcess(const std::string &input) noexcept override {
        assert(is_initalized_);

        if (alive() != Status::OK) killProcess();

        DWORD bytesWritten;
        auto res = WriteFile(out_pipe_.write_end(), input.c_str(), input.length(), &bytesWritten, nullptr);

        return res ? Status::OK : Status::ERR;
    }

   private:
    struct Pipe {
        Pipe() : handles_{}, open_{false, false} {}

        ~Pipe() {
            if (open_[0]) close_read();
            if (open_[1]) close_write();
        }

        void create() {
            if (open_[0]) close_read();
            if (open_[1]) close_write();

            SECURITY_ATTRIBUTES sa = SECURITY_ATTRIBUTES{};
            sa.bInheritHandle      = TRUE;

            if (!CreatePipe(&handles_[0], &handles_[1], &sa, 0)) {
                throw std::runtime_error("CreatePipe() failed");
            }

            open_[0] = open_[1] = true;
        }

        void close_read() {
            assert(open_[0]);
            open_[0] = false;
            CloseHandle(handles_[0]);
        }

        void close_write() {
            assert(open_[1]);
            open_[1] = false;
            CloseHandle(handles_[1]);
        }

        HANDLE read_end() const {
            assert(open_[0]);
            return handles_[0];
        }

        HANDLE write_end() const {
            assert(open_[1]);
            return handles_[1];
        }

       private:
        std::array<HANDLE, 2> handles_;
        std::array<bool, 2> open_;
    };

    void closeHandles() const {
        assert(is_initalized_);

        CloseHandle(pi_.hThread);
        CloseHandle(pi_.hProcess);
    }

    // The command to execute
    std::string command_;
    // The arguments for the engine
    std::string args_;
    // The name in the log file
    std::string log_name_;

    std::string current_line_;

    // True if the process has been initialized
    bool is_initalized_ = false;
    bool startup_error_ = false;

    PROCESS_INFORMATION pi_;

    Pipe in_pipe_ = {}, out_pipe_ = {};
};

}  // namespace engine::process
}  // namespace fastchess

#endif
