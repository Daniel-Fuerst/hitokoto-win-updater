// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/sha256.hpp"

#include <bit>
#include <cstring>
#include <fstream>

namespace updater::core {

namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

std::uint32_t load_be32(std::span<const std::byte, 4> bytes) {
    return (static_cast<std::uint32_t>(bytes[0]) << 24U) | (static_cast<std::uint32_t>(bytes[1]) << 16U) |
           (static_cast<std::uint32_t>(bytes[2]) << 8U) | static_cast<std::uint32_t>(bytes[3]);
}

bool is_hex_digit(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

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

}  // namespace

Sha256::Sha256() {
    state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
}

void Sha256::update(std::span<const std::byte> data) {
    total_bytes_ += data.size();
    std::size_t offset = 0;

    if (buffer_len_ > 0) {
        const std::size_t to_copy = std::min(64 - buffer_len_, data.size());
        std::memcpy(buffer_.data() + buffer_len_, data.data(), to_copy);
        buffer_len_ += to_copy;
        offset += to_copy;
        if (buffer_len_ == 64) {
            transform(std::span<const std::byte, 64>{buffer_.data(), 64});
            buffer_len_ = 0;
        }
    }

    while (offset + 64 <= data.size()) {
        transform(std::span<const std::byte, 64>{data.data() + offset, 64});
        offset += 64;
    }

    if (offset < data.size()) {
        const std::size_t rem = data.size() - offset;
        std::memcpy(buffer_.data(), data.data() + offset, rem);
        buffer_len_ = rem;
    }
}

void Sha256::update(std::string_view text) {
    update(std::as_bytes(std::span<const char>{text.data(), text.size()}));
}

std::string Sha256::final_hex() {
    const std::uint64_t total_bits = total_bytes_ * 8U;
    buffer_[buffer_len_++] = std::byte{0x80};
    if (buffer_len_ > 56) {
        std::memset(buffer_.data() + buffer_len_, 0, 64 - buffer_len_);
        transform(std::span<const std::byte, 64>{buffer_.data(), 64});
        buffer_len_ = 0;
    }
    std::memset(buffer_.data() + buffer_len_, 0, 56 - buffer_len_);
    for (std::size_t i = 0; i < 8; ++i) {
        const auto shift = static_cast<unsigned>((7U - i) * 8U);
        buffer_[56U + i] = static_cast<std::byte>((total_bits >> shift) & 0xFFU);
    }
    transform(std::span<const std::byte, 64>{buffer_.data(), 64});

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (std::uint32_t word : state_) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            out.push_back(kHex[(word >> static_cast<unsigned>(shift)) & 0xFU]);
        }
    }
    return out;
}

void Sha256::transform(std::span<const std::byte, 64> block) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = load_be32(std::span<const std::byte, 4>{block.data() + i * 4, 4});
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = std::rotr(w[i - 15], 7) ^ std::rotr(w[i - 15], 18) ^ (w[i - 15] >> 3U);
        const std::uint32_t s1 = std::rotr(w[i - 2], 17) ^ std::rotr(w[i - 2], 19) ^ (w[i - 2] >> 10U);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    auto [a, b, c, d, e, f, g, h] = state_;
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t s1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 = h + s1 + ch + kRoundConstants[i] + w[i];
        const std::uint32_t s0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::string sha256_hex(std::span<const std::byte> data) {
    Sha256 hasher;
    hasher.update(data);
    return hasher.final_hex();
}

std::string sha256_hex(std::string_view text) {
    Sha256 hasher;
    hasher.update(text);
    return hasher.final_hex();
}

std::optional<std::string> sha256_file_hex(const std::filesystem::path& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in.is_open()) {
        return std::nullopt;
    }
    Sha256 hasher;
    std::array<char, 65536> buf{};
    while (in.good()) {
        in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const auto count = in.gcount();
        if (count > 0) {
            hasher.update(std::as_bytes(std::span<const char>{buf.data(), static_cast<std::size_t>(count)}));
        }
    }
    if (in.bad()) {
        return std::nullopt;
    }
    return hasher.final_hex();
}

std::optional<std::string> parse_sha256sums(std::string_view content, std::string_view filename) {
    std::size_t pos = 0;
    while (pos < content.size()) {
        const auto nl = content.find('\n', pos);
        auto line = (nl == std::string_view::npos) ? content.substr(pos) : content.substr(pos, nl - pos);
        pos = (nl == std::string_view::npos) ? content.size() : nl + 1;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.size() < 66) {
            continue;
        }
        const auto hex = line.substr(0, 64);
        bool hex_ok = true;
        for (char ch : hex) {
            if (!is_hex_digit(ch)) {
                hex_ok = false;
                break;
            }
        }
        if (!hex_ok || line[64] != ' ') {
            continue;
        }
        auto rest = line.substr(65);
        if (!rest.empty() && (rest.front() == ' ' || rest.front() == '*')) {
            rest.remove_prefix(1);
        }
        if (rest == filename) {
            return to_lower_ascii(hex);
        }
    }
    return std::nullopt;
}

}  // namespace updater::core
