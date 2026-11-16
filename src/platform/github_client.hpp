// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace updater::platform {

struct HttpTextResult {
    bool ok = false;
    int status_code = 0;
    std::string body;
    std::string error;
};

struct HttpDownloadResult {
    bool ok = false;
    int status_code = 0;
    std::string error;
};

class IGitHubClient {
public:
    virtual ~IGitHubClient() = default;

    [[nodiscard]] virtual HttpTextResult fetch_latest_release_json(const std::string& repo,
                                                                   const std::string& user_agent) = 0;
    [[nodiscard]] virtual HttpTextResult download_text(const std::string& url, const std::string& user_agent) = 0;
    [[nodiscard]] virtual HttpDownloadResult download_file(
        const std::string& url, const std::filesystem::path& destination, const std::string& user_agent,
        const std::function<void(std::uint64_t received, std::int64_t total)>& progress_cb = {}) = 0;
};

class QtGitHubClient final : public IGitHubClient {
public:
    [[nodiscard]] HttpTextResult fetch_latest_release_json(const std::string& repo,
                                                           const std::string& user_agent) override;
    [[nodiscard]] HttpTextResult download_text(const std::string& url, const std::string& user_agent) override;
    [[nodiscard]] HttpDownloadResult download_file(
        const std::string& url, const std::filesystem::path& destination, const std::string& user_agent,
        const std::function<void(std::uint64_t received, std::int64_t total)>& progress_cb = {}) override;
};

}  // namespace updater::platform
