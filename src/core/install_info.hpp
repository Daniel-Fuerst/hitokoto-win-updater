// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/version.hpp"

#include <optional>
#include <string>
#include <vector>

namespace updater::core {

enum class InstallScope {
    Unknown,
    User,
    Machine,
};

struct InstalledHitokoto {
    Version version;
    InstallScope scope = InstallScope::Unknown;
    std::string install_location;
};

struct MsiInstallOptions {
    bool silent = false;
    bool accept_license = false;
    bool is_upgrade = false;
    InstallScope scope = InstallScope::User;
};

/// Returns a human-readable label for InstallScope ("per-user", "per-machine", "unknown").
[[nodiscard]] std::string scope_to_string(InstallScope scope);

/// Selects the preferred installation when multiple candidates are discovered.
[[nodiscard]] std::optional<InstalledHitokoto> select_preferred_install(
    const std::vector<InstalledHitokoto>& candidates);

/// Builds the argument list to pass to `msiexec.exe` for installing or upgrading hitokoto.
[[nodiscard]] std::vector<std::string> build_msiexec_arguments(const std::string& msi_path,
                                                               const MsiInstallOptions& options);

}  // namespace updater::core
