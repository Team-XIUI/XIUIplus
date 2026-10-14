#pragma once

#include "Ashita.h"

#include <algorithm>
#include <array>

namespace xiui::ui
{
    struct font_face { const char* family; int weight; };
    inline constexpr const char* font_names[]{"XIUI (Bahnschrift)", "Segoe UI", "Tahoma", "Grammara"};
    inline constexpr std::array font_faces{font_face{"Bahnschrift", FW_BOLD}, font_face{"Segoe UI", FW_SEMIBOLD},
        font_face{"Tahoma", FW_BOLD}, font_face{"Grammara", FW_NORMAL}};
    inline constexpr int font_count = static_cast<int>(font_faces.size());
    inline constexpr const char* global_text_section = "global.text";
    inline auto FontIndex(const int index) -> int { return std::clamp(index, 0, font_count - 1); }

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
