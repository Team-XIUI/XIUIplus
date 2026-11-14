#include "localization/localization.hpp"
#include "config/config_layout.hpp"
#include "settings/settings_store.hpp"
#include "ui/text_wrap.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
    auto Require(bool value, const char* message) -> void
    { if (!value) throw std::runtime_error(message); }
}

auto main() -> int
{
    try
    {
        using namespace xiui;
        Require(i18n::ParseLanguage("ja") == i18n::language::japanese &&
            i18n::ParseLanguage("unsupported") == i18n::language::english, "language fallback");
        Require(std::strcmp(i18n::Text("Target"), "Target") == 0, "English default");
        for (const auto language : {i18n::language::english, i18n::language::japanese})
        {
            const i18n::scope locale{language};
            for (const auto* label : {"HP", "MP", "TP"})
                Require(std::strcmp(i18n::Text(label), label) == 0, "resource abbreviations retain their meaning in both locales");
        }
        const char* translated{};
        {
            const i18n::scope japanese{i18n::language::japanese};
            translated = i18n::Text("Target");
            Require(std::strcmp(translated, "Target") != 0, "Japanese lookup");
            for (const auto* message : {"Extra Settings", "Fullness", "EXP Bar", "Actions & Macros", "Search actions and macros...", "Edit Mode  OFF", "Crossbar action sets"})
                Require(std::strcmp(i18n::Text(message), message) != 0, "reported UI surfaces have Japanese translations");
            for (size_t index = 1; index <= 10; ++index)
            {
                const auto key = "Hotbar " + std::to_string(index);
                const auto expected = std::string{i18n::Text("Hotbar")} + std::to_string(index);
                Require(i18n::Text(key.c_str()) == expected && i18n::Format("Hotbar %zu", index) == expected,
                    "numbered Hotbar labels agree across static and dynamic UI paths");
            }
            const char* unknown = "A new untranslated message";
            Require(i18n::Text(unknown) == unknown && i18n::Text(nullptr) == nullptr, "missing message fallback");
            Require(i18n::Format("Apply %s", "Target").find("Target") != std::string::npos, "format leaves user arguments verbatim");
            const auto lines = ui::WrapText(translated, 2, [](const char* text) {
                size_t glyphs{};
                for (; *text; ++text) if ((static_cast<unsigned char>(*text) & 0xC0) != 0x80) ++glyphs;
                return static_cast<float>(glyphs);
            });
            Require(lines.size() > 1, "Japanese wraps without spaces");
            std::string joined;
            for (const auto& line : lines) joined += line;
            Require(joined == translated, "UTF-8 boundaries preserved");
            { const i18n::scope english{i18n::language::english}; Require(std::strcmp(i18n::Text("Target"), "Target") == 0, "nested locale"); }
            Require(i18n::Text("Target") == translated, "locale scope restored");
        }
        Require(std::strcmp(i18n::Text("Target"), "Target") == 0 && std::strlen(translated) > 0, "locale and catalog lifetime");
        const auto words = ui::WrapText("one two three", 7, [](const char* text) { return static_cast<float>(std::strlen(text)); });
        Require(words.size() == 2 && words[0] == "one two" && words[1] == "three", "English word wrapping");
        auto settings = settings_store::FromIniText("");
        settings.WriteString("session.localization", "language", "ja");
        settings.WriteBool("player", "enabled", true);
        bool player{}, leaked{};
        settings.VisitPreferences([&](auto section, auto, auto) { player |= section == "player"; leaked |= section == "session.localization"; });
        Require(player && !leaked, "UI language excluded from portable profiles");
        const ui::rect window{20, 20, 760, 400};
        for (const auto layout : {ui::config_layout{window}, ui::config_layout::Basic(window)})
        {
            const auto drag = layout.HeaderDragArea();
            Require(drag.x + drag.width <= layout.Language().x, "drag area excludes language icon");
            Require(layout.SearchInput().x + layout.SearchInput().width <= layout.Language().x, "search excludes language icon");
            Require(layout.HeaderMenuRow(1, true).y + layout.HeaderMenuRow(1, true).height <=
                layout.HeaderMenu(true).y + layout.HeaderMenu(true).height, "language rows fit popup");
        }
        std::cout << "Localization: locale fallback, UTF-8 wrapping, local persistence and header geometry passed.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
