// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/installed_locator.hpp"

#include <QByteArray>

namespace updater::platform {

std::optional<core::InstalledHitokoto> SystemInstalledLocator::find_installed() const {
    const QByteArray override_ver = qgetenv("HITOKOTO_UPDATER_SIMULATE_INSTALLED_VERSION");
    if (!override_ver.isEmpty()) {
        if (const auto ver = core::parse_version(override_ver.toStdString())) {
            return core::InstalledHitokoto{
                .version = *ver,
                .scope = core::InstallScope::User,
                .install_location = {},
            };
        }
    }
    return std::nullopt;
}

}  // namespace updater::platform
