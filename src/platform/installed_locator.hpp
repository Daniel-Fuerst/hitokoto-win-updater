// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/install_info.hpp"

#include <optional>

namespace updater::platform {

class IInstalledLocator {
public:
    virtual ~IInstalledLocator() = default;

    [[nodiscard]] virtual std::optional<core::InstalledHitokoto> find_installed() const = 0;
};

class SystemInstalledLocator final : public IInstalledLocator {
public:
    [[nodiscard]] std::optional<core::InstalledHitokoto> find_installed() const override;
};

}  // namespace updater::platform
