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

#include <affinity/affinity.hpp>
#include <util/thread_vector.hpp>
#include <util/logger.hpp>

namespace fast_chess {
extern ThreadVector<pid_t> process_list;
}  // namespace fast_chess

class Process : public IProcess {
   public:
    ~Process() override { killProcess(); }

    void init(const std::string &command, const std::string &args,
              const std::string &log_name) override {
        command_  = command;
        args_     = args;
        log_name_ = log_name;

        is_initalized_ = true;

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

            // Execute the engine
            if (execl(command.c_str(), full_command.c_str(), (char *)NULL) == -1)
                throw std::runtime_error("Failed to execute engine");

            _exit(0); /* Note that we do not use exit() */
        }
        // This is the parent process
        else {
            process_pid_ = forkPid;

            // append the process to the list of running processes
            fast_chess::process_list.push(process_pid_);
        }
    }

    bool alive() const override {
        assert(is_initalized_);

        int status;
        const pid_t r = waitpid(process_pid_, &status, WNOHANG);

        if (r == -1) {
            throw std::runtime_error("Error: waitpid() failed");
        } else {
            return r == 0;
        }
    }

    void setAffinity(const std::vector<int> &cpus) override {
        assert(is_initalized_);
        if (!cpus.empty()) {
#if defined(__APPLE__)
// Apple does not support setting the affinity of a pid
#else
            affinity::setAffinity(cpus, process_pid_);
#endif
        }
    }

    void killProcess() {
        fast_chess::process_list.remove(process_pid_);

        if (!is_initalized_) return;

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

    void restart() override {
        killProcess();
        init(command_, args_, log_name_);
    }

   protected:
    /// @brief Read stdout until the line matches last_word or timeout is reached
    /// @param lines
    /// @param last_word
    /// @param threshold_ms 0 means no timeout
    Status readProcess(std::vector<std::string> &lines, std::string_view last_word,
                       std::chrono::milliseconds threshold) override {
        assert(is_initalized_);

        // Disable blocking
        fcntl(in_pipe_[0], F_SETFL, fcntl(in_pipe_[0], F_GETFL) | O_NONBLOCK);

        lines.clear();
        lines.reserve(30);

        std::string currentLine;
        currentLine.reserve(300);

        char buffer[4096];

        struct pollfd pollfds[1];
        pollfds[0].fd     = in_pipe_[0];
        pollfds[0].events = POLLIN;

        // Set up the timeout for poll
        auto timeoutMillis = threshold;
        if (timeoutMillis.count() <= 0) {
            // wait indefinitely
            timeoutMillis = std::chrono::milliseconds(-1);
        }

        // Continue reading output lines until the line matches the specified line or a timeout
        // occurs
        while (true) {
            const int ret = poll(pollfds, 1, timeoutMillis.count());

            if (ret == -1) {
                throw std::runtime_error("Error: poll() failed");
            } else if (ret == 0) {
                // timeout
                lines.emplace_back(currentLine);
                return Status::TIMEOUT;
            } else if (pollfds[0].revents & POLLIN) {
                // input available on the pipe
                const int bytesRead = read(in_pipe_[0], buffer, sizeof(buffer));

                if (bytesRead == -1) {
                    throw std::runtime_error("Error: read() failed");
                }

                // Iterate over each character in the buffer
                for (int i = 0; i < bytesRead; i++) {
                    // append the character to the current line
                    if (buffer[i] != '\n') {
                        currentLine += buffer[i];
                        continue;
                    }

                    // If we encounter a newline, add the current line to the vector and reset the
                    // currentLine
                    // dont add empty lines
                    if (!currentLine.empty()) {
                        fast_chess::Logger::readFromEngine(currentLine, log_name_);
                        lines.emplace_back(currentLine);

                        if (currentLine.rfind(last_word, 0) == 0) {
                            return Status::OK;
                        }

                        currentLine.clear();
                    }
                }
            }
        }
        return Status::OK;
    }

    void writeProcess(const std::string &input) override {
        assert(is_initalized_);
        fast_chess::Logger::writeToEngine(input, log_name_);

        if (!alive()) {
            throw std::runtime_error("IProcess is not alive and write occured with message: " +
                                     input);
        }

        // Write the input and a newline to the output pipe
        if (write(out_pipe_[1], input.c_str(), input.size()) == -1) {
            throw std::runtime_error("Failed to write to pipe");
        }
    }

   private:
    std::string command_;
    std::string args_;
    std::string log_name_;

    bool is_initalized_ = false;

    pid_t process_pid_;
    int in_pipe_[2], out_pipe_[2];
};

#endif
