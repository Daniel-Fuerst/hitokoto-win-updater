// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/version.hpp"

#include <charconv>
#include <limits>
#include <system_error>

namespace updater::core {

namespace {

std::optional<int> parse_component(std::string_view part) {
    if (part.empty()) {
        return std::nullopt;
    }
    if (part.size() > 1 && part.front() == '0') {
        return std::nullopt;
    }
    for (char ch : part) {
        if (ch < '0' || ch > '9') {
            return std::nullopt;
        }
    }
    int value = 0;
    const char* begin = part.data();
    const char* end = part.data() + part.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value);
    if (ec != std::errc{} || ptr != end || value < 0) {
        return std::nullopt;
    }
    return value;
}

std::string_view trim_ascii_whitespace(std::string_view text) {
    while (!text.empty() &&
           (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' || text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

}  // namespace

std::optional<Version> parse_version(std::string_view text) {
    if (text.empty()) {
        return std::nullopt;
    }
    if (text.front() == 'v' || text.front() == 'V') {
        text.remove_prefix(1);
    }

    const auto dot1 = text.find('.');
    if (dot1 == std::string_view::npos) {
        return std::nullopt;
    }
    const auto dot2 = text.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos) {
        return std::nullopt;
    }
    if (text.find('.', dot2 + 1) != std::string_view::npos) {
        return std::nullopt;
    }

    const auto major = parse_component(text.substr(0, dot1));
    const auto minor = parse_component(text.substr(dot1 + 1, dot2 - dot1 - 1));
    const auto patch = parse_component(text.substr(dot2 + 1));
    if (!major || !minor || !patch) {
        return std::nullopt;
    }
    return Version{*major, *minor, *patch};
}

std::optional<Version> parse_hitokoto_cli_version(std::string_view stdout_text) {
    constexpr std::string_view kPrefix = "hitokoto ";
    const auto trimmed = trim_ascii_whitespace(stdout_text);
    if (!trimmed.starts_with(kPrefix)) {
        return std::nullopt;
    }
    return parse_version(trimmed.substr(kPrefix.size()));
}

std::string to_string(const Version& version) {
    return std::to_string(version.major) + '.' + std::to_string(version.minor) + '.' + std::to_string(version.patch);
}

}  // namespace updater::core
