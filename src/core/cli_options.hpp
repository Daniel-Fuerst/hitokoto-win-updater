// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace updater::core {

struct CliOptions {
    bool check_only = false;
    bool yes = false;
    bool silent = false;
    bool show_help = false;
    bool show_version = false;
    std::string repo = "Daniel-Fuerst/hitokoto";
};

struct CliParseResult {
    std::optional<CliOptions> options;
    std::string error;
};

/// Parses command-line arguments (excluding argv[0], or pass full argv[1..argc-1]).
[[nodiscard]] CliParseResult parse_cli_args(const std::vector<std::string>& args);

}  // namespace updater::core
