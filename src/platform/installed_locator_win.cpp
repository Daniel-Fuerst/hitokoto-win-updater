// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/installed_locator.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <QProcess>
#include <QProcessEnvironment>
#include <QString>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace updater::platform {

namespace {

struct RegKeyGuard {
    HKEY key = nullptr;
    ~RegKeyGuard() {
        if (key != nullptr) {
            RegCloseKey(key);
        }
    }
    RegKeyGuard(const RegKeyGuard&) = delete;
    RegKeyGuard& operator=(const RegKeyGuard&) = delete;
    RegKeyGuard() = default;
};

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int needed =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), needed, nullptr, nullptr);
    return out;
}

std::optional<std::wstring> read_reg_string(HKEY key, const wchar_t* value_name) {
    DWORD type = 0;
    DWORD byte_size = 0;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &byte_size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    if ((type != REG_SZ && type != REG_EXPAND_SZ) || byte_size < sizeof(wchar_t)) {
        return std::nullopt;
    }
    std::wstring buf(byte_size / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(key, value_name, nullptr, &type, reinterpret_cast<LPBYTE>(buf.data()), &byte_size) !=
        ERROR_SUCCESS) {
        return std::nullopt;
    }
    while (!buf.empty() && buf.back() == L'\0') {
        buf.pop_back();
    }
    return buf;
}

void scan_uninstall_hive(HKEY root, REGSAM sam_flags, core::InstallScope scope,
                         std::vector<core::InstalledHitokoto>& out) {
    RegKeyGuard uninstall_key;
    constexpr const wchar_t* kUninstallPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    if (RegOpenKeyExW(root, kUninstallPath, 0, KEY_READ | sam_flags, &uninstall_key.key) != ERROR_SUCCESS) {
        return;
    }

    DWORD subkey_count = 0;
    DWORD max_subkey_len = 0;
    if (RegQueryInfoKeyW(uninstall_key.key, nullptr, nullptr, nullptr, &subkey_count, &max_subkey_len, nullptr, nullptr,
                         nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
        return;
    }

    std::wstring name_buf(static_cast<std::size_t>(max_subkey_len) + 1U, L'\0');
    for (DWORD idx = 0; idx < subkey_count; ++idx) {
        DWORD name_len = max_subkey_len + 1U;
        if (RegEnumKeyExW(uninstall_key.key, idx, name_buf.data(), &name_len, nullptr, nullptr, nullptr, nullptr) !=
            ERROR_SUCCESS) {
            continue;
        }
        const std::wstring subkey_name(name_buf.data(), name_len);
        RegKeyGuard entry;
        if (RegOpenKeyExW(uninstall_key.key, subkey_name.c_str(), 0, KEY_READ | sam_flags, &entry.key) !=
            ERROR_SUCCESS) {
            continue;
        }
        const auto display_name = read_reg_string(entry.key, L"DisplayName");
        if (!display_name || *display_name != L"hitokoto") {
            continue;
        }
        const auto display_version = read_reg_string(entry.key, L"DisplayVersion");
        if (!display_version) {
            continue;
        }
        const auto parsed_ver = core::parse_version(wide_to_utf8(*display_version));
        if (!parsed_ver) {
            continue;
        }
        core::InstalledHitokoto info;
        info.version = *parsed_ver;
        info.scope = scope;
        if (const auto loc = read_reg_string(entry.key, L"InstallLocation")) {
            info.install_location = wide_to_utf8(*loc);
        }
        out.push_back(std::move(info));
    }
}

void probe_executable_fallback(const std::filesystem::path& exe_path, core::InstallScope scope,
                               std::vector<core::InstalledHitokoto>& out) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(exe_path, ec)) {
        return;
    }
    QProcess proc;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_COMMAND_LINE_PARSER_NO_GUI_MESSAGE_BOXES"), QStringLiteral("1"));
    proc.setProcessEnvironment(env);
    proc.start(QString::fromStdWString(exe_path.wstring()), {QStringLiteral("--version")});
    if (!proc.waitForFinished(5000) || proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        return;
    }
    const QByteArray stdout_bytes = proc.readAllStandardOutput();
    const auto parsed = core::parse_hitokoto_cli_version(stdout_bytes.toStdString());
    if (!parsed) {
        return;
    }
    out.push_back({
        .version = *parsed,
        .scope = scope,
        .install_location = wide_to_utf8(exe_path.parent_path().wstring()),
    });
}

std::optional<std::filesystem::path> env_path(const wchar_t* name) {
    DWORD len = GetEnvironmentVariableW(name, nullptr, 0);
    if (len == 0) {
        return std::nullopt;
    }
    std::wstring buf(len, L'\0');
    DWORD written = GetEnvironmentVariableW(name, buf.data(), len);
    if (written == 0 || written >= len) {
        return std::nullopt;
    }
    buf.resize(written);
    return std::filesystem::path(buf);
}

}  // namespace

std::optional<core::InstalledHitokoto> SystemInstalledLocator::find_installed() const {
    std::vector<core::InstalledHitokoto> candidates;

    scan_uninstall_hive(HKEY_CURRENT_USER, 0, core::InstallScope::User, candidates);
    scan_uninstall_hive(HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY, core::InstallScope::Machine, candidates);
    scan_uninstall_hive(HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY, core::InstallScope::Machine, candidates);

    if (candidates.empty()) {
        if (const auto local_app_data = env_path(L"LOCALAPPDATA")) {
            probe_executable_fallback(*local_app_data / L"Programs" / L"hitokoto" / L"hitokoto.exe",
                                      core::InstallScope::User, candidates);
        }
        if (const auto program_files = env_path(L"ProgramFiles")) {
            probe_executable_fallback(*program_files / L"hitokoto" / L"hitokoto.exe", core::InstallScope::Machine,
                                      candidates);
        }
    }

    return core::select_preferred_install(candidates);
}

}  // namespace updater::platform
