// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/github_release.hpp"
#include "core/install_info.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace updater::core {

enum class UpdateStatus {
    UpToDate,
    UpdateAvailable,
    NotInstalled,
    MissingMsiAsset,
    InvalidRelease,
};

struct UpdatePlan {
    UpdateStatus status = UpdateStatus::InvalidRelease;
    std::optional<InstalledHitokoto> installed;
    std::optional<GitHubRelease> release;
    std::optional<ReleaseAsset> msi_asset;
    std::optional<ReleaseAsset> sha256sums_asset;
    std::string reason;
};

/// Decides whether an update/install is available based on the installed state and GitHub release.
[[nodiscard]] UpdatePlan plan_update(const std::optional<InstalledHitokoto>& installed, const GitHubRelease& release);

/// Verifies that `actual_hex` matches all available trusted checksum sources (`SHA256SUMS` and/or
/// the GitHub asset `digest`), requiring at least one source to be present.
[[nodiscard]] bool verify_expected_sha256(std::string_view actual_hex,
                                          const std::optional<std::string>& sums_file_content,
                                          std::string_view msi_filename, const std::optional<std::string>& asset_digest,
                                          std::string& error_out);

}  // namespace updater::core
