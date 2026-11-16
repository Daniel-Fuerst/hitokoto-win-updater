// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/update_plan.hpp"

#include "core/sha256.hpp"

namespace updater::core {

UpdatePlan plan_update(const std::optional<InstalledHitokoto>& installed, const GitHubRelease& release) {
    UpdatePlan plan;
    plan.installed = installed;
    plan.release = release;

    if (release.draft) {
        plan.status = UpdateStatus::InvalidRelease;
        plan.reason = "Latest release is marked as draft";
        return plan;
    }
    if (release.prerelease) {
        plan.status = UpdateStatus::InvalidRelease;
        plan.reason = "Latest release is a pre-release";
        return plan;
    }

    plan.msi_asset = find_msi_asset(release);
    plan.sha256sums_asset = find_sha256sums_asset(release);

    if (installed.has_value() && release.version <= installed->version) {
        plan.status = UpdateStatus::UpToDate;
        plan.reason = "Installed version is already up to date";
        return plan;
    }

    if (!plan.msi_asset.has_value()) {
        plan.status = UpdateStatus::MissingMsiAsset;
        plan.reason =
            "Release " + release.tag_name + " does not contain hitokoto-" + to_string(release.version) + "-x64.msi";
        return plan;
    }

    plan.status = installed.has_value() ? UpdateStatus::UpdateAvailable : UpdateStatus::NotInstalled;
    plan.reason = installed.has_value() ? "Newer version available" : "hitokoto is not installed";
    return plan;
}

bool verify_expected_sha256(std::string_view actual_hex, const std::optional<std::string>& sums_file_content,
                            std::string_view msi_filename, const std::optional<std::string>& asset_digest,
                            std::string& error_out) {
    std::optional<std::string> sums_expected;
    if (sums_file_content.has_value()) {
        sums_expected = parse_sha256sums(*sums_file_content, msi_filename);
        if (!sums_expected.has_value()) {
            error_out = "SHA256SUMS does not contain an entry for " + std::string(msi_filename);
            return false;
        }
    }

    if (!sums_expected.has_value() && !asset_digest.has_value()) {
        error_out = "No trusted SHA-256 checksum available for " + std::string(msi_filename);
        return false;
    }

    if (sums_expected.has_value() && asset_digest.has_value() && *sums_expected != *asset_digest) {
        error_out = "Checksum mismatch between SHA256SUMS and GitHub asset digest";
        return false;
    }

    if (sums_expected.has_value() && actual_hex != *sums_expected) {
        error_out = "Downloaded MSI SHA-256 (" + std::string(actual_hex) + ") does not match SHA256SUMS (" +
                    *sums_expected + ")";
        return false;
    }

    if (asset_digest.has_value() && actual_hex != *asset_digest) {
        error_out = "Downloaded MSI SHA-256 (" + std::string(actual_hex) + ") does not match GitHub asset digest (" +
                    *asset_digest + ")";
        return false;
    }

    error_out.clear();
    return true;
}

}  // namespace updater::core
