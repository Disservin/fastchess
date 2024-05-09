#pragma once

#ifdef _WIN64

#include <engine/process/iprocess.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <windows.h>

#include <affinity/affinity.hpp>
#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

namespace fast_chess {
extern util::ThreadVector<HANDLE> process_list;
}

namespace fast_chess::engine::process {

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void init(const std::string &command, const std::string &args,
              const std::string &log_name) override {
        command_  = command;
        args_     = args;
        log_name_ = log_name;

        current_line_.reserve(300);

        pi_ = PROCESS_INFORMATION();

        STARTUPINFOA si = STARTUPINFOA();
        si.dwFlags      = STARTF_USESTDHANDLES;

        SECURITY_ATTRIBUTES sa = SECURITY_ATTRIBUTES();
        sa.bInheritHandle      = TRUE;

        HANDLE child_stdout_read, child_stdout_write;
        CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0);
        SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = child_stdout_write;

        HANDLE child_stdin_read, child_stdin_write;
        CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0);
        SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = child_stdin_read;

        /*
        CREATE_NEW_PROCESS_GROUP flag is important here to disable all CTRL+C signals for the new
        process
        */
        CreateProcessA(nullptr, const_cast<char *>((command + " " + args).c_str()), nullptr,
                       nullptr, TRUE, CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi_);

        // not needed
        CloseHandle(child_stdout_write);
        CloseHandle(child_stdin_read);

        child_std_out_ = child_stdout_read;
        child_std_in_  = child_stdin_write;

        fast_chess::process_list.push(pi_.hProcess);
        is_initalized_ = true;
    }

    [[nodiscard]] bool alive() const override {
        assert(is_initalized_);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);
        return exitCode == STILL_ACTIVE;
    }

    void setAffinity(const std::vector<int> &cpus) override {
        assert(is_initalized_);
        if (!cpus.empty()) {
            affinity::setAffinity(cpus, pi_.hProcess);
        }
    }

    void killProcess() {
        if (!is_initalized_) return;
        fast_chess::process_list.remove(pi_.hProcess);

        try {
            DWORD exitCode = 0;
            GetExitCodeProcess(pi_.hProcess, &exitCode);

            if (exitCode == STILL_ACTIVE) {
                UINT uExitCode = 0;
                TerminateProcess(pi_.hProcess, uExitCode);
            }

        } catch (const std::exception &e) {
        }

        is_initalized_ = false;
    }

    void restart() override {
        killProcess();
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

        auto readFuture = std::async(std::launch::async, [this, &last_word, &lines]() {
            char buffer[4096];
            DWORD bytesRead;

            while (true) {
                if (!ReadFile(child_std_out_, buffer, sizeof(buffer), &bytesRead, nullptr)) {
                    return Status::ERR;
                }

                if (bytesRead <= 0) continue;

                // Iterate over each character in the buffer
                for (DWORD i = 0; i < bytesRead; i++) {
                    // If we encounter a newline, add the current line to the vector and reset
                    // the current_line_ on Windows newlines are \r\n. Otherwise, append the
                    // character to the current line
                    if (buffer[i] != '\n' && buffer[i] != '\r') {
                        current_line_ += buffer[i];
                        continue;
                    }

                    // don't add empty lines
                    if (current_line_.empty()) continue;

                    lines.emplace_back(Line{current_line_});

                    if (current_line_.rfind(last_word, 0) == 0) {
                        return Status::OK;
                    }

                    current_line_.clear();
                }
            }
        });

        // Check if the asynchronous operation is completed
        const auto fut = readFuture.wait_for(threshold);

        if (fut == std::future_status::timeout) {
            return Status::TIMEOUT;
        }

        return Status::OK;
    }

    void writeProcess(const std::string &input) override {
        assert(is_initalized_);
        fast_chess::Logger::writeToEngine(input, log_name_);

        if (!alive()) {
            killProcess();
        }

        DWORD bytesWritten;
        WriteFile(child_std_in_, input.c_str(), input.length(), &bytesWritten, nullptr);

        assert(bytesWritten == input.length());
    }

   private:
    void closeHandles() const {
        assert(is_initalized_);
        try {
            CloseHandle(pi_.hThread);
            CloseHandle(pi_.hProcess);

            CloseHandle(child_std_out_);
            CloseHandle(child_std_in_);
        } catch (const std::exception &e) {
            std::cerr << e.what();
        }
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

    PROCESS_INFORMATION pi_;
    HANDLE child_std_out_;
    HANDLE child_std_in_;
};

}  // namespace fast_chess::engine::process

#endif
