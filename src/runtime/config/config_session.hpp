#pragma once

#include "modules/module.hpp"
#include "native_window_data.hpp"
#include "config_layout.hpp"
#include "color_picker.hpp"
#include "choice_dropdown.hpp"
#include "slider_value_input.hpp"
#include "text_input.hpp"
#include "settings_search.hpp"
#include "input/config_controller_focus.hpp"
#include "settings_sidebar.hpp"

#include <unordered_map>

namespace xiui
{
    struct config_session
    {
        hud_module* module{};
        bool basic{};
        bool edit_mode{}, preview_selected{};
        bool native_edit{};
        std::array<bool, native_window_count> native_edit_hidden{};
        ui::settings_sidebar_animation::transition native_edit_slide{};
        hud_module* advanced_module{};
        hud_module* layout_selection{};
        size_t advanced_section{}, advanced_scroll{};
        module_option_group advanced_group{module_option_group::general};
        ui::rect bounds{-1.0f, 20.0f, ui::config_layout::default_width, ui::config_layout::default_height};
        bool collapsed{};
        bool sidebar_collapsed{};
        config_controller_focus focus{};
        bool header_menu_open{}, language_menu{};
        size_t header_menu_selection{};
        D3DCOLOR* active_color{};
        ui::color_picker color_picker{};
        float* active_slider{};
        ui::slider_value_input slider_input{};
        hud_module* slider_module{};
        size_t slider_option{};
        ui::rect slider_bounds{};
        ui::text_input text_input{};
        ui::text_input search_input{};
        bool search_open{};
        float search_expansion{};
        uint64_t search_animation_at{};
        std::vector<ui::settings_search_entry> search_entries{};
        std::vector<size_t> search_matches{};
        size_t search_selection{}, search_scroll{};
        hud_module* search_revealed_module{};
        hud_module* search_highlight_module{};
        size_t search_highlight_option{};
        uint64_t search_highlight_until{};
        hud_module* text_module{};
        size_t text_option{};
        std::string* text_value{};
        ui::rect text_bounds{};
        ui::choice_dropdown choice_input{};
        hud_module* choice_module{};
        size_t choice_option{};
        float slider_minimum{};
        float slider_maximum{1.0f};
        float slider_step{0.1f};
        ui::rect slider_row{};
        size_t option{};
        module_option_group group{module_option_group::general};
        size_t section{};
        std::unordered_map<std::string, std::string> appearance_topics{};
        mutable ui::settings_sidebar_animation sidebar_animation{};
        bool sidebar_scrolling{};
        int scroll{};
        float scrollbar_grab{};
        std::string tooltip{};
        ui::rect tooltip_anchor{};

        auto ActivateToggle(const module_option& control, const size_t part = 0) const -> bool
        {
            if (!control.visible || !control.available) return false;
            if (control.kind == module_option_kind::toggle_buttons) return control.ToggleButton(part);
            if (control.kind != module_option_kind::toggle || control.toggle == nullptr) return false;
            *control.toggle = !*control.toggle;
            return true;
        }
    };
}
