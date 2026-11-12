#pragma once

#include "Ashita.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace xiui::ui
{
    enum class font_weight : uint8_t { face_default, regular, bold, medium, count };
    inline constexpr int font_weight_count = static_cast<int>(font_weight::count);
    // Weight and slant for one draw. A bare weight converts implicitly for internal overrides.
    struct font_variant
    {
        font_weight weight{};
        bool italic{};
        constexpr font_variant(const font_weight value = font_weight::face_default, const bool slanted = false) : weight(value), italic(slanted) {}
        constexpr auto Key(void) const -> int { return static_cast<int>(this->weight) * 2 + (this->italic ? 1 : 0); }
    };
    inline constexpr int font_variant_count = font_weight_count * 2;
    // Saved text weight choices; Default keeps each face's tuned weight.
    inline constexpr int text_weight_default = 0, text_weight_regular = 1, text_weight_bold = 2;
    inline constexpr const char* text_weight_names[]{"Default", "Regular", "Bold"};
    struct font_face { const char* family; int weight; bool italic{}; };
    inline constexpr int xiv1_font = 4, xiv1_italic_font = 5, xiv1_number_font = 6;
    // XIUI+ faces keep their saved indices. Installed fonts follow them and are saved by family name.
    inline constexpr const char* builtin_font_names[]{"XIUI+ (Bahnschrift)", "Segoe UI", "Tahoma", "Grammara", "XIV 1.0 (Georgia)", "XIV 1.0 (Georgia Italic)", "XIV 1.0 (Lining Numerals)"};
    inline constexpr std::array builtin_font_faces{font_face{"Bahnschrift", FW_BOLD}, font_face{"Segoe UI", FW_SEMIBOLD},
        font_face{"Tahoma", FW_BOLD}, font_face{"Grammara", FW_NORMAL}, font_face{"Georgia", FW_BOLD}, font_face{"Georgia", FW_BOLD, true},
        font_face{"Times New Roman", FW_BOLD, true}};
    inline constexpr int builtin_font_count = static_cast<int>(builtin_font_faces.size());
    inline constexpr int grammara_font = 3;
    inline constexpr int max_font_count = 1024;
    // Rasterizer-only Yu Gothic UI face for glyphs the selected font lacks.
    inline constexpr int cjk_fallback_font = max_font_count;
    inline constexpr const char* global_text_section = "global.text";

    struct font_entry
    {
        std::string label{}, family{};
        std::wstring wide_family{};
        int weight{FW_BOLD};
        bool italic{}, builtin{};
    };
    // Built-in faces, then installed families sorted by name, then saved names that are not installed.
    auto FontCount(void) -> int;
    auto FontNames(void) -> const char* const*;
    auto FontEntry(int font) -> const font_entry&;
    auto FontIndex(int index) -> int;
    // Finds or registers a non-built-in family so a saved choice survives even when it is not installed here.
    auto FontFamilyIndex(std::string_view family) -> int;
    // Empty for built-in faces, which are saved by index.
    auto FontFamily(int font) -> std::string;

    class embedded_font
    {
    public:
        embedded_font(void) = default;
        embedded_font(const embedded_font&) = delete;
        auto operator=(const embedded_font&) -> embedded_font& = delete;
        ~embedded_font(void);
        auto Load(void) -> bool;
        auto Clear(void) -> void;
        auto Available(void) const -> bool { return this->handle_ != nullptr; }

    private:
        HANDLE handle_{};
    };
}
