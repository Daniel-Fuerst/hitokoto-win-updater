// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/version.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace updater::core {

struct ReleaseAsset {
    std::string name;
    std::string download_url;
    std::optional<std::string> sha256_digest;
};

struct GitHubRelease {
    Version version;
    std::string tag_name;
    std::string name;
    bool prerelease = false;
    bool draft = false;
    std::vector<ReleaseAsset> assets;
};

struct ReleaseParseResult {
    std::optional<GitHubRelease> release;
    std::string error;
};

/// Parses a GitHub REST API `/repos/<owner>/<repo>/releases/latest` JSON payload.
[[nodiscard]] ReleaseParseResult parse_github_release_json(std::string_view json_text);

/// Finds the Windows x64 MSI asset (`hitokoto-<version>-x64.msi`) in a release.
[[nodiscard]] std::optional<ReleaseAsset> find_msi_asset(const GitHubRelease& release);

/// Finds the `SHA256SUMS` asset in a release.
[[nodiscard]] std::optional<ReleaseAsset> find_sha256sums_asset(const GitHubRelease& release);

}  // namespace updater::core
