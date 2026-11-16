// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/cli_options.hpp"
#include "platform/github_client.hpp"
#include "platform/installed_locator.hpp"
#include "platform/msi_installer.hpp"

#include <filesystem>
#include <functional>
#include <ostream>
#include <string>

namespace updater::cli {

struct RunnerDeps {
    platform::IInstalledLocator& locator;
    platform::IGitHubClient& github;
    platform::IMsiInstaller& installer;
    std::ostream& out;
    std::ostream& err;
    std::function<bool(const std::string& prompt)> confirm;
    std::string app_version = "0.0.0";
    std::filesystem::path temp_root;
};

/// Executes the CLI update flow and returns the process exit code.
[[nodiscard]] int run_cli(const core::CliOptions& options, RunnerDeps& deps);

[[nodiscard]] std::string help_text();

}  // namespace updater::cli
