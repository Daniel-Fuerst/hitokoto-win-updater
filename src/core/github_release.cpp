// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/github_release.hpp"

#include "core/json.hpp"

namespace updater::core {

namespace {

std::string to_lower_ascii(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        if (ch >= 'A' && ch <= 'Z') {
            out.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

bool is_64_hex(std::string_view s) {
    if (s.size() != 64) {
        return false;
    }
    for (char ch : s) {
        const bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
        if (!ok) {
            return false;
        }
    }
    return true;
}

std::optional<std::string> parse_sha256_digest_field(std::string_view digest) {
    constexpr std::string_view kPrefix = "sha256:";
    if (!digest.starts_with(kPrefix)) {
        return std::nullopt;
    }
    const auto hex = digest.substr(kPrefix.size());
    if (!is_64_hex(hex)) {
        return std::nullopt;
    }
    return to_lower_ascii(hex);
}

}  // namespace

ReleaseParseResult parse_github_release_json(std::string_view json_text) {
    const auto root = json::parse_json(json_text);
    if (!root || root->as_object() == nullptr) {
        return {.release = std::nullopt, .error = "Invalid GitHub release JSON payload"};
    }

    const auto* tag = root->find_string("tag_name");
    if (tag == nullptr || tag->empty()) {
        return {.release = std::nullopt, .error = "GitHub release payload is missing tag_name"};
    }

    const auto version = parse_version(*tag);
    if (!version) {
        return {.release = std::nullopt, .error = "Release tag '" + *tag + "' is not a MAJOR.MINOR.PATCH version"};
    }

    GitHubRelease release;
    release.version = *version;
    release.tag_name = *tag;
    if (const auto* name = root->find_string("name")) {
        release.name = *name;
    }
    release.prerelease = root->find_bool("prerelease").value_or(false);
    release.draft = root->find_bool("draft").value_or(false);

    if (const auto* assets = root->find_array("assets")) {
        for (const auto& item : *assets) {
            const auto* asset_name = item.find_string("name");
            const auto* download_url = item.find_string("browser_download_url");
            if (asset_name == nullptr || download_url == nullptr) {
                continue;
            }
            ReleaseAsset asset;
            asset.name = *asset_name;
            asset.download_url = *download_url;
            if (const auto* digest = item.find_string("digest")) {
                asset.sha256_digest = parse_sha256_digest_field(*digest);
            }
            release.assets.push_back(std::move(asset));
        }
    }

    return {.release = std::move(release), .error = {}};
}

std::optional<ReleaseAsset> find_msi_asset(const GitHubRelease& release) {
    const std::string expected = "hitokoto-" + to_string(release.version) + "-x64.msi";
    for (const auto& asset : release.assets) {
        if (asset.name == expected) {
            return asset;
        }
    }
    return std::nullopt;
}

std::optional<ReleaseAsset> find_sha256sums_asset(const GitHubRelease& release) {
    for (const auto& asset : release.assets) {
        if (asset.name == "SHA256SUMS") {
            return asset;
        }
    }
    return std::nullopt;
}

}  // namespace updater::core
