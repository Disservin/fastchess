#include <core/globals/globals.hpp>
#ifndef _WIN64
#    include <engine/process/process_posix.hpp>
#endif

#include <doctest/doctest.hpp>

#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace fastchess {

#ifdef __linux__
namespace {

// The pipes a process holds besides its stdin, stdout and stderr ("pipe:[inode]"), read from /proc/<pid>/fd.
std::set<std::string> pipesOf(const std::string& pid) {
    std::set<std::string> pipes;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/proc/" + pid + "/fd", ec)) {
        const auto fd = entry.path().filename().string();
        if (fd == "0" || fd == "1" || fd == "2") continue;
        std::error_code link_error;
        const auto target = std::filesystem::read_symlink(entry.path(), link_error).string();
        if (!link_error && target.rfind("pipe:", 0) == 0) pipes.insert(target);
    }
    return pipes;
}

}  // namespace
#endif

TEST_SUITE("Engine Process") {
    TEST_CASE("Engines started side by side inherit none of each other's pipes") {
#ifdef __linux__
        // Several threads start processes at once, as the games of a tournament start their engines. Each process
        // must hold only its own stdin, stdout and stderr: a pipe end it inherited from another engine's pipes keeps
        // them open, so that when that engine ends, the read of its output never sees the end.
        constexpr int threads    = 8;
        constexpr int per_thread = 8;
        constexpr int rounds     = 8;

        // Pipes the test process already holds, inherited from whatever started it (a CI runner gives it some), are
        // inherited by every child too: only the pipes made while the test runs count.
        const auto outside = pipesOf("self");

        int stray = 0;
        for (int round = 0; round < rounds && stray == 0; ++round) {
            std::vector<std::unique_ptr<engine::process::Process>> processes(threads * per_thread);
            std::vector<std::thread> workers;
            for (int t = 0; t < threads; ++t) {
                workers.emplace_back([&processes, t] {
                    for (int i = 0; i < per_thread; ++i) {
                        auto process = std::make_unique<engine::process::Process>();
                        // Alive long enough to be looked at, then gone by itself: terminate() waits for it.
                        if (process->init("", "sleep", "0.3", "sleep").code == engine::process::Status::OK)
                            processes[t * per_thread + i] = std::move(process);
                    }
                });
            }
            for (auto& worker : workers) worker.join();

            process_list.lock();
            for (const auto& process : process_list)
                for (const auto& pipe : pipesOf(std::to_string(process.identifier)))
                    if (outside.count(pipe) == 0) ++stray;
            process_list.unlock();
        }
        CHECK(stray == 0);
#else
        CHECK(true);
#endif
    }
}

}  // namespace fastchess
