// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/json.hpp"

#include <cstdint>
#include <cstdlib>

namespace updater::core::json {

namespace {

constexpr int kMaxDepth = 64;

class Parser {
public:
    explicit Parser(std::string_view input) : input_(input) {}

    std::optional<JsonValue> parse() {
        skip_ws();
        auto val = parse_value(0);
        if (!val) {
            return std::nullopt;
        }
        skip_ws();
        if (pos_ != input_.size()) {
            return std::nullopt;
        }
        return val;
    }

private:
    void skip_ws() {
        while (pos_ < input_.size()) {
            const char ch = input_[pos_];
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool consume(char expected) {
        if (pos_ < input_.size() && input_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    std::optional<JsonValue> parse_value(int depth) {
        if (depth > kMaxDepth || pos_ >= input_.size()) {
            return std::nullopt;
        }
        switch (input_[pos_]) {
            case '{':
                return parse_object(depth + 1);
            case '[':
                return parse_array(depth + 1);
            case '"': {
                auto s = parse_string();
                if (!s) {
                    return std::nullopt;
                }
                return JsonValue{std::move(*s)};
            }
            case 't':
                return parse_literal("true", JsonValue{true});
            case 'f':
                return parse_literal("false", JsonValue{false});
            case 'n':
                return parse_literal("null", JsonValue{nullptr});
            default:
                return parse_number();
        }
    }

    std::optional<JsonValue> parse_literal(std::string_view lit, JsonValue val) {
        if (input_.substr(pos_, lit.size()) == lit) {
            pos_ += lit.size();
            return val;
        }
        return std::nullopt;
    }

    static bool is_hex(char ch, std::uint32_t& out) {
        if (ch >= '0' && ch <= '9') {
            out = static_cast<std::uint32_t>(ch - '0');
            return true;
        }
        if (ch >= 'a' && ch <= 'f') {
            out = static_cast<std::uint32_t>(10 + (ch - 'a'));
            return true;
        }
        if (ch >= 'A' && ch <= 'F') {
            out = static_cast<std::uint32_t>(10 + (ch - 'A'));
            return true;
        }
        return false;
    }

    std::optional<std::uint32_t> parse_hex4() {
        if (pos_ + 4 > input_.size()) {
            return std::nullopt;
        }
        std::uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            std::uint32_t nibble = 0;
            if (!is_hex(input_[pos_++], nibble)) {
                return std::nullopt;
            }
            code = (code << 4U) | nibble;
        }
        return code;
    }

    static void append_utf8(std::string& out, std::uint32_t cp) {
        if (cp <= 0x7FU) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FFU) {
            out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp <= 0xFFFFU) {
            out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        }
    }

    std::optional<std::string> parse_string() {
        if (!consume('"')) {
            return std::nullopt;
        }
        std::string out;
        while (pos_ < input_.size()) {
            const char ch = input_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (static_cast<unsigned char>(ch) < 0x20U) {
                return std::nullopt;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (pos_ >= input_.size()) {
                return std::nullopt;
            }
            const char esc = input_[pos_++];
            switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                case 'u': {
                    auto u1 = parse_hex4();
                    if (!u1) {
                        return std::nullopt;
                    }
                    std::uint32_t cp = *u1;
                    if (cp >= 0xD800U && cp <= 0xDBFFU) {
                        if (pos_ + 2 > input_.size() || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') {
                            return std::nullopt;
                        }
                        pos_ += 2;
                        auto u2 = parse_hex4();
                        if (!u2 || *u2 < 0xDC00U || *u2 > 0xDFFFU) {
                            return std::nullopt;
                        }
                        cp = 0x10000U + (((cp - 0xD800U) << 10U) | (*u2 - 0xDC00U));
                    } else if (cp >= 0xDC00U && cp <= 0xDFFFU) {
                        return std::nullopt;
                    }
                    append_utf8(out, cp);
                    break;
                }
                default:
                    return std::nullopt;
            }
        }
        return std::nullopt;
    }

    std::optional<JsonValue> parse_number() {
        const std::size_t start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= input_.size()) {
            return std::nullopt;
        }
        if (input_[pos_] == '0') {
            ++pos_;
        } else if (input_[pos_] >= '1' && input_[pos_] <= '9') {
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                ++pos_;
            }
        } else {
            return std::nullopt;
        }
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') {
                return std::nullopt;
            }
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                ++pos_;
            }
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) {
                ++pos_;
            }
            if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') {
                return std::nullopt;
            }
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') {
                ++pos_;
            }
        }
        const std::string num_str(input_.substr(start, pos_ - start));
        char* end_ptr = nullptr;
        const double value = std::strtod(num_str.c_str(), &end_ptr);
        if (end_ptr != num_str.c_str() + num_str.size()) {
            return std::nullopt;
        }
        return JsonValue{value};
    }

    std::optional<JsonValue> parse_array(int depth) {
        if (!consume('[')) {
            return std::nullopt;
        }
        JsonArray arr;
        skip_ws();
        if (consume(']')) {
            return JsonValue{std::move(arr)};
        }
        while (true) {
            skip_ws();
            auto elem = parse_value(depth);
            if (!elem) {
                return std::nullopt;
            }
            arr.push_back(std::move(*elem));
            skip_ws();
            if (consume(']')) {
                return JsonValue{std::move(arr)};
            }
            if (!consume(',')) {
                return std::nullopt;
            }
        }
    }

    std::optional<JsonValue> parse_object(int depth) {
        if (!consume('{')) {
            return std::nullopt;
        }
        JsonObject obj;
        skip_ws();
        if (consume('}')) {
            return JsonValue{std::move(obj)};
        }
        while (true) {
            skip_ws();
            auto key = parse_string();
            if (!key) {
                return std::nullopt;
            }
            skip_ws();
            if (!consume(':')) {
                return std::nullopt;
            }
            skip_ws();
            auto val = parse_value(depth);
            if (!val) {
                return std::nullopt;
            }
            obj.insert_or_assign(std::move(*key), std::move(*val));
            skip_ws();
            if (consume('}')) {
                return JsonValue{std::move(obj)};
            }
            if (!consume(',')) {
                return std::nullopt;
            }
        }
    }

    std::string_view input_;
    std::size_t pos_ = 0;
};

}  // namespace

const JsonObject* JsonValue::as_object() const {
    return std::get_if<JsonObject>(&data);
}
const JsonArray* JsonValue::as_array() const {
    return std::get_if<JsonArray>(&data);
}
const std::string* JsonValue::as_string() const {
    return std::get_if<std::string>(&data);
}

std::optional<bool> JsonValue::as_bool() const {
    if (const auto* b = std::get_if<bool>(&data)) {
        return *b;
    }
    return std::nullopt;
}

const JsonValue* JsonValue::find(std::string_view key) const {
    const auto* obj = as_object();
    if (obj == nullptr) {
        return nullptr;
    }
    const auto it = obj->find(std::string(key));
    if (it == obj->end()) {
        return nullptr;
    }
    return &it->second;
}

const std::string* JsonValue::find_string(std::string_view key) const {
    const auto* val = find(key);
    return val != nullptr ? val->as_string() : nullptr;
}

std::optional<bool> JsonValue::find_bool(std::string_view key) const {
    const auto* val = find(key);
    return val != nullptr ? val->as_bool() : std::nullopt;
}

const JsonArray* JsonValue::find_array(std::string_view key) const {
    const auto* val = find(key);
    return val != nullptr ? val->as_array() : nullptr;
}

std::optional<JsonValue> parse_json(std::string_view input) {
    Parser parser(input);
    return parser.parse();
}

}  // namespace updater::core::json
