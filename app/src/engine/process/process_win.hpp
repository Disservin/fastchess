#pragma once

#ifdef _WIN64

#    include <engine/process/iprocess.hpp>

#    include <array>
#    include <cassert>
#    include <chrono>
#    include <string>
#    include <string_view>
#    include <system_error>
#    include <vector>

#    include <windows.h>

#    include <affinity/affinity.hpp>
#    include <engine/process/anon_pipe.hpp>
#    include <globals/globals.hpp>
#    include <util/logger/logger.hpp>
#    include <util/thread_vector.hpp>

namespace fastchess {

extern util::ThreadVector<ProcessInformation> process_list;

namespace atomic {
extern std::atomic_bool stop;
}

}  // namespace fastchess

namespace fastchess::engine::process {

namespace detail {

struct AsyncReadContext {
    OVERLAPPED overlapped;
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

    void resetEvent() noexcept { ResetEvent(completion_event); }
};

class ProcessGuard {
   public:
    explicit ProcessGuard() {}
    explicit ProcessGuard(PROCESS_INFORMATION pi) : pi_(pi) {}

    ~ProcessGuard() {
        if (pi_.hThread) CloseHandle(pi_.hThread);
        if (pi_.hProcess) CloseHandle(pi_.hProcess);
    }

    [[nodiscard]] HANDLE handle() const noexcept { return pi_.hProcess; }
    [[nodiscard]] bool valid() const noexcept { return pi_.hProcess != nullptr; }

   private:
    PROCESS_INFORMATION pi_;
};

}  // namespace detail

class Process : public IProcess {
   public:
    static constexpr size_t BUFFER_SIZE = 4096;

    ~Process() override { terminate(); }

    Status init(const std::string& wd, const std::string& command, const std::string& args,
                const std::string& log_name) override {
        try {
            config_ = ProcessConfig{wd, command, args, log_name};
            current_line_.reserve(300);

            in_pipe_.create();
            out_pipe_.create();

            auto si = createStartupInfo();

            if (createProcessInternal(si)) {
                process_list.push(ProcessInformation{process_guard_.handle(), in_pipe_.write_end()});
                return Status::OK;
            }
        } catch (const std::exception& e) {
            Logger::trace<true>("Process creation failed: {}", e.what());
        }

        return Status::ERR;
    }

    [[nodiscard]] Status alive() const noexcept override {
        assert(is_initialized_);

        if (!process_guard_.valid()) return Status::ERR;

        DWORD exit_code = 0;
        auto code       = GetExitCodeProcess(process_guard_.handle(), &exit_code);

        return code && exit_code == STILL_ACTIVE ? Status::OK : Status::ERR;
    }

    void setAffinity(const std::vector<int>& cpus) noexcept override {
        assert(is_initialized_);

        if (!process_guard_.valid() || cpus.empty()) return;
        affinity::setAffinity(cpus, process_guard_.handle());
    }

    void setupRead() override {
        assert(is_initialized_);

        current_line_.clear();
        async_context_ = detail::AsyncReadContext{};
    }

    Status readOutput(std::vector<Line>& lines, std::string_view last_word,
                      std::chrono::milliseconds threshold) override {
        assert(is_initialized_);

        if (!process_guard_.valid()) return Status::ERR;

        lines.clear();
        std::array<char, BUFFER_SIZE> buffer{};

        const auto timeout = threshold.count() == 0 ? INFINITE : static_cast<DWORD>(threshold.count());

        while (true) {
            const auto res = readAsync(buffer.data(), timeout, lines, last_word);

            if (res != Status::NONE) return res;
            if (atomic::stop) return Status::ERR;

            async_context_.resetEvent();
        }
    }

    Status writeInput(const std::string& input) noexcept override {
        assert(is_initialized_);

        if (alive() != Status::OK) {
            terminate();
            return Status::ERR;
        }

        DWORD bytes_written;

        const auto res = WriteFile(              //
            out_pipe_.write_end(),               //
            input.c_str(),                       //
            static_cast<DWORD>(input.length()),  //
            &bytes_written,                      //
            nullptr                              //
        );

        return res ? Status::OK : Status::ERR;
    }

   private:
    struct ProcessConfig {
        std::string working_dir;
        std::string command;
        std::string args;
        std::string log_name;
    };

    void terminate() {
        if (!process_guard_.valid()) return;

        process_list.remove_if([this](const auto& pi) { return pi.identifier == process_guard_.handle(); });

        DWORD exit_code = 0;

        if (GetExitCodeProcess(process_guard_.handle(), &exit_code) && exit_code == STILL_ACTIVE) {
            TerminateProcess(process_guard_.handle(), 0);
        }
    }

    STARTUPINFOA createStartupInfo() const {
        STARTUPINFOA si{};
        si.dwFlags = STARTF_USESTDHANDLES;

        SetHandleInformation(in_pipe_.read_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdOutput = in_pipe_.write_end();

        SetHandleInformation(out_pipe_.write_end(), HANDLE_FLAG_INHERIT, 0);
        si.hStdInput = out_pipe_.read_end();

        return si;
    }

    bool createProcessInternal(STARTUPINFOA& si) {
        const auto cmd = config_.command + " " + config_.args;

        PROCESS_INFORMATION pi{};

        const auto success = CreateProcessA(                                      //
            nullptr,                                                              //
            const_cast<char*>(cmd.c_str()),                                       //
            nullptr,                                                              //
            nullptr,                                                              //
            TRUE,                                                                 //
            CREATE_NEW_PROCESS_GROUP,                                             //
            nullptr,                                                              //
            config_.working_dir.empty() ? nullptr : config_.working_dir.c_str(),  //
            &si,                                                                  //
            &pi                                                                   //
        );

        if (success) {
            is_initialized_ = true;
            process_guard_  = detail::ProcessGuard{pi};
            out_pipe_.close_read();
        }

        return success;
    }

    Status readAsync(char* buffer, DWORD timeout, std::vector<Line>& lines, std::string_view last_word) {
        const auto success = ReadFileEx(      //
            in_pipe_.read_end(),              //
            buffer,                           //
            static_cast<DWORD>(BUFFER_SIZE),  //
            &async_context_.overlapped,       //
            nullptr                           //
        );

        if (!success) {
            return Status::ERR;
        }

        DWORD bytes_transferred = 0;

        const auto result = GetOverlappedResultEx(  //
            in_pipe_.read_end(),                    //
            &async_context_.overlapped,             //
            &bytes_transferred,                     //
            timeout,                                //
            FALSE                                   //
        );

        if (!result) {
            const auto error = GetLastError();

            if (error == WAIT_TIMEOUT) {
                CancelIo(in_pipe_.read_end());
                return Status::TIMEOUT;
            }

            return Status::ERR;
        }

        if (bytes_transferred == 0) return Status::ERR;

        return processBuffer(buffer, bytes_transferred, lines, last_word) ? Status::OK : Status::NONE;
    }

    bool processBuffer(char* buffer, DWORD bytes_read, std::vector<Line>& lines, std::string_view searchword) {
        for (DWORD i = 0; i < bytes_read; ++i) {
            if (buffer[i] != '\n' && buffer[i] != '\r') {
                current_line_ += buffer[i];
                continue;
            }

            if (current_line_.empty()) continue;

            addLine(lines);

            if (current_line_.rfind(searchword, 0) == 0) {
                return true;
            }

            current_line_.clear();
        }

        return false;
    }

    void addLine(std::vector<Line>& lines) const {
        const auto timestamp = Logger::should_log_ ? util::time::datetime_precise() : "";

        lines.emplace_back(Line{current_line_, timestamp});

        if (realtime_logging_) {
            Logger::readFromEngine(current_line_, timestamp, config_.log_name, false, thread_id_);
        }
    }

    ProcessConfig config_;

    detail::ProcessGuard process_guard_;
    detail::AsyncReadContext async_context_;

    Pipe in_pipe_, out_pipe_;

    std::string current_line_;

    bool is_initialized_ = false;

    const std::thread::id thread_id_ = std::this_thread::get_id();
};

}  // namespace fastchess::engine::process

#endif