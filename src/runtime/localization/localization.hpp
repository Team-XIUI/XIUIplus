#pragma once

#include "locale_catalog.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <string_view>

namespace xiui::i18n
{
    enum class language { english, japanese };
    inline constexpr const char* language_codes[]{"en", "ja"};
    inline constexpr const char* language_names[]{"English", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"};
    inline thread_local language current_language = language::english;

    inline auto ParseLanguage(const std::string_view code) -> language
    { return code == "ja" ? language::japanese : language::english; }

    class scope
    {
    public:
        explicit scope(language selected) : previous_(current_language) { current_language = selected; }
        ~scope() { current_language = this->previous_; }
        scope(const scope&) = delete;
        auto operator=(const scope&) -> scope& = delete;
    private:
        language previous_;
    };

    // Only pass XIUI+ messages here. Client resources and user content stay verbatim.
    inline auto Text(const char* message) -> const char*
    {
        if (message == nullptr || current_language == language::english) return message;
        const std::string_view key{message};
        const auto found = std::lower_bound(std::begin(catalog), std::end(catalog), key,
            [](const auto& entry, const auto value) { return std::string_view{entry.english} < value; });
        return found != std::end(catalog) && key == found->english ? found->japanese : message;
    }

    template<typename... Args>
    auto Format(const char* message, Args... args) -> std::string
    {
        const auto* format = Text(message);
        const int size = std::snprintf(nullptr, 0, format, args...);
        if (size < 0 || size > 16384) return {};
        std::string result(static_cast<size_t>(size), '\0');
        std::snprintf(result.data(), result.size() + 1, format, args...);
        return result;
    }
}
