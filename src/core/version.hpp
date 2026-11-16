// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace updater::core {

struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;

    auto operator<=>(const Version&) const = default;
};

/// Parses a strict MAJOR.MINOR.PATCH version string, optionally prefixed with 'v' or 'V'.
/// Rejects leading zeros (except '0'), negative numbers, pre-release suffixes, or trailing junk.
[[nodiscard]] std::optional<Version> parse_version(std::string_view text);

/// Parses the output of `hitokoto --version` (e.g. "hitokoto 0.6.2\n").
[[nodiscard]] std::optional<Version> parse_hitokoto_cli_version(std::string_view stdout_text);

/// Formats a Version as "MAJOR.MINOR.PATCH".
[[nodiscard]] std::string to_string(const Version& version);

}  // namespace updater::core
