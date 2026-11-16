// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/install_info.hpp"

#include <filesystem>
#include <string>

namespace updater::platform {

struct InstallResult {
    bool launched = false;
    int exit_code = -1;
    std::string error;

    [[nodiscard]] bool succeeded() const { return launched && (exit_code == 0 || exit_code == 3010); }
};

class IMsiInstaller {
public:
    virtual ~IMsiInstaller() = default;

    [[nodiscard]] virtual InstallResult install(const std::filesystem::path& msi_path,
                                                const core::MsiInstallOptions& options) const = 0;
};

class WindowsMsiInstaller final : public IMsiInstaller {
public:
    [[nodiscard]] InstallResult install(const std::filesystem::path& msi_path,
                                        const core::MsiInstallOptions& options) const override;
};

}  // namespace updater::platform
