// SPDX-License-Identifier: GPL-3.0-or-later
#include "cli/updater_runner.hpp"
#include "core/cli_options.hpp"
#include "platform/github_client.hpp"
#include "platform/installed_locator.hpp"
#include "platform/msi_installer.hpp"

#include <QCoreApplication>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifndef UPDATER_VERSION_STRING
#define UPDATER_VERSION_STRING "0.0.0+unknown"
#endif

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("hitokoto-win-updater-cli"));
    QCoreApplication::setApplicationVersion(QStringLiteral(UPDATER_VERSION_STRING));

    std::vector<std::string> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const auto parsed = updater::core::parse_cli_args(args);
    if (!parsed.options) {
        std::cerr << "Error: " << parsed.error << "\n\n" << updater::cli::help_text();
        return 2;
    }

    updater::platform::SystemInstalledLocator locator;
    updater::platform::QtGitHubClient github;
    updater::platform::WindowsMsiInstaller installer;

    updater::cli::RunnerDeps deps{
        .locator = locator,
        .github = github,
        .installer = installer,
        .out = std::cout,
        .err = std::cerr,
        .confirm =
            [](const std::string& prompt) {
                std::cout << prompt << std::flush;
                std::string line;
                if (!std::getline(std::cin, line)) {
                    return false;
                }
                return !line.empty() && (line.front() == 'y' || line.front() == 'Y');
            },
        .app_version = UPDATER_VERSION_STRING,
        .temp_root = std::filesystem::temp_directory_path(),
    };

    return updater::cli::run_cli(*parsed.options, deps);
}
