// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/msi_installer.hpp"

namespace updater::platform {

InstallResult WindowsMsiInstaller::install(const std::filesystem::path& /*msi_path*/,
                                           const core::MsiInstallOptions& /*options*/) const {
    return {
        .launched = false,
        .exit_code = -1,
        .error = "MSI installation is only supported on Windows",
    };
}

}  // namespace updater::platform
