// SPDX-License-Identifier: GPL-3.0-or-later
#include "platform/msi_installer.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <shellapi.h>

#include <string>
#include <vector>

namespace updater::platform {

namespace {

struct HandleGuard {
    HANDLE handle = nullptr;
    ~HandleGuard() {
        if (handle != nullptr) {
            CloseHandle(handle);
        }
    }
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;
    HandleGuard() = default;
};

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), needed);
    return out;
}

std::wstring quote_win_arg(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    for (auto it = arg.begin();; ++it) {
        std::size_t backslashes = 0;
        while (it != arg.end() && *it == L'\\') {
            ++it;
            ++backslashes;
        }
        if (it == arg.end()) {
            out.append(backslashes * 2U, L'\\');
            break;
        }
        if (*it == L'"') {
            out.append(backslashes * 2U + 1U, L'\\');
            out.push_back(L'"');
        } else {
            out.append(backslashes, L'\\');
            out.push_back(*it);
        }
    }
    out.push_back(L'"');
    return out;
}

std::wstring join_win_args(const std::vector<std::wstring>& args) {
    std::wstring joined;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (i > 0) {
            joined.push_back(L' ');
        }
        joined.append(quote_win_arg(args[i]));
    }
    return joined;
}

bool is_process_elevated() {
    HandleGuard token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.handle)) {
        return false;
    }
    TOKEN_ELEVATION elevation{};
    DWORD size = sizeof(elevation);
    if (!GetTokenInformation(token.handle, TokenElevation, &elevation, sizeof(elevation), &size)) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

std::wstring system_msiexec_path() {
    std::wstring sys_dir(MAX_PATH, L'\0');
    const UINT len = GetSystemDirectoryW(sys_dir.data(), static_cast<UINT>(sys_dir.size()));
    if (len == 0 || len >= sys_dir.size()) {
        return L"msiexec.exe";
    }
    sys_dir.resize(len);
    return sys_dir + L"\\msiexec.exe";
}

std::string format_win32_error(DWORD code) {
    return "Win32 error " + std::to_string(code);
}

}  // namespace

InstallResult WindowsMsiInstaller::install(const std::filesystem::path& msi_path,
                                           const core::MsiInstallOptions& options) const {
    const auto utf8_args = core::build_msiexec_arguments(msi_path.string(), options);
    std::vector<std::wstring> wide_args;
    wide_args.reserve(utf8_args.size());
    for (const auto& arg : utf8_args) {
        wide_args.push_back(utf8_to_wide(arg));
    }
    const std::wstring exe = system_msiexec_path();
    const std::wstring params = join_win_args(wide_args);

    const bool needs_elevation = (options.scope == core::InstallScope::Machine) && !is_process_elevated();
    if (needs_elevation) {
        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"runas";
        sei.lpFile = exe.c_str();
        sei.lpParameters = params.c_str();
        sei.nShow = SW_SHOWNORMAL;

        if (!ShellExecuteExW(&sei)) {
            const DWORD err = GetLastError();
            return {.launched = false,
                    .exit_code = -1,
                    .error = "Failed to elevate msiexec.exe (" + format_win32_error(err) + ")"};
        }
        HandleGuard proc;
        proc.handle = sei.hProcess;
        if (proc.handle == nullptr) {
            return {.launched = false, .exit_code = -1, .error = "Elevated msiexec.exe returned no process handle"};
        }
        WaitForSingleObject(proc.handle, INFINITE);
        DWORD exit_code = 1;
        if (!GetExitCodeProcess(proc.handle, &exit_code)) {
            return {.launched = true, .exit_code = -1, .error = "Failed to query msiexec.exe exit code"};
        }
        return {.launched = true, .exit_code = static_cast<int>(exit_code), .error = {}};
    }

    std::wstring cmdline = quote_win_arg(exe) + L" " + params;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmdline.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        const DWORD err = GetLastError();
        return {.launched = false,
                .exit_code = -1,
                .error = "Failed to start msiexec.exe (" + format_win32_error(err) + ")"};
    }
    HandleGuard proc;
    proc.handle = pi.hProcess;
    HandleGuard thread;
    thread.handle = pi.hThread;

    WaitForSingleObject(proc.handle, INFINITE);
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(proc.handle, &exit_code)) {
        return {.launched = true, .exit_code = -1, .error = "Failed to query msiexec.exe exit code"};
    }
    return {.launched = true, .exit_code = static_cast<int>(exit_code), .error = {}};
}

}  // namespace updater::platform
