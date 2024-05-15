#pragma once

#ifndef _WIN64

#include <engine/process/iprocess.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
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
#include <wordexp.h>

#include <affinity/affinity.hpp>
#include <util/logger/logger.hpp>
#include <util/thread_vector.hpp>

#include <argv_split.hpp>

namespace fast_chess {
extern util::ThreadVector<pid_t> process_list;

namespace engine::process {
class Pipes {
    int pipe_[2];

    int open_ = -1;

   public:
    Pipes() {
        if (pipe(pipe_) == -1) {
            throw std::runtime_error("Failed to create pipe.");
        }
    }

    void dup2(int fd, int fd2) {
        if (::dup2(pipe_[fd], fd2) == -1) {
            throw std::runtime_error("Failed to duplicate pipe.");
        }

        close(pipe_[fd]);

        // 1 -> 0, 0 -> 1
        open_ = 1 - fd;
    }

    void setNonBlocking() const noexcept {
        fcntl(get(), F_SETFL, fcntl(get(), F_GETFL) | O_NONBLOCK);
    }

    void setOpen(int fd) noexcept { open_ = fd; }

    [[nodiscard]] int get() const noexcept {
        assert(open_ != -1);
        return pipe_[open_];
    }

    ~Pipes() {
        close(pipe_[0]);
        close(pipe_[1]);
    }
};

class Process : public IProcess {
   public:
    virtual ~Process() override { killProcess(); }

    void init(const std::string &command, const std::string &args,
              const std::string &log_name) override {
        assert(is_initalized_ == false);

        command_  = command;
        args_     = args;
        log_name_ = log_name;

        is_initalized_ = true;

        current_line_.reserve(300);

        // Fork the current process, this makes a copy of the current process
        // and returns the process id of the child process to the parent process.
        pid_t fork_pid = fork();

        if (fork_pid < 0) {
            throw std::runtime_error("Failed to fork process");
        }

        out_pipe_.setOpen(1);
        in_pipe_.setOpen(0);
        err_pipe_.setOpen(0);

        if (fork_pid == 0) {
            // This is the child process, set up the pipes and start the engine.

            // Ignore signals, because the main process takes care of them
            signal(SIGINT, SIG_IGN);

            // Redirect the child's standard input to the read end of the output pipe
            out_pipe_.dup2(0, STDIN_FILENO);

            // Redirect the child's standard output to the write end of the input pipe
            in_pipe_.dup2(1, STDOUT_FILENO);
            err_pipe_.dup2(1, STDERR_FILENO);

            argv_split parser(command);
            parser.parse(args);
            char *const *execv_argv = (char *const *)parser.argv();

            // Execute the engine, execv does not return if successful
            // If it fails, it returns -1 and we throw an exception.
            // Is the _exit(0) necessary?
            // execv is replacing the current process with the new process
            if (execv(command.c_str(), execv_argv) == -1) {
                throw std::runtime_error("Failed to execute engine");
            }

            _exit(0);
        }
        // This is the parent process
        else {
            process_pid_ = fork_pid;

            // append the process to the list of running processes
            process_list.push(process_pid_);
        }
    }

    bool alive() const override {
        assert(is_initalized_);

        int status;
        const pid_t r = waitpid(process_pid_, &status, WNOHANG);

        if (r == -1)
            throw std::runtime_error("Error: waitpid() failed");
        else
            return r == 0;
    }

    std::string signalToString(int status) {
        if (WIFEXITED(status)) {
            return std::to_string(WEXITSTATUS(status));
        } else if (WIFSTOPPED(status)) {
            return strsignal(WSTOPSIG(status));
        } else if (WIFSIGNALED(status)) {
            return strsignal(WTERMSIG(status));
        } else {
            return "Unknown child status";
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
        if (!is_initalized_) return;
        process_list.remove(process_pid_);

        int status;
        const pid_t pid = waitpid(process_pid_, &status, WNOHANG);

        // lgo the status of the process
        Logger::readFromEngine(signalToString(status), log_name_, true);

        // If the process is still running, kill it
        if (pid == 0) {
            kill(process_pid_, SIGKILL);
            wait(nullptr);
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

        // Set up the timeout for poll
        if (threshold.count() <= 0) {
            // wait indefinitely
            threshold = std::chrono::milliseconds(-1);
        }

        char buffer[4096];

        // Disable blocking
        in_pipe_.setNonBlocking();
        err_pipe_.setNonBlocking();

        std::array<pollfd, 2> pollfds;
        pollfds[0].fd     = in_pipe_.get();
        pollfds[0].events = POLLIN;

        pollfds[1].fd     = err_pipe_.get();
        pollfds[1].events = POLLIN;

        // Continue reading output lines until the line matches the specified line or a timeout
        // occurs
        while (true) {
            const int ready = poll(pollfds.data(), pollfds.size(), threshold.count());

            // errors
            if (ready == -1) {
                return Status::ERR;
            }
            // timeout
            else if (ready == 0) {
                lines.emplace_back(Line{current_line_});
                return Status::TIMEOUT;
            }

            // There is data to read
            if (pollfds[0].revents & POLLIN) {
                const auto bytesRead = read(in_pipe_.get(), buffer, sizeof(buffer));

                if (bytesRead == -1) return Status::ERR;

                // Iterate over each character in the buffer
                for (ssize_t i = 0; i < bytesRead; i++) {
                    // append the character to the current line
                    if (buffer[i] != '\n') {
                        current_line_ += buffer[i];
                        continue;
                    }

                    // If we encounter a newline, add the current line
                    // to the vector and reset the current_line_.
                    // Dont add empty lines
                    if (!current_line_.empty()) {
                        lines.emplace_back(Line{current_line_});

                        if (current_line_.rfind(last_word, 0) == 0) {
                            return Status::OK;
                        }

                        current_line_.clear();
                    }
                }
            }

            // There is data to read
            if (pollfds[1].revents & POLLIN) {
                const auto bytesRead = read(err_pipe_.get(), buffer, sizeof(buffer));

                if (bytesRead == -1) return Status::ERR;

                // Iterate over each character in the buffer
                for (ssize_t i = 0; i < bytesRead; i++) {
                    // append the character to the current line
                    if (buffer[i] != '\n') {
                        current_line_ += buffer[i];
                        continue;
                    }

                    // If we encounter a newline, add the current line
                    // to the vector and reset the current_line_.
                    // Dont add empty lines
                    if (!current_line_.empty()) {
                        lines.emplace_back(Line{current_line_, Standard::ERR});

                        current_line_.clear();
                    }
                }
            }
        }

        return Status::OK;
    }

    void writeProcess(const std::string &input) override {
        assert(is_initalized_);
        Logger::writeToEngine(input, log_name_);

        if (!alive()) {
            throw std::runtime_error("IProcess is not alive and write occured with message: " +
                                     input);
        }

        // Write the input and a newline to the output pipe
        if (write(out_pipe_.get(), input.c_str(), input.size()) == -1) {
            throw std::runtime_error("Failed to write to pipe");
        }
    }

   private:
    // The command to execute
    std::string command_;
    // The arguments for the engine
    std::string args_;
    // The name in the log file
    std::string log_name_;

    std::string current_line_;

    // True if the process has been initialized
    bool is_initalized_ = false;

    // The process id of the engine
    pid_t process_pid_;

    Pipes in_pipe_ = {}, out_pipe_ = {}, err_pipe_ = {};
};
}  // namespace engine::process
}  // namespace fast_chess

#endif
