#include <engine/process/iprocess.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include <doctest/doctest.hpp>

#include <core/logger/logger.hpp>

namespace fastchess::engine::process {
namespace {

class LineAccumulatorHarness : private IProcess {
   public:
    using IProcess::LineAccumulator;

    void setupRead() override {}
    Result init(const std::string&, const std::string&, const std::string&, const std::string&) override {
        return Result::OK();
    }
    Result alive() noexcept override { return Result::OK(); }
    bool setAffinity(const std::vector<int>&) noexcept override { return true; }
    Result readOutput(std::vector<Line>&, std::string_view, std::chrono::milliseconds) override { return Result::OK(); }
    Result writeInput(const std::string&) noexcept override { return Result::OK(); }
};

std::string build_input(size_t info_lines, std::string_view line_ending, bool with_match) {
    std::string input;
    input.reserve(info_lines * 80);

    for (size_t i = 0; i < info_lines; ++i) {
        input += "info depth ";
        input += std::to_string((i % 40) + 1);
        input += " seldepth ";
        input += std::to_string((i % 64) + 1);
        input += " nodes ";
        input += std::to_string((i + 1) * 1024);
        input += " nps ";
        input += std::to_string((i + 1) * 4096);
        input += " score cp ";
        input += std::to_string(static_cast<int>(i % 200) - 100);
        input += " pv e2e4 e7e5 g1f3";
        input += line_ending;
    }

    if (with_match) {
        input += "bestmove e2e4 ponder e7e5";
        input += line_ending;
    }

    return input;
}

struct BenchmarkSummary {
    std::chrono::nanoseconds elapsed{};
    size_t bytes = 0;
    size_t lines = 0;
    bool matched = false;
};

BenchmarkSummary run_benchmark(const std::string& input, std::string_view searchword, size_t chunk_size,
                               size_t iterations, bool cr_is_newline) {
    BenchmarkSummary summary;
    summary.bytes = input.size() * iterations;

    std::vector<Line> lines;
    lines.reserve(4096);

    const auto start = std::chrono::steady_clock::now();

    for (size_t iter = 0; iter < iterations; ++iter) {
        size_t sequence = 0;
        LineAccumulatorHarness::LineAccumulator accumulator(false, Standard::OUTPUT, "bench", &sequence);
        bool matched = false;

        for (size_t offset = 0; offset < input.size(); offset += chunk_size) {
            const size_t size = std::min(chunk_size, input.size() - offset);
            if (accumulator.consume(std::string_view(input.data() + offset, size), lines, searchword, cr_is_newline)) {
                matched = true;
                break;
            }
        }

        while (accumulator.hasCompletedLines()) {
            accumulator.drainOne(lines);
        }

        summary.lines += lines.size();
        summary.matched = matched;
        lines.clear();
    }

    summary.elapsed = std::chrono::steady_clock::now() - start;
    return summary;
}

void print_summary(std::string_view label, const BenchmarkSummary& summary) {
    const auto elapsed_ns = static_cast<double>(summary.elapsed.count());
    const auto mib        = static_cast<double>(summary.bytes) / (1024.0 * 1024.0);
    const auto mib_per_s  = mib / (elapsed_ns / 1'000'000'000.0);
    const auto ns_per_line = summary.lines == 0 ? 0.0 : elapsed_ns / static_cast<double>(summary.lines);

    std::cout << label << ": " << mib_per_s << " MiB/s, " << ns_per_line << " ns/line, lines=" << summary.lines
              << ", matched=" << summary.matched << '\n';
}

}  // namespace

TEST_CASE("line accumulator benchmark" * doctest::skip()) {
    Logger::should_log_ = false;

    const size_t iterations = 2000;
    const auto lf_input     = build_input(256, "\n", true);
    const auto crlf_input   = build_input(256, "\r\n", true);

    const auto lf_small = run_benchmark(lf_input, "bestmove", 32, iterations, false);
    const auto lf_large = run_benchmark(lf_input, "bestmove", 256, iterations, false);
    const auto crlf     = run_benchmark(crlf_input, "bestmove", 64, iterations, true);

    print_summary("lf chunk=32", lf_small);
    print_summary("lf chunk=256", lf_large);
    print_summary("crlf chunk=64", crlf);

    CHECK(lf_small.matched);
    CHECK(lf_large.matched);
    CHECK(crlf.matched);
    CHECK(lf_small.lines > 0);
    CHECK(lf_large.lines > 0);
    CHECK(crlf.lines > 0);
}

}  // namespace fastchess::engine::process
