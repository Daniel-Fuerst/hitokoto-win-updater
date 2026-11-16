// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/cli_options.hpp"

namespace updater::core {

CliParseResult parse_cli_args(const std::vector<std::string>& args) {
    CliOptions opts;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--check" || arg == "-c") {
            opts.check_only = true;
        } else if (arg == "--yes" || arg == "-y") {
            opts.yes = true;
        } else if (arg == "--silent" || arg == "-s") {
            opts.silent = true;
        } else if (arg == "--help" || arg == "-h") {
            opts.show_help = true;
        } else if (arg == "--version" || arg == "-v") {
            opts.show_version = true;
        } else if (arg == "--repo") {
            if (i + 1 >= args.size() || args[i + 1].empty()) {
                return {.options = std::nullopt, .error = "--repo requires an <owner>/<name> argument"};
            }
            opts.repo = args[++i];
        } else {
            return {.options = std::nullopt, .error = "Unknown argument: " + arg};
        }
    }
    return {.options = std::move(opts), .error = {}};
}

}  // namespace updater::core
