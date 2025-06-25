#pragma once

#ifndef _WIN64

#    include <engine/process/iprocess.hpp>

#    include <array>
#    include <cassert>
#    include <chrono>
#    include <cstdint>
#    include <iostream>
#    include <memory>
#    include <optional>
#    include <stdexcept>
#    include <string>
#    include <thread>
#    include <vector>

#    include <errno.h>
#    include <fcntl.h>  // fcntl
#    include <poll.h>   // poll
#    include <signal.h>
#    include <spawn.h>
#    include <string.h>
#    include <sys/types.h>  // pid_t
#    include <sys/wait.h>
#    include <unistd.h>  // _exit, fork
#    include <unistd.h>
#    include <csignal>
#    include <stdexcept>
#    include <string>
#    include <vector>

#    include <affinity/affinity.hpp>
#    include <core/globals/globals.hpp>
#    include <core/logger/logger.hpp>
#    include <core/threading/thread_vector.hpp>
#    include <engine/process/spawn.hpp>

#    include <argv_split.hpp>

namespace fastchess {
extern util::ThreadVector<ProcessInformation> process_list;

namespace engine::process {

class Process : public IProcess {
   public:
    virtual ~Process() override { terminate(); }

    Status init(const std::string &wd, const std::string &command, const std::string &args,
                const std::string &log_name) override {
        assert(!is_initalized_);

        wd_            = wd;
        command_       = command;
        args_          = args;
        log_name_      = log_name;
        is_initalized_ = true;
        startup_error_ = false;

#    ifdef NO_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR
        command_ = getPath(wd_, command_);
#    endif

        current_line_.reserve(300);

        argv_split parser(command_);
        parser.parse(args);

        char *const *execv_argv = (char *const *)parser.argv();

        try {
            int result = spawn_process(command_, execv_argv, wd_, out_pipe_.read_end(), in_pipe_.write_end(),
                                       err_pipe_.write_end(), process_pid_);

            if (result != 0) {
                out_pipe_ = {}; 
                in_pipe_  = {};
                err_pipe_ = {};
                result = spawn_process("./" + command_, execv_argv, wd_, out_pipe_.read_end(), in_pipe_.write_end(),
                                       err_pipe_.write_end(), process_pid_, true);
            }

            if (result != 0) {
                startup_error_ = true;
                LOG_ERR_THREAD("Failed to start process: {}", strerror(result));
                return Status::ERR;
            }
        } catch (const std::exception &e) {
            startup_error_ = true;
            LOG_ERR_THREAD("Failed to start process: {}", e.what());
            return Status::ERR;
        }

        // Append the process to the list of running processes
        // which are killed when the program exits, as a last resort
        process_list.push(ProcessInformation{process_pid_, in_pipe_.write_end()});

        return Status::OK;
    }

    Status alive() noexcept override {
        assert(is_initalized_);

        int status;
        const pid_t r = waitpid(process_pid_, &status, WNOHANG);

        if (r != 0) {
            exit_code_ = status;
        }

        return r == 0 ? Status::OK : Status::ERR;
    }

    std::string signalToString(int status) {
        std::stringstream result;

        if (WIFEXITED(status)) {
            result << "Process exited normally with status " << WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            result << "Process terminated by signal " << WTERMSIG(status) << " (";

            switch (WTERMSIG(status)) {
                case SIGABRT:
                    result << "SIGABRT - Abort";
                    break;
                case SIGALRM:
                    result << "SIGALRM - Alarm clock";
                    break;
                case SIGBUS:
                    result << "SIGBUS - Bus error";
                    break;
                case SIGCHLD:
                    result << "SIGCHLD - Child stopped or terminated";
                    break;
                case SIGCONT:
                    result << "SIGCONT - Continue executing";
                    break;
                case SIGFPE:
                    result << "SIGFPE - Floating point exception";
                    break;
                case SIGHUP:
                    result << "SIGHUP - Hangup";
                    break;
                case SIGILL:
                    result << "SIGILL - Illegal instruction";
                    break;
                case SIGINT:
                    result << "SIGINT - Interrupt";
                    break;
                case SIGKILL:
                    result << "SIGKILL - Kill";
                    break;
                case SIGPIPE:
                    result << "SIGPIPE - Broken pipe";
                    break;
                case SIGQUIT:
                    result << "SIGQUIT - Quit program";
                    break;
                case SIGSEGV:
                    result << "SIGSEGV - Segmentation fault";
                    break;
                case SIGSTOP:
                    result << "SIGSTOP - Stop executing";
                    break;
                case SIGTERM:
                    result << "SIGTERM - Termination";
                    break;
                case SIGTRAP:
                    result << "SIGTRAP - Trace/breakpoint trap";
                    break;
                case SIGTSTP:
                    result << "SIGTSTP - Terminal stop signal";
                    break;
                case SIGTTIN:
                    result << "SIGTTIN - Background process attempting read";
                    break;
                case SIGTTOU:
                    result << "SIGTTOU - Background process attempting write";
                    break;
                case SIGUSR1:
                    result << "SIGUSR1 - User-defined signal 1";
                    break;
                case SIGUSR2:
                    result << "SIGUSR2 - User-defined signal 2";
                    break;
#    ifdef SIGPOLL
                case SIGPOLL:
                    result << "SIGPOLL - Pollable event";
                    break;
#    endif
                case SIGPROF:
                    result << "SIGPROF - Profiling timer expired";
                    break;
                case SIGSYS:
                    result << "SIGSYS - Bad system call";
                    break;
                case SIGURG:
                    result << "SIGURG - Urgent condition on socket";
                    break;
                case SIGVTALRM:
                    result << "SIGVTALRM - Virtual timer expired";
                    break;
                case SIGXCPU:
                    result << "SIGXCPU - CPU time limit exceeded";
                    break;
                case SIGXFSZ:
                    result << "SIGXFSZ - File size limit exceeded";
                    break;
                default:
                    result << "Unknown signal";
            }
            result << ")";

#    ifdef WCOREDUMP
            if (WCOREDUMP(status)) {
                result << " - Core dumped";
            }
#    endif
        } else if (WIFSTOPPED(status)) {
            result << "Process stopped by signal " << WSTOPSIG(status);

            switch (WSTOPSIG(status)) {
                case SIGSTOP:
                    result << " (SIGSTOP - Stop executing)";
                    break;
                case SIGTSTP:
                    result << " (SIGTSTP - Terminal stop signal)";
                    break;
                case SIGTTIN:
                    result << " (SIGTTIN - Background process attempting read)";
                    break;
                case SIGTTOU:
                    result << " (SIGTTOU - Background process attempting write)";
                    break;
                default:
                    result << " (Unknown stop signal)";
            }
        }
        // Not all systems define this
#    ifdef WIFCONTINUED
        else if (WIFCONTINUED(status)) {
            result << "Process continued";
        }
#    endif
        else {
            result << "Unknown status " << status;
        }

        return result.str();
    }

    void setAffinity(const std::vector<int> &cpus) noexcept override {
        assert(is_initalized_);

        if (!cpus.empty()) {
            // Apple does not support setting the affinity of a pid
#    ifndef __APPLE__
            affinity::setAffinity(cpus, process_pid_);
#    endif
        }
    }

    void terminate() {
        if (startup_error_) {
            is_initalized_ = false;
            return;
        }

        if (!is_initalized_) return;

        process_list.remove_if([this](const ProcessInformation &pi) { return pi.identifier == process_pid_; });

        int status = 0;

        if (!exit_code_.has_value()) {
            const pid_t pid = waitpid(process_pid_, &status, WNOHANG);

            // If the process is still running, kill it
            if (pid == 0) {
                kill(process_pid_, SIGKILL);
                wait(nullptr);
            }
        } else {
            status = exit_code_.value();
        }

        LOG_TRACE_THREAD("Terminating process with pid: {} {}", process_pid_, status);

        // log the status of the process
        Logger::readFromEngine(signalToString(status), time::datetime_precise(), log_name_, true);

        is_initalized_ = false;
    }

    void setupRead() override { current_line_.clear(); }

    // Read stdout until the line matches searchword or timeout is reached
    // 0 means no timeout clears the lines vector
    Status readOutput(std::vector<Line> &lines, std::string_view searchword,
                      std::chrono::milliseconds threshold) override {
        assert(is_initalized_);

        lines.clear();

        // Set up the timeout for poll
        if (threshold.count() <= 0) {
            // wait indefinitely
            threshold = std::chrono::milliseconds(-1);
        }

        // We prefer to use poll instead of select because
        // poll is more efficient and select has a filedescriptor limit of 1024
        // which can be a problem when running with a high concurrency
        std::array<pollfd, 2> pollfds;
        pollfds[0].fd     = in_pipe_.read_end();
        pollfds[0].events = POLLIN;

        pollfds[1].fd     = err_pipe_.read_end();
        pollfds[1].events = POLLIN;

        // Continue reading output lines until the line
        // matches the specified searchword or a timeout occurs
        while (true) {
            const int ready = poll(pollfds.data(), pollfds.size(), threshold.count());

            if (atomic::stop) {
                return Status::ERR;
            }

            // error
            if (ready == -1) {
                return Status::ERR;
            }

            // timeout
            if (ready == 0) {
                if (!current_line_.empty()) lines.emplace_back(Line{current_line_, time::datetime_precise()});
                if (realtime_logging_) Logger::readFromEngine(current_line_, time::datetime_precise(), log_name_);

                return Status::TIMEOUT;
            }

            // data on stdin
            if (pollfds[0].revents & POLLIN) {
                const auto bytes_read = read(in_pipe_.read_end(), buffer.data(), sizeof(buffer));

                if (auto status = processBuffer(buffer, bytes_read, lines, searchword); status != Status::NONE)
                    return status;
            }

            // data on stderr, we dont search for searchword here
            if (pollfds[1].revents & POLLIN) {
                const auto bytes_read = read(err_pipe_.read_end(), buffer.data(), sizeof(buffer));

                if (auto status = processBuffer(buffer, bytes_read, lines, ""); status != Status::NONE) return status;
            }
        }

        return Status::OK;
    }

    Status writeInput(const std::string &input) noexcept override {
        assert(is_initalized_);

        if (alive() != Status::OK) return Status::ERR;

        if (write(out_pipe_.write_end(), input.c_str(), input.size()) == -1) return Status::ERR;

        return Status::OK;
    }

   private:
    [[nodiscard]] Status processBuffer(const std::array<char, 4096> &buffer, ssize_t bytes_read,
                                       std::vector<Line> &lines, std::string_view searchword) {
        if (bytes_read == -1) return Status::ERR;

        // Iterate over each character in the buffer
        for (ssize_t i = 0; i < bytes_read; i++) {
            // append the character to the current line
            if (buffer[i] != '\n') {
                current_line_ += buffer[i];
                continue;
            }

            // If we encounter a newline, add the current line
            // to the vector and reset the current_line_.
            // Dont add empty lines
            if (!current_line_.empty()) {
                const auto time = Logger::should_log_ ? time::datetime_precise() : "";

                lines.emplace_back(Line{current_line_, time});

                if (realtime_logging_) Logger::readFromEngine(current_line_, time, log_name_, searchword.empty());
                if (!searchword.empty() && current_line_.rfind(searchword, 0) == 0) return Status::OK;

                current_line_.clear();
            }
        }

        return Status::NONE;
    }

    struct Pipe {
        std::array<int, 2> fds_;

        Pipe() {
            if (pipe(fds_.data()) != 0) throw std::runtime_error("pipe() failed");
        }

        ~Pipe() {
            close(fds_[0]);
            close(fds_[1]);
        }

        int read_end() const { return fds_[0]; }
        int write_end() const { return fds_[1]; }
    };

    // buffer to read into
    std::array<char, 4096> buffer;

    std::string wd_;

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

    // The process id of the engine
    pid_t process_pid_;

    Pipe in_pipe_ = {}, out_pipe_ = {}, err_pipe_ = {};

    std::optional<int> exit_code_;
};
}  // namespace engine::process
}  // namespace fastchess

#endif
