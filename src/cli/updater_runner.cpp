// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli/updater_runner.hpp"

#include "core/github_release.hpp"
#include "core/install_info.hpp"
#include "core/sha256.hpp"
#include "core/update_plan.hpp"
#include "core/version.hpp"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <random>
#include <sstream>

namespace updater::cli {

namespace {

class TempDirGuard {
public:
    explicit TempDirGuard(std::filesystem::path dir) : dir_(std::move(dir)) {}
    ~TempDirGuard() {
        if (!dir_.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(dir_, ec);
        }
    }
    TempDirGuard(const TempDirGuard&) = delete;
    TempDirGuard& operator=(const TempDirGuard&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const { return dir_; }

private:
    std::filesystem::path dir_;
};

std::filesystem::path create_unique_temp_dir(const std::filesystem::path& root, std::error_code& ec) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::ostringstream name;
    name << "hitokoto-win-updater-" << now << '-' << std::hex << rd();
    const auto dir = root / name.str();
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::string format_megabytes(std::uint64_t bytes) {
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1) << mb << " MB";
    return ss.str();
}

}  // namespace

std::string help_text() {
    return "Usage: hitokoto-win-updater-cli [options]\n"
           "\n"
           "Checks the installed hitokoto version against GitHub Releases and installs\n"
           "the latest Windows x64 MSI package if a newer version is available.\n"
           "\n"
           "Options:\n"
           "  -c, --check           Only check for updates; do not download or install\n"
           "  -y, --yes             Install without prompting if hitokoto is not installed\n"
           "  -s, --silent          Run msiexec silently (/qn instead of /qb)\n"
           "      --repo OWNER/REPO GitHub repository (default: Daniel-Fuerst/hitokoto)\n"
           "  -v, --version         Print updater version and exit\n"
           "  -h, --help            Show this help message and exit\n";
}

int run_cli(const core::CliOptions& options, RunnerDeps& deps) {
    if (options.show_help) {
        deps.out << help_text();
        return 0;
    }
    if (options.show_version) {
        deps.out << "hitokoto-win-updater-cli " << deps.app_version << '\n';
        return 0;
    }

    const auto installed = deps.locator.find_installed();
    if (installed.has_value()) {
        deps.out << "Installed version: " << core::to_string(installed->version) << " ("
                 << core::scope_to_string(installed->scope) << ")\n";
    } else {
        deps.out << "Installed version: not installed\n";
    }

    const std::string user_agent = "hitokoto-win-updater/" + deps.app_version;
    const auto release_resp = deps.github.fetch_latest_release_json(options.repo, user_agent);
    if (!release_resp.ok) {
        deps.err << "Failed to query GitHub releases for " << options.repo << ": " << release_resp.error;
        if (release_resp.status_code > 0) {
            deps.err << " (HTTP " << release_resp.status_code << ")";
        }
        deps.err << '\n';
        return 1;
    }

    const auto parsed_release = core::parse_github_release_json(release_resp.body);
    if (!parsed_release.release.has_value()) {
        deps.err << "Failed to parse GitHub release metadata: " << parsed_release.error << '\n';
        return 1;
    }

    const auto& release = *parsed_release.release;
    deps.out << "Latest version:    " << core::to_string(release.version) << " (" << release.tag_name << ")\n";

    const auto plan = core::plan_update(installed, release);
    switch (plan.status) {
        case core::UpdateStatus::UpToDate:
            deps.out << "hitokoto is up to date.\n";
            return 0;
        case core::UpdateStatus::InvalidRelease:
        case core::UpdateStatus::MissingMsiAsset:
            deps.err << "Cannot update: " << plan.reason << '\n';
            return 1;
        case core::UpdateStatus::UpdateAvailable:
            if (installed.has_value()) {
                deps.out << "Update available: " << core::to_string(installed->version) << " -> "
                         << core::to_string(release.version) << '\n';
            }
            break;
        case core::UpdateStatus::NotInstalled:
            deps.out << "hitokoto " << core::to_string(release.version) << " is available to install.\n";
            break;
    }

    if (options.check_only) {
        return 0;
    }

    if (!plan.msi_asset.has_value()) {
        deps.err << "Cannot update: release MSI asset is missing.\n";
        return 1;
    }

    const bool is_upgrade = (plan.status == core::UpdateStatus::UpdateAvailable);
    if (!is_upgrade && !options.yes) {
        const std::string prompt = "hitokoto is licensed under GPL-3.0-or-later. Download and install hitokoto " +
                                   core::to_string(release.version) + " (per-user)? [y/N] ";
        if (!deps.confirm || !deps.confirm(prompt)) {
            deps.out << "Installation cancelled.\n";
            return 0;
        }
    }

    const auto& msi_asset = plan.msi_asset.value();
    std::optional<std::string> sums_content;
    if (plan.sha256sums_asset.has_value()) {
        deps.out << "Downloading " << plan.sha256sums_asset->name << "...\n";
        const auto sums_resp = deps.github.download_text(plan.sha256sums_asset->download_url, user_agent);
        if (!sums_resp.ok) {
            deps.err << "Failed to download " << plan.sha256sums_asset->name << ": " << sums_resp.error << '\n';
            return 1;
        }
        sums_content = sums_resp.body;
    }

    std::error_code fs_ec;
    const auto base_temp = deps.temp_root.empty() ? std::filesystem::temp_directory_path() : deps.temp_root;
    const auto temp_dir = create_unique_temp_dir(base_temp, fs_ec);
    if (fs_ec) {
        deps.err << "Failed to create temporary directory: " << fs_ec.message() << '\n';
        return 1;
    }
    TempDirGuard temp_guard(temp_dir);

    const auto msi_path = temp_guard.path() / msi_asset.name;
    deps.out << "Downloading " << msi_asset.name << "...\n";
    int last_pct = -1;
    const auto dl_resp = deps.github.download_file(
        msi_asset.download_url, msi_path, user_agent, [&](std::uint64_t received, std::int64_t total) {
            if (total > 0) {
                const int pct = static_cast<int>((received * 100U) / static_cast<std::uint64_t>(total));
                if (pct / 10 != last_pct / 10) {
                    last_pct = pct;
                    deps.out << "  " << pct << "% (" << format_megabytes(received) << " / "
                             << format_megabytes(static_cast<std::uint64_t>(total)) << ")\n";
                }
            }
        });
    if (!dl_resp.ok) {
        deps.err << "Failed to download " << msi_asset.name << ": " << dl_resp.error << '\n';
        return 1;
    }

    const auto actual_sha256 = core::sha256_file_hex(msi_path);
    if (!actual_sha256.has_value()) {
        deps.err << "Failed to read downloaded MSI for SHA-256 verification.\n";
        return 1;
    }

    std::string verify_err;
    if (!core::verify_expected_sha256(*actual_sha256, sums_content, msi_asset.name, msi_asset.sha256_digest,
                                      verify_err)) {
        deps.err << "Checksum verification failed: " << verify_err << '\n';
        return 1;
    }
    deps.out << "SHA-256 verified: " << *actual_sha256 << '\n';

    core::InstallScope scope = core::InstallScope::User;
    if (installed.has_value() && installed->scope != core::InstallScope::Unknown) {
        scope = installed->scope;
    }
    const core::MsiInstallOptions install_opts{
        .silent = options.silent,
        .accept_license = true,
        .is_upgrade = is_upgrade,
        .scope = scope,
    };

    deps.out << "Installing " << msi_asset.name << " (" << core::scope_to_string(scope) << ")...\n";
    const auto install_res = deps.installer.install(msi_path, install_opts);
    if (!install_res.succeeded()) {
        deps.err << "Installation failed";
        if (!install_res.error.empty()) {
            deps.err << ": " << install_res.error;
        } else {
            deps.err << " (msiexec exit code " << install_res.exit_code << ")";
        }
        deps.err << '\n';
        return 1;
    }

    const auto installed_after = deps.locator.find_installed();
    if (!installed_after.has_value() || installed_after->version < release.version) {
        deps.err << "Installation verification failed: expected hitokoto " << core::to_string(release.version)
                 << " or newer, but ";
        if (installed_after.has_value()) {
            deps.err << "version " << core::to_string(installed_after->version) << " is still installed.\n";
        } else {
            deps.err << "hitokoto was not detected as installed.\n";
        }
        return 1;
    }

    deps.out << "hitokoto " << core::to_string(release.version) << " installed successfully.\n";
    if (install_res.exit_code == 3010) {
        deps.out << "A system restart is required to complete the installation.\n";
    }

    return 0;
}

}  // namespace updater::cli
