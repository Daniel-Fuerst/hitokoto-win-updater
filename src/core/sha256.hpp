// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace updater::core {

class Sha256 {
public:
    Sha256();

    void update(std::span<const std::byte> data);
    void update(std::string_view text);

    /// Finalizes the hash and returns the 64-character lowercase hex digest.
    [[nodiscard]] std::string final_hex();

private:
    void transform(std::span<const std::byte, 64> block);

    std::array<std::uint32_t, 8> state_{};
    std::array<std::byte, 64> buffer_{};
    std::size_t buffer_len_ = 0;
    std::uint64_t total_bytes_ = 0;
};

[[nodiscard]] std::string sha256_hex(std::span<const std::byte> data);
[[nodiscard]] std::string sha256_hex(std::string_view text);

/// Computes the SHA-256 hex digest of a file on disk, or std::nullopt on I/O failure.
[[nodiscard]] std::optional<std::string> sha256_file_hex(const std::filesystem::path& file_path);

/// Parses a `SHA256SUMS` file and returns the 64-char lowercase hex digest for `filename`.
[[nodiscard]] std::optional<std::string> parse_sha256sums(std::string_view content, std::string_view filename);

}  // namespace updater::core
