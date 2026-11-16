// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/install_info.hpp"

#include <algorithm>

namespace updater::core {

std::string scope_to_string(InstallScope scope) {
    switch (scope) {
        case InstallScope::User:
            return "per-user";
        case InstallScope::Machine:
            return "per-machine";
        case InstallScope::Unknown:
            return "unknown";
    }
    return "unknown";
}

std::optional<InstalledHitokoto> select_preferred_install(const std::vector<InstalledHitokoto>& candidates) {
    if (candidates.empty()) {
        return std::nullopt;
    }
    const auto best = std::max_element(candidates.begin(), candidates.end(),
                                       [](const InstalledHitokoto& a, const InstalledHitokoto& b) {
                                           if (a.version != b.version) {
                                               return a.version < b.version;
                                           }
                                           return static_cast<int>(a.scope) < static_cast<int>(b.scope);
                                       });
    return *best;
}

std::vector<std::string> build_msiexec_arguments(const std::string& msi_path, const MsiInstallOptions& options) {
    std::vector<std::string> args;
    args.emplace_back("/i");
    args.push_back(msi_path);
    if (options.scope == InstallScope::Machine) {
        args.emplace_back("ALLUSERS=1");
    } else {
        args.emplace_back("MSIINSTALLPERUSER=1");
    }
    if (options.accept_license) {
        args.emplace_back("ACCEPT_LICENSE=yes");
    }
    args.emplace_back(options.silent ? "/qn" : "/qb");
    args.emplace_back("/norestart");
    return args;
}

}  // namespace updater::core
