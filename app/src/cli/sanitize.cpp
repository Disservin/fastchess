#include "sanitize.hpp"

#include <algorithm>
#include <limits>
#include <thread>
#include <utility>
#include <vector>

#include <core/filesystem/fd_limit.hpp>
#include <core/filesystem/file_system.hpp>
#include <core/logger/logger.hpp>
#include <matchmaking/sprt/sprt.hpp>
#include <types/exception.hpp>

namespace fastchess::cli {

void sanitize(config::Tournament& config) {
    if (config.games > 2) {
        // wrong config, lets try to fix it
        std::swap(config.games, config.rounds);

        if (config.games > 2) {
            throw fastchess_exception("Error: Exceeded -game limit! Must be less than 2");
        }
    }

    // fix wrong config
    if (config.report_penta && config.output == OutputType::CUTECHESS) config.report_penta = false;

    if (config.report_penta && config.games != 2) config.report_penta = false;

    // throw error for invalid sprt config
    if (config.sprt.enabled) {
        SPRT::isValid(config.sprt.alpha, config.sprt.beta, config.sprt.elo0, config.sprt.elo1, config.sprt.model,
                      config.report_penta);
    }

    if (config.concurrency > static_cast<int>(std::thread::hardware_concurrency()) && !config.force_concurrency) {
        throw fastchess_exception("Error: Concurrency exceeds number of CPUs. Use -force-concurrency to override.");
    }

#ifdef _WIN64
    if (config.concurrency > 63 && !fd_limit::isWindows11OrNewer()) {
        Logger::print<Logger::Level::WARN>(
            "A concurrency setting of more than 63 is currently not supported on Windows.\nIf this affects "
            "your system, please open an issue or get in touch with the maintainers.");
        config.concurrency = 63;  // not 64 because we need one thread for the main thread
    }
#else
    if (fd_limit::maxSystemFileDescriptorCount() < fd_limit::minFileDescriptorRequired(config.concurrency)) {
        Logger::print<Logger::Level::WARN>(
            "There aren't enough file descriptors available for the specified concurrency.\nPlease increase the limit "
            "using ulimit -n 65536 for each shell manually or \nadjust the defaults (e.g. /etc/security/limits.conf,"
            "/etc/systemd/system.conf, and/or /etc/systemd/user.conf).\nThe maximum number of file descriptors "
            "required for this configuration is: {}",
            fd_limit::minFileDescriptorRequired(config.concurrency));

        const auto max_supported_concurrency = fd_limit::maxConcurrency(fd_limit::maxSystemFileDescriptorCount());

        Logger::print<Logger::Level::WARN>("Limiting concurrency to: {}", max_supported_concurrency);

        if (max_supported_concurrency < 1) {
            throw fastchess_exception("Error: Not enough file descriptors available for the specified concurrency.");
        }

        config.concurrency = max_supported_concurrency;
    }
#endif

    if (config.variant == VariantType::FRC && config.opening.file.empty()) {
        throw fastchess_exception("Error: Please specify a Chess960 opening book");
    }

    if (config.opening.file.empty()) {
        Logger::print<Logger::Level::WARN>(
            "Warning: No opening book specified! Consider using one, otherwise all games will be "
            "played from the starting position.");
    }

    if (config.opening.format != FormatType::EPD && config.opening.format != FormatType::PGN) {
        Logger::print<Logger::Level::WARN>(
            "Warning: Unknown opening format, {}. All games will be played from the starting position.",
            int(config.opening.format));
    }

    if (config.ratinginterval == 0) config.ratinginterval = std::numeric_limits<int>::max();

    if (config.scoreinterval == 0) config.scoreinterval = std::numeric_limits<int>::max();

    if (config.tb_adjudication.enabled) {
        if (config.tb_adjudication.syzygy_dirs.empty()) {
            throw fastchess_exception("Error: Must provide a ;-separated list of Syzygy tablebase directories.");
        }
    }
}

void sanitize(std::vector<EngineConfiguration>& configs) {
    if (configs.size() < 2) {
        throw fastchess_exception("Error: Need at least two engines to start!");
    }

    for (std::size_t i = 0; i < configs.size(); i++) {
#ifdef _WIN64
        // add .exe if . is not present
        if (configs[i].cmd.find('.') == std::string::npos) {
            configs[i].cmd += ".exe";
        }
#endif

        if (configs[i].name.empty()) {
            throw fastchess_exception("Error; please specify a name for each engine!");
        }

        if ((configs[i].limit.tc.time + configs[i].limit.tc.increment) == 0 && configs[i].limit.tc.fixed_time == 0 &&
            configs[i].limit.nodes == 0 && configs[i].limit.plies == 0) {
            throw fastchess_exception("Error; no TimeControl specified!");
        }

        if ((((configs[i].limit.tc.time + configs[i].limit.tc.increment) != 0) +
             (configs[i].limit.tc.fixed_time != 0)) > 1) {
            throw fastchess_exception("Error; cannot use tc and st together!");
        }

        for (std::size_t j = 0; j < i; j++) {
            if (configs[i].name == configs[j].name) {
                throw fastchess_exception("Error: Engine with the same name are not allowed!: " + configs[i].name);
            }
        }

#ifndef NO_STD_FILESYSTEM
        std::filesystem::path enginePath = configs[i].cmd;
        if (!configs[i].dir.empty()) {
            enginePath = (std::filesystem::path(configs[i].dir) / configs[i].cmd);
        }

        if (!configs[i].dir.empty() || enginePath.is_absolute()) {
            if (!std::filesystem::is_regular_file(enginePath)) {
                throw fastchess_exception("Engine binary does not exist: " + enginePath.string());
            }
        }
#endif
    }
}

}  // namespace fastchess::cli
