#include "lua_data.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace xiui::lua_data
{
    namespace
    {
        constexpr size_t maximum_file = 16 * 1024 * 1024, maximum_string = 1024 * 1024;
        auto IdentifierPart(const unsigned char c) -> bool { return std::isalnum(c) != 0 || c == '_'; }
        auto Quote(const std::string_view text) -> std::string
        {
            std::string result{"\""};
            for (const unsigned char c : text)
            {
                if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
                else if (c < 32 || c >= 127)
                { result += '\\'; result += static_cast<char>('0' + c / 100); result += static_cast<char>('0' + c / 10 % 10); result += static_cast<char>('0' + c % 10); }
                else result += static_cast<char>(c);
            }
            return result + '"';
        }
    }

    auto value::Find(const std::string_view key) const -> const value*
    {
        for (auto it = this->fields.rbegin(); it != this->fields.rend(); ++it)
            if (it->first == key && it->second.key_type == scalar_kind::string) return &it->second;
        return nullptr;
    }
    auto value::Index(const size_t index) const -> const value*
    {
        const auto key = std::to_string(index);
        for (auto it = this->fields.rbegin(); it != this->fields.rend(); ++it)
            if (it->first == key && it->second.key_type == scalar_kind::number) return &it->second;
        return this->Find(key);
    }
    auto value::Path(const std::string_view path) const -> const value*
    {
        const value* current = this;
        for (size_t start = 0; start < path.size();)
        {
            const auto end = path.find('.', start);
            const auto part = path.substr(start, end == path.npos ? path.size() - start : end - start);
            const auto* next = current->Find(part);
            if (next == nullptr && !part.empty() && std::all_of(part.begin(), part.end(), [](const char c) { return c >= '0' && c <= '9'; }))
            {
                size_t index{}; const auto parsed = std::from_chars(part.data(), part.data() + part.size(), index);
                if (parsed.ec == std::errc{}) next = current->Index(index);
            }
            current = next;
            if (current == nullptr) return nullptr;
            if (end == path.npos) break;
            start = end + 1;
        }
        return current;
    }
    auto value::String(std::string fallback) const -> std::string { return this->type == kind::scalar ? this->text : std::move(fallback); }
    auto value::Number(const double fallback) const -> double
    {
        if (this->type != kind::scalar || this->scalar_type == scalar_kind::boolean) return fallback;
        char* end{}; const auto number = std::strtod(this->text.c_str(), &end);
        return end != this->text.c_str() && *end == '\0' && std::isfinite(number) ? number : fallback;
    }
    auto value::Boolean(const bool fallback) const -> bool
    { return this->type == kind::scalar && this->scalar_type == scalar_kind::boolean ? this->text == "true" : fallback; }
    auto value::Set(std::string key, value item, const scalar_kind keyType) -> void
    {
        item.key_type = keyType;
        for (auto& [oldKey, old] : this->fields)
            if (oldKey == key && old.key_type == keyType) { old = std::move(item); return; }
        this->fields.emplace_back(std::move(key), std::move(item));
    }
    auto value::MergeDefaults(const value& defaults) -> void
    {
        if (this->type != kind::table || defaults.type != kind::table) return;
        for (const auto& [key, fallback] : defaults.fields)
        {
            auto found = std::find_if(this->fields.begin(), this->fields.end(), [&](const auto& item) { return item.first == key && item.second.key_type == fallback.key_type; });
            if (found == this->fields.end()) this->fields.emplace_back(key, fallback);
            else if (found->second.type == kind::nil) found->second = fallback;
            else found->second.MergeDefaults(fallback);
        }
    }
    [[noreturn]] auto reader::Fail(const char* message) const -> void
    { throw std::runtime_error{std::string{message} + " (byte " + std::to_string(this->at_ + 1) + ")."}; }
    auto reader::LongString() -> std::string
    {
        const auto start = this->at_++;
        while (this->at_ < this->input_.size() && this->input_[this->at_] == '=') ++this->at_;
        if (this->at_ == this->input_.size() || this->input_[this->at_++] != '[') this->Fail("Invalid long string delimiter");
        const auto closing = "]" + std::string(this->at_ - start - 2, '=') + "]";
        if (this->at_ < this->input_.size() && this->input_[this->at_] == '\r') ++this->at_;
        if (this->at_ < this->input_.size() && this->input_[this->at_] == '\n') ++this->at_;
        const auto end = this->input_.find(closing, this->at_);
        if (end == this->input_.npos || end - this->at_ > maximum_string) this->Fail("Unterminated or oversized long string");
        auto result = std::string{this->input_.substr(this->at_, end - this->at_)};
        this->at_ = end + closing.size(); return result;
    }
    auto reader::Skip() -> void
    {
        while (this->at_ < this->input_.size())
        {
            if (std::isspace(static_cast<unsigned char>(this->input_[this->at_]))) { ++this->at_; continue; }
            if (this->input_.substr(this->at_, 2) != "--") break;
            this->at_ += 2;
            if (this->at_ < this->input_.size() && this->input_[this->at_] == '[' &&
                this->at_ + 1 < this->input_.size() && (this->input_[this->at_ + 1] == '[' || this->input_[this->at_ + 1] == '=')) this->LongString();
            else { const auto end = this->input_.find('\n', this->at_); this->at_ = end == this->input_.npos ? this->input_.size() : end + 1; }
        }
    }
    auto reader::Consume(const char token) -> bool
    { this->Skip(); if (this->at_ < this->input_.size() && this->input_[this->at_] == token) { ++this->at_; return true; } return false; }
    auto reader::Word(const std::string_view word) -> bool
    {
        this->Skip();
        if (this->input_.substr(this->at_, word.size()) != word ||
            (this->at_ + word.size() < this->input_.size() && IdentifierPart(this->input_[this->at_ + word.size()]))) return false;
        this->at_ += word.size(); return true;
    }
    auto reader::Identifier() -> std::string
    {
        this->Skip(); const auto start = this->at_;
        if (this->at_ < this->input_.size() && (std::isalpha(static_cast<unsigned char>(this->input_[this->at_])) || this->input_[this->at_] == '_'))
            while (this->at_ < this->input_.size() && IdentifierPart(this->input_[this->at_])) ++this->at_;
        return std::string{this->input_.substr(start, this->at_ - start)};
    }
    auto reader::String() -> std::string
    {
        if (this->input_[this->at_] == '[') return this->LongString();
        const char quote = this->input_[this->at_++]; std::string result;
        while (this->at_ < this->input_.size())
        {
            char c = this->input_[this->at_++];
            if (c == quote) return result;
            if (c == '\\')
            {
                if (this->at_ == this->input_.size()) break;
                c = this->input_[this->at_++];
                if (c >= '0' && c <= '9')
                {
                    unsigned byte = c - '0'; size_t count = 1;
                    while (count < 3 && this->at_ < this->input_.size() && this->input_[this->at_] >= '0' && this->input_[this->at_] <= '9')
                    { byte = byte * 10 + this->input_[this->at_++] - '0'; ++count; }
                    if (byte > 255) this->Fail("String byte escape exceeds 255");
                    c = static_cast<char>(byte);
                }
                else if (c == 'n') c = '\n'; else if (c == 'r') c = '\r'; else if (c == 't') c = '\t';
                else if (c == 'a') c = '\a'; else if (c == 'b') c = '\b'; else if (c == 'f') c = '\f'; else if (c == 'v') c = '\v';
                else if (c == '\r') { if (this->at_ < this->input_.size() && this->input_[this->at_] == '\n') ++this->at_; c = '\n'; }
                else if (c != '\n' && c != '\\' && c != '\'' && c != '"') this->Fail("Unsupported string escape");
            }
            else if (c == '\r' || c == '\n') this->Fail("Unescaped line break in string");
            result += c;
            if (result.size() > maximum_string) this->Fail("String exceeds the import limit");
        }
        this->Fail("Unterminated string");
    }
    auto reader::Key() -> std::pair<std::string, scalar_kind>
    {
        auto key = this->Value(1);
        if (key.type != value::kind::scalar) this->Fail("Table keys must be literal strings, numbers or booleans");
        if (key.scalar_type == scalar_kind::number)
        {
            const auto number = key.Number(NAN);
            if (!std::isfinite(number) || std::abs(number) > 9007199254740991.0) this->Fail("Invalid table number key");
            if (std::floor(number) == number) key.text = std::to_string(static_cast<long long>(number));
        }
        return {std::move(key.text), key.scalar_type};
    }
    auto reader::Value(const size_t depth) -> value
    {
        this->Skip(); if (depth > 48 || ++this->nodes_ > 500000) this->Fail("Literal nesting or record limit exceeded");
        if (this->at_ == this->input_.size()) this->Fail("Unexpected end of data");
        if (this->Word("T")) this->Skip();
        if (this->Consume('{'))
        {
            value table{value::kind::table}; size_t index = 1;
            while (!this->Consume('}'))
            {
                this->Skip(); const auto start = this->at_;
                std::string key; scalar_kind keyType{scalar_kind::string};
                if (this->at_ < this->input_.size() && this->input_[this->at_] == '[' &&
                    this->input_.substr(this->at_, 2) != "[[" && this->input_.substr(this->at_, 2) != "[=")
                { ++this->at_; auto parsed = this->Key(); key = std::move(parsed.first); keyType = parsed.second; if (!this->Consume(']') || !this->Consume('=')) this->Fail("Expected ] = after key"); }
                else
                {
                    key = this->Identifier();
                    if (key.empty() || !this->Consume('=')) { this->at_ = start; key = std::to_string(index++); keyType = scalar_kind::number; }
                }
                auto item = this->Value(depth + 1);
                if (std::any_of(table.fields.begin(), table.fields.end(), [&](const auto& old) { return old.first == key && old.second.key_type == keyType; })) ++this->duplicates;
                table.Set(std::move(key), std::move(item), keyType);
                if (this->Consume(',') || this->Consume(';')) continue;
                if (!this->Consume('}')) this->Fail("Expected comma or table end");
                return table;
            }
            return table;
        }
        if (this->at_ == this->input_.size()) this->Fail("Expected a value");
        const char first = this->input_[this->at_];
        if (first == '\'' || first == '"' || first == '[') return {value::kind::scalar, this->String()};
        if (this->Word("true")) return {value::kind::scalar, "true", {}, scalar_kind::boolean};
        if (this->Word("false")) return {value::kind::scalar, "false", {}, scalar_kind::boolean};
        if (this->Word("nil")) return {};
        const auto start = this->at_;
        if (first == '-' || first == '+') ++this->at_;
        const bool hex = this->input_.substr(this->at_, 2) == "0x" || this->input_.substr(this->at_, 2) == "0X";
        if (hex) this->at_ += 2;
        while (this->at_ < this->input_.size())
        {
            const auto c = static_cast<unsigned char>(this->input_[this->at_]);
            if (hex ? std::isxdigit(c) != 0 : std::isdigit(c) != 0 || c == '.') ++this->at_;
            else break;
        }
        if (!hex && this->at_ < this->input_.size() && (this->input_[this->at_] == 'e' || this->input_[this->at_] == 'E'))
        {
            ++this->at_; if (this->at_ < this->input_.size() && (this->input_[this->at_] == '+' || this->input_[this->at_] == '-')) ++this->at_;
            while (this->at_ < this->input_.size() && std::isdigit(static_cast<unsigned char>(this->input_[this->at_]))) ++this->at_;
        }
        value result{value::kind::scalar, std::string{this->input_.substr(start, this->at_ - start)}, {}, scalar_kind::number};
        if (!std::isfinite(result.Number(NAN))) this->Fail("Expected finite literal data; executable Lua is not imported");
        return result;
    }
    auto reader::Assignment(value& root) -> void
    {
        if (!this->Word("settings")) this->Fail("Only settings assignments are supported");
        std::vector<std::pair<std::string, scalar_kind>> keys;
        while (this->Consume('['))
        {
            keys.push_back(this->Key());
            if (keys.size() > 48 || !this->Consume(']')) this->Fail("Invalid settings path");
        }
        if (keys.empty() || !this->Consume('=')) this->Fail("Expected settings path and assignment");
        auto item = this->Value(keys.size());
        auto* table = &root;
        for (size_t i = 0; i + 1 < keys.size(); ++i)
        {
            auto found = std::find_if(table->fields.begin(), table->fields.end(), [&](const auto& field) { return field.first == keys[i].first && field.second.key_type == keys[i].second; });
            if (found == table->fields.end() || found->second.type != value::kind::table) this->Fail("Settings parent must be an initialized table");
            table = &found->second;
        }
        table->Set(std::move(keys.back().first), std::move(item), keys.back().second);
        this->Consume(';');
    }
    auto reader::Parse() -> value
    {
        if (this->input_.size() > maximum_file) this->Fail("The data file exceeds 16 MB");
        if (this->input_.starts_with("\xEF\xBB\xBF")) this->at_ = 3;
        this->Skip(); value result;
        if (this->Word("require"))
        {
            if (!this->Consume('(') || this->String() != "common" || !this->Consume(')')) this->Fail("Only the common serializer header is accepted");
            this->Consume(';');
        }
        if (this->Word("local"))
        {
            if (!this->Word("settings") || !this->Consume('=')) this->Fail("Expected local settings declaration");
            result = this->Value(0);
            if (result.type != value::kind::table) this->Fail("Settings must be a literal table");
            this->Consume(';');
            while (!this->Word("return")) this->Assignment(result);
            if (!this->Word("settings")) this->Fail("Expected return settings");
        }
        else { this->Word("return"); result = this->Value(0); }
        this->Consume(';'); this->Skip();
        if (this->at_ != this->input_.size()) this->Fail("Trailing executable Lua is not imported");
        if (result.type != value::kind::table) this->Fail("Expected a literal table");
        return result;
    }
    auto UserSettings(const value& root) -> const value&
    { const auto* settings = root.Find("userSettings"); return settings != nullptr && settings->type == value::kind::table ? *settings : root; }
    auto ToLiteral(const value& data) -> std::string
    {
        if (data.type == value::kind::nil) return "nil";
        if (data.type == value::kind::scalar) return data.scalar_type == scalar_kind::string ? Quote(data.text) : data.text;
        std::string result{"{"};
        for (const auto& [key, item] : data.fields)
            result += "[" + (item.key_type == scalar_kind::string ? Quote(key) : key) + "]=" + ToLiteral(item) + ",";
        return result + '}';
    }
}
