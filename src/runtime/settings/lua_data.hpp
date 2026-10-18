#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace xiui::lua_data
{
    enum class scalar_kind { string, number, boolean };

    struct value
    {
        enum class kind { nil, scalar, table } type{};
        std::string text{};
        std::vector<std::pair<std::string, value>> fields{};
        scalar_kind scalar_type{scalar_kind::string};
        scalar_kind key_type{scalar_kind::string};

        auto Find(std::string_view key) const -> const value*;
        auto Index(size_t index) const -> const value*;
        auto Path(std::string_view path) const -> const value*;
        auto String(std::string fallback = {}) const -> std::string;
        auto Number(double fallback = 0) const -> double;
        auto Boolean(bool fallback = false) const -> bool;
        auto Set(std::string key, value item, scalar_kind keyType = scalar_kind::string) -> void;
        auto MergeDefaults(const value& defaults) -> void;
    };

    // Reads the data grammar emitted by Ashita/XIUI serializers; never executes Lua.
    class reader
    {
    public:
        explicit reader(std::string_view input) : input_(input) {}
        size_t duplicates{};
        auto Parse() -> value;

    private:
        std::string_view input_{};
        size_t at_{}, nodes_{};
        [[noreturn]] auto Fail(const char* message) const -> void;
        auto Skip() -> void;
        auto Consume(char token) -> bool;
        auto Word(std::string_view word) -> bool;
        auto Identifier() -> std::string;
        auto LongString() -> std::string;
        auto String() -> std::string;
        auto Value(size_t depth) -> value;
        auto Key() -> std::pair<std::string, scalar_kind>;
        auto Assignment(value& root) -> void;
    };

    auto UserSettings(const value& root) -> const value&;
    auto ToLiteral(const value& data) -> std::string;
}
