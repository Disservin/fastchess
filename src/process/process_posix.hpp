#pragma once

#ifndef _WIN64

#include <process/iprocess.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <errno.h>
#include <fcntl.h>  // fcntl
#include <poll.h>   // poll
#include <signal.h>
#include <string.h>
#include <sys/types.h>  // pid_t
#include <sys/wait.h>
#include <unistd.h>  // _exit, fork

#include <util/logger.hpp>
#include <affinity/affinity.hpp>

namespace fast_chess {
inline std::vector<pid_t> pid_list;
}  // namespace fast_chess

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void initProcess(const std::string &command, const std::string &args,
                     const std::string &log_name, const std::vector<int> &cpus) override {
        is_initalized_ = true;
        log_name_      = log_name;

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

        if (forkPid == 0) {
            // This is the child process, set up the pipes and start the engine.

            // Ignore signals, because the main process takes care of them
            signal(SIGINT, SIG_IGN);

            // Redirect the child's standard input to the read end of the output pipe
            if (dup2(out_pipe_[0], 0) == -1)
                throw std::runtime_error("Failed to duplicate outpipe");

            if (close(out_pipe_[0]) == -1) throw std::runtime_error("Failed to close outpipe");

            // Redirect the child's standard output to the write end of the input pipe
            if (dup2(in_pipe_[1], 1) == -1) throw std::runtime_error("Failed to duplicate inpipe");

            if (close(in_pipe_[1]) == -1) throw std::runtime_error("Failed to close inpipe");

            constexpr auto rtrim = [](std::string &s) {
                s.erase(std::find_if(s.rbegin(), s.rend(),
                                     [](unsigned char ch) { return !std::isspace(ch); })
                            .base(),
                        s.end());
            };

            auto full_command = command + " " + args;

            // remove trailing whitespaces
            rtrim(full_command);

// Apple does not support setting the affinity of a pid, so we need to set the
// affinity from within the process
#if defined(__APPLE__)
            // assign the process to specified core
            if (cpus.size()) {
                affinity::set_affinity(cpus);
            }
#endif

            // Execute the engine
            if (execl(command.c_str(), full_command.c_str(), (char *)NULL) == -1)
                throw std::runtime_error("Failed to execute engine");

            _exit(0); /* Note that we do not use exit() */
        } else {
            process_pid_ = forkPid;

#if !defined(__APPLE__)
            // assign the process to specified core
            if (cpus.size()) {
                affinity::set_affinity(cpus, process_pid_);
            }
#endif

            // append the process to the list of running processes
            fast_chess::pid_list.push_back(process_pid_);
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
        fast_chess::pid_list.erase(
            std::remove(fast_chess::pid_list.begin(), fast_chess::pid_list.end(), process_pid_),
            fast_chess::pid_list.end());
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
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold_ms 0 means no timeout
    void readProcess(std::vector<std::string> &lines, std::string_view last_word,
                     int64_t threshold_ms) override {
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
        pollfds[0].fd     = in_pipe_[0];
        pollfds[0].events = POLLIN;

        // Set up the timeout for poll
        int timeoutMillis = threshold_ms;
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
