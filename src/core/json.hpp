// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace updater::core::json {

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::unordered_map<std::string, JsonValue>;

struct JsonValue {
    std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject> data{nullptr};

    [[nodiscard]] const JsonObject* as_object() const;
    [[nodiscard]] const JsonArray* as_array() const;
    [[nodiscard]] const std::string* as_string() const;
    [[nodiscard]] std::optional<bool> as_bool() const;

    [[nodiscard]] const JsonValue* find(std::string_view key) const;
    [[nodiscard]] const std::string* find_string(std::string_view key) const;
    [[nodiscard]] std::optional<bool> find_bool(std::string_view key) const;
    [[nodiscard]] const JsonArray* find_array(std::string_view key) const;
};

/// Parses a UTF-8 JSON document into a JsonValue, or returns std::nullopt on malformed input.
[[nodiscard]] std::optional<JsonValue> parse_json(std::string_view input);

}  // namespace updater::core::json
