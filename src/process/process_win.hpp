#pragma once

#ifdef _WIN64

#include <process/iprocess.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <future>

#include <windows.h>

#include <affinity/affinity.hpp>
#include <process/process_list.hpp>
#include <util/logger.hpp>

namespace fast_chess {
extern ProcessList<HANDLE> pid_list;
}  // namespace fast_chess

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void initProcess(const std::string &command, const std::string &args,
                     const std::string &log_name, const std::vector<int> &cpus) override {
        log_name_ = log_name;

        pi_ = PROCESS_INFORMATION();

        is_initalized_  = true;
        STARTUPINFOA si = STARTUPINFOA();
        si.dwFlags      = STARTF_USESTDHANDLES;

        SECURITY_ATTRIBUTES sa = SECURITY_ATTRIBUTES();
        sa.bInheritHandle      = TRUE;

        HANDLE childStdOutRd, childStdOutWr;
        CreatePipe(&childStdOutRd, &childStdOutWr, &sa, 0);
        SetHandleInformation(childStdOutRd, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = childStdOutWr;

        HANDLE childStdInRd, childStdInWr;
        CreatePipe(&childStdInRd, &childStdInWr, &sa, 0);
        SetHandleInformation(childStdInWr, HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = childStdInRd;

        /*
        CREATE_NEW_PROCESS_GROUP flag is important here to disable all CTRL+C signals for the new
        process
        */
        CreateProcessA(nullptr, const_cast<char *>((command + " " + args).c_str()), nullptr,
                       nullptr, TRUE, CREATE_NEW_PROCESS_GROUP, nullptr, nullptr, &si, &pi_);

        CloseHandle(childStdOutWr);
        CloseHandle(childStdInRd);

        child_std_out_ = childStdOutRd;
        child_std_in_  = childStdInWr;

        // set process affinity
        if (!cpus.empty()) {
            affinity::set_affinity(cpus, pi_.hProcess);
        }

        fast_chess::pid_list.push(pi_.hProcess);
    }

    [[nodiscard]] bool isAlive() const override {
        assert(is_initalized_);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);
        return exitCode == STILL_ACTIVE;
    }

    void killProcess() {
        fast_chess::pid_list.remove(pi_.hProcess);

        if (!is_initalized_) return;

        try {
            DWORD exitCode = 0;
            GetExitCodeProcess(pi_.hProcess, &exitCode);

            if (exitCode == STILL_ACTIVE) {
                UINT uExitCode = 0;
                TerminateProcess(pi_.hProcess, uExitCode);
            }

            // Clean up the child process resources
            closeHandles();
        } catch (const std::exception &e) {
            std::cerr << e.what();
        }

        is_initalized_ = false;
    }

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold 0 means no timeout
    Status readProcess(std::vector<std::string> &lines, std::string_view last_word,
                       std::chrono::milliseconds threshold) override {
        assert(is_initalized_);

        lines.clear();
        lines.reserve(30);

        auto readFuture = std::async(std::launch::async, [this, &last_word, &lines]() {
            std::string currentLine;

            char buffer[4096];
            DWORD bytesRead;
            DWORD bytesAvail = 0;

            while (true) {
                if (!ReadFile(child_std_out_, buffer, sizeof(buffer), &bytesRead, nullptr)) {
                    return Status::ERR;
                }

                if (bytesRead <= 0) continue;

                bytesAvail += bytesRead;

                // Iterate over each character in the buffer
                for (DWORD i = 0; i < bytesRead; i++) {
                    // If we encounter a newline, add the current line to the vector and reset
                    // the currentLine on Windows newlines are \r\n. Otherwise, append the
                    // character to the current line
                    if (buffer[i] != '\n' && buffer[i] != '\r') {
                        currentLine += buffer[i];
                        continue;
                    }

                    // don't add empty lines
                    if (currentLine.empty()) continue;

                    // logging will significantly slow down the reading and lead to engine
                    // timeouts
                    fast_chess::Logger::readFromEngine(currentLine, log_name_);

                    lines.emplace_back(currentLine);

                    if (currentLine.rfind(last_word, 0) == 0) {
                        return Status::OK;
                    }

                    currentLine.clear();
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

        if (!isAlive()) {
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

    std::string log_name_;

    bool is_initalized_ = false;

    PROCESS_INFORMATION pi_;
    HANDLE child_std_out_;
    HANDLE child_std_in_;
};

#endif