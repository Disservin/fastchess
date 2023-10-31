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

#include <windows.h>

#include <util/logger.hpp>
#include <affinity/affinity.hpp>
#include <process_list.hpp>

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
        if (cpus.size()) {
            // affinity::set_affinity(cpus, pi_.hProcess);
        }

        fast_chess::pid_list.push(pi_.hProcess);
    }

    bool isAlive() override {
        assert(is_initalized_);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);
        return exitCode == STILL_ACTIVE;
    }

    void killProcess() {
        fast_chess::pid_list.remove(pi_.hProcess);

        if (is_initalized_) {
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
    }

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold_ms 0 means no timeout
    void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                     int64_t threshold_ms) override {
        assert(is_initalized_);

        lines.clear();
        lines.reserve(30);

        std::string currentLine;
        currentLine.reserve(300);

        char buffer[4096];
        DWORD bytesRead;
        DWORD bytesAvail = 0;

        int checkTime = 255;

        timeout_ = false;

        auto start = std::chrono::high_resolution_clock::now();

        while (true) {
            // Check if timeout milliseconds have elapsed
            if (threshold_ms > 0 && checkTime-- == 0) {
                /* To achieve "non blocking" file reading on windows with anonymous pipes the only
                solution that I found was using peeknamedpipe however it turns out this function is
                terribly slow and leads to timeouts for the engines. Checking this only after n runs
                seems to reduce the impact of this. For high concurrency windows setups
                threshold_ms should probably be 0. Using the assumption that the engine works
                rather clean and is able to send the last word.*/
                if (!PeekNamedPipe(child_std_out_, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
                    break;
                }

                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                        .count() > threshold_ms) {
                    lines.emplace_back(currentLine);
                    timeout_ = true;
                    break;
                }

                checkTime = 255;
            }

            // no new bytes to read
            if (threshold_ms > 0 && bytesAvail == 0) continue;

            if (!ReadFile(child_std_out_, buffer, sizeof(buffer), &bytesRead, nullptr)) {
                break;
            }

            // Iterate over each character in the buffer
            for (DWORD i = 0; i < bytesRead; i++) {
                // If we encounter a newline, add the current line to the vector and reset the
                // currentLine on windows newlines are \r\n
                if (buffer[i] == '\n' || buffer[i] == '\r') {
                    // dont add empty lines
                    if (!currentLine.empty()) {
                        // logging will significantly slowdown the reading and lead to engine
                        // timeouts
                        fast_chess::Logger::read(currentLine, std::this_thread::get_id(),
                                                 log_name_);
                        lines.emplace_back(currentLine);

                        if (currentLine.rfind(last_word, 0) == 0) {
                            return;
                        }

                        currentLine = "";
                    }
                }
                // Otherwise, append the character to the current line
                else {
                    currentLine += buffer[i];
                }
            }
        }

        return;
    }

    void writeProcess(const std::string &input) override {
        assert(is_initalized_);
        fast_chess::Logger::write(input, std::this_thread::get_id(), log_name_);

        if (!isAlive()) {
            killProcess();
        }

        DWORD bytesWritten;
        WriteFile(child_std_in_, input.c_str(), input.length(), &bytesWritten, nullptr);
    }

   private:
    void closeHandles() {
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

    PROCESS_INFORMATION pi_;
    HANDLE child_std_out_;
    HANDLE child_std_in_;
};

#endif