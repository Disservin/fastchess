/*
MIT License

Copyright (c) 2023 disservin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <logger.hpp>

#ifdef _WIN64
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>  // fcntl
#include <poll.h>   // poll
#include <signal.h>
#include <string.h>
#include <sys/types.h>  // pid_t
#include <sys/wait.h>
#include <unistd.h>  // _exit, fork
#endif

namespace Communication {

class IProcess {
   public:
    virtual ~IProcess() = default;

    // Initialize the engine process
    virtual void initProcess(const std::string &command, const std::string &log_name) = 0;

    // Returns true if the engine process is alive
    virtual bool isAlive() = 0;

    bool timeout() const { return timeout_; }

   protected:
    // Read engine's stdout until the line matches last_word or timeout is reached
    virtual void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                             int64_t timeout_threshold) = 0;

    // Write input to the engine's stdin
    virtual void writeProcess(const std::string &input) = 0;

    std::string log_name_;

    bool is_initalized_ = false;
    bool timeout_ = false;
};

#ifdef _WIN64

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void initProcess(const std::string &command, const std::string &log_name) override {
        log_name_ = log_name;

        pi_ = PROCESS_INFORMATION();

        is_initalized_ = true;
        STARTUPINFOA si = STARTUPINFOA();
        si.dwFlags = STARTF_USESTDHANDLES;

        SECURITY_ATTRIBUTES sa = SECURITY_ATTRIBUTES();
        sa.bInheritHandle = TRUE;

        HANDLE childStdOutRd, childStdOutWr;
        CreatePipe(&childStdOutRd, &childStdOutWr, &sa, 0);
        SetHandleInformation(childStdOutRd, HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = childStdOutWr;

        HANDLE childStdInRd, childStdInWr;
        CreatePipe(&childStdInRd, &childStdInWr, &sa, 0);
        SetHandleInformation(childStdInWr, HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = childStdInRd;

        CreateProcessA(nullptr, const_cast<char *>(command.c_str()), nullptr, nullptr, TRUE, 0,
                       nullptr, nullptr, &si, &pi_);

        CloseHandle(childStdOutWr);
        CloseHandle(childStdInRd);

        child_std_out_ = childStdOutRd;
        child_std_in_ = childStdInWr;
    }

    bool isAlive() override {
        assert(is_initalized_);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi_.hProcess, &exitCode);
        return exitCode == STILL_ACTIVE;
    }

    void killProcess() {
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
    void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                     int64_t timeout_threshold) override {
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
            if (timeout_threshold > 0 && checkTime-- == 0) {
                /* To achieve "non blocking" file reading on windows with anonymous pipes the only
                solution that I found was using peeknamedpipe however it turns out this function is
                terribly slow and leads to timeouts for the engines. Checking this only after n runs
                seems to reduce the impact of this. For high concurrency windows setups
                timeout_threshold should probably be 0. Using the assumption that the engine works
                rather clean and is able to send the last word.*/
                if (!PeekNamedPipe(child_std_out_, nullptr, 0, nullptr, &bytesAvail, nullptr)) {
                    break;
                }

                if (std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::high_resolution_clock::now() - start)
                        .count() > timeout_threshold) {
                    lines.emplace_back(currentLine);
                    timeout_ = true;
                    break;
                }

                checkTime = 255;
            }

            // no new bytes to read
            if (timeout_threshold > 0 && bytesAvail == 0) continue;

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
        };

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

#else

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void initProcess(const std::string &command, const std::string &log_name) override {
        is_initalized_ = true;
        log_name_ = log_name;
        // Create input pipe
        if (pipe(in_pipe_) == -1) {
            throw std::runtime_error("Failed to create input pipe");
        }

        // Create output pipe
        if (pipe(out_pipe_) == -1) {
            throw std::runtime_error("Failed to create output pipe");
        }

        // Fork the current process
        pid_t forkPid = fork();

        if (forkPid < 0) {
            throw std::runtime_error("Failed to fork process");
        }

        // If this is the child process, set up the pipes and start the engine
        if (forkPid == 0) {
            // Redirect the child's standard input to the read end of the output pipe
            if (dup2(out_pipe_[0], 0) == -1)
                throw std::runtime_error("Failed to duplicate outpipe");

            if (close(out_pipe_[0]) == -1) throw std::runtime_error("Failed to close outpipe");

            // Redirect the child's standard output to the write end of the input pipe
            if (dup2(in_pipe_[1], 1) == -1) throw std::runtime_error("Failed to duplicate inpipe");

            if (close(in_pipe_[1]) == -1) throw std::runtime_error("Failed to close inpipe");

            // Execute the engine
            if (execl(command.c_str(), command.c_str(), (char *)NULL) == -1)
                throw std::runtime_error("Failed to execute engine");

            _exit(0); /* Note that we do not use exit() */
        } else {
            process_pid_ = forkPid;
        }
    }

    bool isAlive() override {
        assert(is_initalized_);
        int status;

        const pid_t r = waitpid(process_pid_, &status, WNOHANG);
        if (r == -1) {
            throw std::runtime_error("Error: waitpid() failed");
        } else {
            return r == 0;
        }
    }

    void killProcess() {
        if (is_initalized_) {
            close(in_pipe_[0]);
            close(in_pipe_[1]);
            close(out_pipe_[0]);
            close(out_pipe_[1]);

            int status;
            pid_t r = waitpid(process_pid_, &status, WNOHANG);

            if (r == 0) {
                kill(process_pid_, SIGKILL);
                wait(NULL);
            }
        }
    }

   protected:
    void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                     int64_t timeout_threshold) override {
        assert(is_initalized_);

        // Disable blocking
        fcntl(in_pipe_[0], F_SETFL, fcntl(in_pipe_[0], F_GETFL) | O_NONBLOCK);

        lines.clear();
        lines.reserve(30);

        std::string currentLine;
        currentLine.reserve(300);

        char buffer[4096];
        timeout_ = false;

        struct pollfd pollfds[1];
        pollfds[0].fd = in_pipe_[0];
        pollfds[0].events = POLLIN;

        // Set up the timeout for poll
        int timeoutMillis = timeout_threshold;
        if (timeoutMillis <= 0) {
            timeoutMillis = -1;  // wait indefinitely
        }

        // Continue reading output lines until the line matches the specified line or a timeout
        // occurs
        while (true) {
            const int ret = poll(pollfds, 1, timeoutMillis);

            if (ret == -1) {
                throw std::runtime_error("Error: poll() failed");
            } else if (ret == 0) {
                // timeout
                lines.emplace_back(currentLine);
                timeout_ = true;
                break;
            } else if (pollfds[0].revents & POLLIN) {
                // input available on the pipe
                const int bytesRead = read(in_pipe_[0], buffer, sizeof(buffer));

                if (bytesRead == -1) {
                    throw std::runtime_error("Error: read() failed");
                }

                // Iterate over each character in the buffer
                for (int i = 0; i < bytesRead; i++) {
                    // If we encounter a newline, add the current line to the vector and reset the
                    // currentLine
                    if (buffer[i] == '\n') {
                        // dont add empty lines
                        if (!currentLine.empty()) {
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
        }
        return;
    }

    void writeProcess(const std::string &input) override {
        assert(is_initalized_);
        fast_chess::Logger::write(input, std::this_thread::get_id(), log_name_);

        if (!isAlive()) {
            throw std::runtime_error("IProcess is not alive and write occured with message: " +
                                     input);
        }

        // Write the input and a newline to the output pipe
        if (write(out_pipe_[1], input.c_str(), input.size()) == -1) {
            throw std::runtime_error("Failed to write to pipe");
        }
    }

   private:
    pid_t process_pid_;
    int in_pipe_[2], out_pipe_[2];
};

#endif

}  // namespace Communication