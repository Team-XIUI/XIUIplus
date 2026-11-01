#include "ui/text_fit.hpp"
#include "actionbars_module.hpp"
#include "combat/server_rules.hpp"

#include "config/config_controls.hpp"
#include "config/config_scrollbar.hpp"
#include "config/config_window_resize.hpp"
#include "config/clipboard.hpp"
#include "ui/recast_icons.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>

namespace xiui
{
    namespace
    {
        constexpr const char* bar_names[]{"Hotbar 1", "Hotbar 2", "Hotbar 3", "Hotbar 4", "Hotbar 5", "Hotbar 6", "Hotbar 7", "Hotbar 8", "Hotbar 9", "Hotbar 10", "Crossbar"};
        constexpr const char* config_views[]{"Hotbar 1", "Hotbar 2", "Hotbar 3", "Hotbar 4", "Hotbar 5", "Hotbar 6", "Hotbar 7", "Hotbar 8", "Hotbar 9", "Hotbar 10", "Crossbar", "Actions & Macros", "Action Sets", "Import", "Global"};
        constexpr const char* bar_ids[]{"hotbar", "hotbar2", "hotbar3", "hotbar4", "hotbar5", "hotbar6", "hotbar7", "hotbar8", "hotbar9", "hotbar10", "crossbar", "global"};
        constexpr const char* bank_names[]{"Left trigger", "Right trigger", "Left then right", "Right then left", "Double left", "Double right", "Shared expanded"};
        constexpr std::array<size_t, 11> crossbar_sections{0, 6, 9, 1, 2, 3, 4, 5, 7, 8, 10};
        constexpr std::array<size_t, 10> hotbar_sections{0, 9, 1, 2, 3, 4, 5, 7, 8, 10};
        constexpr const char* profile_sections[]{"text.actionbars.title", "text.actionbars.label", "text.actionbars.binding", "text.actionbars.timer", "text.actionbars.tooltip", "text.actionbars.count", "text.actionbars.cost"};
        constexpr const char* workspace_sections[]{"layout.action_library", "layout.action_sets", "layout.action_editor", "layout.action_controller", "layout.action_keybinds"};
        constexpr int window_move = 700, window_resize = 701, window_close = 702;
        constexpr const char* categories[]{"All actions", "Spells", "Abilities", "Weapon skills", "Items", "Equipment", "Pet commands", "Macros", "Commands"};
        constexpr action_kind category_kinds[]{action_kind::none, action_kind::spell, action_kind::ability, action_kind::weapon_skill, action_kind::item, action_kind::equip, action_kind::pet, action_kind::macro, action_kind::command};
        constexpr const char* targets[]{"<t>", "<me>", "<stpc>", "<stnpc>", "<st>", "<bt>", "<lastst>", "<stal>", "<stpt>", "<pet>", "<p0>", "<p1>", "<p2>", "<p3>", "<p4>", "<p5>"};
        constexpr const char* target_labels[]{"Current target <t>", "Self <me>", "Choose player <stpc>", "Choose enemy <stnpc>", "Choose target <st>", "Battle target <bt>", "Last subtarget <lastst>", "Choose alliance <stal>", "Choose party <stpt>", "Pet <pet>", "Party 1 <p0>", "Party 2 <p1>", "Party 3 <p2>", "Party 4 <p3>", "Party 5 <p4>", "Party 6 <p5>"};
        constexpr const char* equipment_slots[]{"main", "sub", "range", "ammo", "head", "body", "hands", "legs", "feet", "neck", "waist", "ear1", "ear2", "ring1", "ring2", "back"};
        constexpr size_t visible_results = 9;
        constexpr int result_first = 10000, category_first = 200, palette_first = 3000;
        constexpr int command_new = 510, command_copy = 511, command_rename = 512, command_delete = 513, command_up = 514, command_down = 515, command_select = 516;
        constexpr int command_preview = 530, command_import = 531, command_learned = 532, command_all_actions = 534;
        constexpr int menu_library = 550, menu_edit = 551, menu_palettes = 552, menu_lock = 553, menu_shape = 554, menu_settings = 555, menu_keybinds = 556;
        constexpr int new_macro = 560, new_command = 561, new_native = 562;
        constexpr int setup_enable = 600;

        auto Contains(const ui::rect& bounds, const float x, const float y) -> bool
        { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
        auto Lower(std::string value) -> std::string
        { for (auto& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return value; }
        auto Trim(std::string value) -> std::string
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return {};
            return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
        }
        auto Numeric(const std::string& value, uint32_t fallback = 0) -> uint32_t
        { uint32_t parsed{}; const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed); return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? parsed : fallback; }

    }

    actionbars_module::actionbars_module()
    {
        this->global_.custom_text = true;
        this->bars_[0].state.enabled = true;
        this->bars_[action_bar_count].size = 32;
        this->bars_[action_bar_count].columns = 8;
        this->bars_[action_bar_count].slots = 8;
        for (size_t index = 0; index < this->bars_.size(); ++index) this->Layout(index);
    }

    auto actionbars_module::CopyVisuals(const actionbar_presentation& source, actionbar_presentation& destination) -> void
    {
        const auto state = destination.state;
        const auto keys = destination.keys;
        const auto locked = destination.locked;
        const auto columns = destination.columns, slots = destination.slots;
        destination = source;
        destination.state = state; destination.keys = keys; destination.locked = locked;
        destination.columns = columns; destination.slots = slots;
    }

    auto actionbars_module::ApplyGlobalVisuals(void) -> void
    {
        for (size_t index = 0; index < action_bar_count; ++index)
            if (this->use_global_[index]) { CopyVisuals(this->global_, this->bars_[index]); this->Layout(index); }
    }

    auto actionbars_module::SavedPresentation(const size_t index) const -> actionbar_presentation
    {
        if (index == this->bars_.size()) return this->global_;
        auto result = this->bars_[index];
        if (index < action_bar_count && this->use_global_[index]) CopyVisuals(this->local_visuals_[index], result);
        return result;
    }

    auto actionbars_module::VisualOption(const size_t index) -> bool
    {
        return (index >= size && index <= padding) || index == selected_text ||
            (index >= slot_frame && index <= slot_reset) || index == vertical_gap || index == per_bar_text ||
            index == label_wrap || index >= base_options;
    }

    auto actionbars_module::Initialize(IAshitaCore* core, action_store* store, action_executor* executor) -> void
    { this->core_ = core; this->store_ = store; this->executor_ = executor; this->SyncStorage(); }

    auto actionbars_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.meter_text}, module_text_profile{theme.body_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}};
        this->text_[0].style.scale = 0.8f;
        this->text_[1].style.scale = this->text_[2].style.scale = 0.6f;
        this->text_[3].style.scale = 0.8f;
        this->text_[4].style.scale = 0.8f;
        this->text_[1].style.alignment = this->text_[3].style.alignment = ui::text_alignment::center;
        this->text_[2].style.alignment = ui::text_alignment::left;
        this->text_[5].style.scale = this->text_[6].style.scale = 0.6f;
        this->text_[5].style.alignment = ui::text_alignment::right;
        for (const size_t index : {size_t{1}, size_t{2}, size_t{3}, size_t{5}, size_t{6}})
        {
            auto& style = this->text_[index].style;
            style.top_color = style.bottom_color = 0xFFFFFFFF;
            style.stroke.enabled = style.shadow.enabled = true;
            style.stroke.color = 0xFF000000; style.shadow.color = 0xDC000000;
        }
        this->text_[6].style.top_color = this->text_[6].style.bottom_color = 0xFFBFE07D;
    }

    auto actionbars_module::ResetDefaults(const ui::theme& theme) -> void
    {
        this->SuspendInput();
        for (size_t index = 0; index < this->bars_.size(); ++index)
        {
            const auto position = this->bars_[index].state.bounds;
            this->bars_[index] = {};
            this->bars_[index].state.bounds.x = position.x; this->bars_[index].state.bounds.y = position.y;
        }
        this->global_ = {}; this->global_.custom_text = true; this->use_global_.fill(false);
        this->bars_[action_bar_count].size = 32; this->bars_[action_bar_count].slots = this->bars_[action_bar_count].columns = 8;
        this->bars_[0].state.enabled = true;
        this->inactive_ = this->prompts_ = this->crossbar_animations_ = true;
        this->allow_native_keys_ = false;
        this->shared_expanded_ = false; this->crossbar_visibility_ = 0; this->crossbar_delay_ = 5; this->crossbar_fade_ = .15f; this->crossbar_alpha_ = 1;
        this->skillchain_predictions_ = true;
        this->local_combat_settings_.ResetDefaults();
        this->magic_burst_settings_request_ = false;
        this->container_ = {1, 5, 8, false};
        this->ApplyTextDefaults(theme); this->global_.text = this->text_;
        for (size_t index = 0; index < this->bars_.size(); ++index) this->Layout(index);
    }

    auto actionbars_module::ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void
    {
        const auto& source = static_cast<const actionbars_module&>(defaults);
        this->bars_[0] = source.bars_[0]; this->use_global_[0] = source.use_global_[0]; this->local_visuals_[0] = source.local_visuals_[0];
        this->Layout(0); this->settings_changed_ = true;
    }

    auto actionbar_module::ResetSettingsFrom(const hud_module& defaults, const ui::theme&) -> void
    {
        const auto& source = static_cast<const actionbar_module&>(defaults);
        this->owner_.bars_[this->index_] = source.owner_.bars_[source.index_];
        if (this->index_ < action_bar_count)
        {
            this->owner_.use_global_[this->index_] = source.owner_.use_global_[source.index_];
            this->owner_.local_visuals_[this->index_] = source.owner_.local_visuals_[source.index_];
        }
        this->owner_.Layout(this->index_); this->owner_.settings_changed_ = true;
    }

    auto actionbars_module::Load(const settings_store& settings) -> void
    {
        this->SuspendInput();
        this->skillchain_predictions_ = settings.ReadBool("actionbars", "skillchain_predictions", settings.ReadBool("magicburst", "predictions", true));
        if (this->combat_settings_ == nullptr)
            this->local_combat_settings_.Load(settings);
        const int slotFrameVersion = settings.ReadInt("actionbars", "slot_frame_version", 0);
        auto legacySlots = actionbar_slot_style{}; legacySlots.frame = 0;
        for (size_t index = 0; index < this->workspace_bounds_.size(); ++index)
            this->workspace_bounds_[index] = settings.ReadRect(workspace_sections[index], this->workspace_bounds_[index]);
        auto& setupBounds = this->workspace_bounds_[3];
        if (settings.ReadInt("actionbars", "setup_layout_version", 0) < 1 && (setupBounds.height == 548 || setupBounds.height == 510))
        { setupBounds.height = 416; this->settings_changed_ = true; }
        for (size_t index = 0; index <= this->bars_.size(); ++index)
        {
            auto& bar = index == this->bars_.size() ? this->global_ : this->bars_[index];
            const auto section = std::string{"actionbars."} + bar_ids[index];
            bar.state.bounds = settings.ReadRect(("layout." + std::string{bar_ids[index]}).c_str(), bar.state.bounds);
            for (auto [key, value] : {std::pair{"enabled", &bar.state.enabled}, {"locked", &bar.locked}, {"heading", &bar.heading}, {"labels", &bar.labels}, {"bindings", &bar.bindings}, {"resources", &bar.resources}, {"cooldowns", &bar.cooldowns}, {"empty", &bar.empty}, {"background", &bar.background}, {"tooltips", &bar.tooltips}})
                *value = settings.ReadBool(section.c_str(), key, *value);
            const auto number = [&](const char* key, float value, float minimum, float maximum) { const auto result = settings.ReadFloat(section.c_str(), key, value); return std::clamp(std::isfinite(result) ? result : value, minimum, maximum); };
            bar.columns = std::round(number("columns", bar.columns, 1, 12)); bar.slots = std::round(number("slots", bar.slots, 1, 12));
            bar.size = number("icon_size", bar.size, 16, 72); bar.gap = number("spacing", bar.gap, 0, 32);
            bar.gap_y = number("vertical_spacing", -1, -1, 32); bar.custom_text = settings.ReadBool(section.c_str(), "custom_text", false);
            bar.label_wrap = settings.ReadBool(section.c_str(), "label_wrap", false); bar.button_icons = settings.ReadBool(section.c_str(), "button_icons", true);
            bar.diamond_spacing = number("diamond_spacing", -1, -1, 128); bar.group_spacing = number("group_spacing", 32, 0, 128);
            bar.prompt_size = number("prompt_size", 16, 8, 48); bar.prompt_gap_x = number("prompt_gap_x", 8, 0, 48); bar.prompt_gap_y = number("prompt_gap_y", 2, 0, 48);
            const auto slotSection = section + ".slots"; auto& style = bar.style;
            style.frame = std::clamp(settings.ReadInt(slotSection.c_str(), "frame", style.frame), 0, actionbar_slot_style::window_frame);
            style.cooldown_style = std::clamp(settings.ReadInt(slotSection.c_str(), "cooldown_style", style.cooldown_style), 0, 3);
            for (auto [key, value] : {std::pair{"binding_anchor", &style.binding_anchor}, {"count_anchor", &style.count_anchor}, {"timer_anchor", &style.timer_anchor}, {"cost_anchor", &style.cost_anchor}})
                *value = std::clamp(settings.ReadInt(slotSection.c_str(), key, *value), 0, 8);
            const auto slotNumber = [&](const char* key, float fallback, float minimum, float maximum) {
                const auto value = settings.ReadFloat(slotSection.c_str(), key, fallback);
                return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
            };
            style.radius = slotNumber("radius", style.radius, 0, 48);
            style.border_width = slotNumber("border_width", style.border_width, 0, 4);
            style.icon_inset = slotNumber("icon_inset", style.icon_inset, 0, 12);
            style.icon_opacity = slotNumber("icon_opacity", style.icon_opacity, 0, 1);
            style.inactive_opacity = slotNumber("inactive_opacity", style.inactive_opacity, 0, 1);
            for (auto [key, value] : {std::pair{"binding_x", &style.binding_x}, {"binding_y", &style.binding_y}, {"count_x", &style.count_x}, {"count_y", &style.count_y}, {"timer_x", &style.timer_x}, {"timer_y", &style.timer_y}, {"cost_x", &style.cost_x}, {"cost_y", &style.cost_y}})
                *value = slotNumber(key, *value, -48, 48);
            for (auto [key, value] : {std::pair{"shadow", &style.shadow}, {"sheen", &style.sheen}, {"hover_highlight", &style.hover_highlight}, {"press_highlight", &style.press_highlight}, {"activation_flash", &style.activation_flash}, {"cooldown_text", &style.cooldown_text}, {"mp_cost", &style.mp_cost}})
                *value = settings.ReadBool(slotSection.c_str(), key, *value);
            for (auto [key, value] : {std::pair{"fill", &style.fill}, {"empty_fill", &style.empty_fill}, {"border", &style.border}, {"icon_tint", &style.icon_tint}, {"unavailable_tint", &style.unavailable_tint}, {"inactive_tint", &style.inactive_tint}, {"hover_color", &style.hover_color}, {"pressed_color", &style.pressed_color}, {"cooldown_color", &style.cooldown_color}})
                *value = settings.ReadArgb(slotSection.c_str(), key, *value);
            if (settings.ReadInt(slotSection.c_str(), "frame_version", slotFrameVersion) < 1 && style == legacySlots)
            {
                style.frame = actionbar_slot_style::window_frame;
                this->settings_changed_ = true;
            }
            for (size_t slot = 0; slot < bar.keys.size(); ++slot)
            {
                const auto key = "slot" + std::to_string(slot + 1);
                bar.keys[slot].key = static_cast<uint16_t>(std::clamp(settings.ReadInt((section + ".keys").c_str(), key.c_str(), 0), 0, 255));
                bar.keys[slot].modifiers = static_cast<uint8_t>(std::clamp(settings.ReadInt((section + ".modifiers").c_str(), key.c_str(), 0), 0, 7));
            }
            this->Layout(index);
        }
        this->shared_expanded_ = settings.ReadBool("actionbars", "shared_expanded", false);
        this->crossbar_visibility_ = std::clamp(settings.ReadInt("actionbars", "crossbar_visibility", 0), 0, 2);
        this->crossbar_delay_ = std::clamp(settings.ReadFloat("actionbars", "crossbar_release_delay", 5), 0.0f, 30.0f);
        this->crossbar_fade_ = std::clamp(settings.ReadFloat("actionbars", "crossbar_fade", .15f), 0.0f, 2.0f);
        this->crossbar_alpha_ = this->crossbar_visibility_ == 0 ? 1.0f : 0.0f;
        this->visibility_updated_ = this->last_trigger_ = 0;
        this->inactive_ = settings.ReadBool("actionbars", "inactive_crossbar", true);
        this->prompts_ = settings.ReadBool("actionbars", "controller_prompts", true);
        this->crossbar_animations_ = settings.ReadBool("actionbars", "crossbar_animations", true);
        this->prompt_theme_ = std::clamp(settings.ReadInt("actionbars", "prompt_theme", 0), 0, 4);
        this->allow_native_keys_ = settings.ReadBool("actionbars", "replace_native_macros", false);
        this->container_.opacity = std::clamp(settings.ReadFloat("actionbars", "opacity", 1), 0.1f, 1.0f);
        this->container_.padding = std::clamp(settings.ReadFloat("actionbars", "padding", 8), 0.0f, 24.0f);
        if (settings.ReadInt("actionbars", "presentation_version", 0) < 1 && this->container_.padding == 8 && this->container_.opacity == 1)
            for (auto& bar : this->bars_) { bar.background = false; this->settings_changed_ = true; }
        for (size_t index = 0; index < this->bars_.size(); ++index) this->Layout(index);
        for (size_t index = 0; index < this->text_.size(); ++index)
        {
            auto fallback = index == 5 ? this->text_[2] : index == 6 ? this->text_[5] : this->text_[index];
            if (index == 5) fallback.style.alignment = ui::text_alignment::right;
            this->text_[index] = LoadTextProfile(settings, profile_sections[index], fallback);
        }
        if (settings.ReadInt("actionbars", "slot_text_version", 0) < 1)
        {
            const auto legacy = ui::theme::CreateDefault();
            for (const size_t index : {size_t{1}, size_t{2}, size_t{3}, size_t{5}, size_t{6}})
            {
                auto& style = this->text_[index].style;
                const auto& previous = index == 3 ? legacy.meter_text : legacy.caption_text;
                if (style.top_color == previous.top_color && style.bottom_color == previous.bottom_color &&
                    style.stroke.enabled == previous.stroke.enabled && style.shadow.enabled == previous.shadow.enabled)
                {
                    style.top_color = style.bottom_color = 0xFFFFFFFF;
                    style.stroke.enabled = style.shadow.enabled = true;
                }
            }
            this->settings_changed_ = true;
        }
        if (settings.ReadInt("actionbars", "slot_text_version", 0) < 2)
        {
            const auto source = this->text_[6].style;
            if (source.top_color == 0xFFFFFFFF && source.bottom_color == 0xFFFFFFFF)
            {
                const auto legacy = ui::theme::CreateDefault();
                for (const size_t index : {size_t{1}, size_t{2}})
                {
                    auto& style = this->text_[index].style;
                    if ((style.top_color == 0xFFFFFFFF && style.bottom_color == 0xFFFFFFFF) ||
                        (style.top_color == legacy.caption_text.top_color && style.bottom_color == legacy.caption_text.bottom_color))
                    {
                        const auto alignment = style.alignment;
                        style = source; style.alignment = alignment;
                    }
                }
                const auto green = settings.ReadArgb("meter.player.mp", "fill_bottom", settings.ReadArgb("player", "mp_color", 0xFFBFE07D));
                this->text_[6].style.top_color = this->text_[6].style.bottom_color = green;
            }
            this->settings_changed_ = true;
        }
        for (size_t barIndex = 0; barIndex <= this->bars_.size(); ++barIndex)
            for (size_t role = 0; role < this->text_.size(); ++role)
                (barIndex == this->bars_.size() ? this->global_ : this->bars_[barIndex]).text[role] = LoadTextProfile(settings, ("text.actionbars.bar" + std::to_string(barIndex) + "." + std::to_string(role)).c_str(), this->text_[role]);
        auto& controller = this->controller_settings_;
        for (auto [key, value] : {std::pair{"enabled", &controller.enabled}, {"replace_native", &controller.replace_native}, {"expanded", &controller.expanded}, {"double_tap", &controller.double_tap}, {"palette_cycle", &controller.palette_cycle}, {"palette_left_shoulder", &controller.palette_left_shoulder}})
            *value = settings.ReadBool("controller.crossbar.device", key, *value);
        controller.trigger_press = static_cast<uint16_t>(std::clamp(settings.ReadInt("controller.crossbar.device", "trigger_press", controller.trigger_press), 1, 255));
        controller.trigger_release = static_cast<uint16_t>(std::clamp(settings.ReadInt("controller.crossbar.device", "trigger_release", controller.trigger_release), 0, static_cast<int>(controller.trigger_press) - 1));
        controller.double_tap_ms = static_cast<uint16_t>(std::clamp(settings.ReadInt("controller.crossbar.device", "double_tap_ms", controller.double_tap_ms), 100, 1000));
        controller.min_hold_ms = static_cast<uint16_t>(std::clamp(settings.ReadInt("controller.crossbar.device", "min_hold_ms", controller.min_hold_ms), 0, 500));
        for (int backend = 0; backend < 2; ++backend)
        {
            auto& mapping = backend == 0 ? controller.xinput : controller.direct_input;
            const auto section = backend == 0 ? "controller.crossbar.device.xinput" : "controller.crossbar.device.directinput";
            const int minimum = backend == 0 ? -2 : -127, maximum = backend == 0 ? 15 : 127;
            mapping.left_trigger = static_cast<int16_t>(std::clamp(settings.ReadInt(section, "left_trigger", mapping.left_trigger), minimum, maximum));
            mapping.right_trigger = static_cast<int16_t>(std::clamp(settings.ReadInt(section, "right_trigger", mapping.right_trigger), minimum, maximum));
            mapping.palette_modifier = static_cast<int16_t>(std::clamp(settings.ReadInt(section, "palette_modifier", mapping.palette_modifier), minimum, maximum));
            for (size_t slot = 0; slot < 8; ++slot) mapping.slots[slot] = static_cast<int16_t>(std::clamp(settings.ReadInt(section, ("slot" + std::to_string(slot + 1)).c_str(), mapping.slots[slot]), minimum, maximum));
        }
        this->global_.custom_text = true;
        for (size_t index = 0; index < action_bar_count; ++index)
        {
            this->local_visuals_[index] = this->bars_[index];
            this->use_global_[index] = settings.ReadBool((std::string{"actionbars."} + bar_ids[index]).c_str(), "use_global", false);
        }
        this->ApplyGlobalVisuals();
        this->SyncStorage();
    }

    auto actionbars_module::Save(const settings_store& settings) const -> void
    {
        for (size_t index = 0; index < this->workspace_bounds_.size(); ++index)
            settings.WriteRect(workspace_sections[index], this->window_gesture_ != window_gesture::none && index == this->window_workspace_ ? this->window_original_ : this->workspace_bounds_[index]);
        settings.WriteInt("actionbars", "presentation_version", 1);
        settings.WriteBool("actionbars", "skillchain_predictions", this->skillchain_predictions_);
        if (this->combat_settings_ == nullptr)
            this->local_combat_settings_.Save(settings);
        for (size_t index = 0; index <= this->bars_.size(); ++index)
        {
            const auto bar = this->SavedPresentation(index); const auto section = std::string{"actionbars."} + bar_ids[index];
            settings.WriteRect(("layout." + std::string{bar_ids[index]}).c_str(), bar.state.bounds);
            for (auto [key, value] : {std::pair{"enabled", bar.state.enabled}, {"locked", bar.locked}, {"heading", bar.heading}, {"labels", bar.labels}, {"bindings", bar.bindings}, {"resources", bar.resources}, {"cooldowns", bar.cooldowns}, {"empty", bar.empty}, {"background", bar.background}, {"tooltips", bar.tooltips}})
                settings.WriteBool(section.c_str(), key, value);
            for (auto [key, value] : {std::pair{"columns", bar.columns}, {"slots", bar.slots}, {"icon_size", bar.size}, {"spacing", bar.gap}}) settings.WriteFloat(section.c_str(), key, value);
            const auto slotSection = section + ".slots"; const auto& style = bar.style;
            settings.WriteInt(slotSection.c_str(), "frame_version", 1);
            for (auto [key, value] : {std::pair{"frame", style.frame}, {"cooldown_style", style.cooldown_style}, {"binding_anchor", style.binding_anchor}, {"count_anchor", style.count_anchor}, {"timer_anchor", style.timer_anchor}, {"cost_anchor", style.cost_anchor}})
                settings.WriteInt(slotSection.c_str(), key, value);
            for (auto [key, value] : {std::pair{"radius", style.radius}, {"border_width", style.border_width}, {"icon_inset", style.icon_inset}, {"icon_opacity", style.icon_opacity}, {"inactive_opacity", style.inactive_opacity}, {"binding_x", style.binding_x}, {"binding_y", style.binding_y}, {"count_x", style.count_x}, {"count_y", style.count_y}, {"timer_x", style.timer_x}, {"timer_y", style.timer_y}, {"cost_x", style.cost_x}, {"cost_y", style.cost_y}})
                settings.WriteFloat(slotSection.c_str(), key, value);
            for (auto [key, value] : {std::pair{"shadow", style.shadow}, {"sheen", style.sheen}, {"hover_highlight", style.hover_highlight}, {"press_highlight", style.press_highlight}, {"activation_flash", style.activation_flash}, {"cooldown_text", style.cooldown_text}, {"mp_cost", style.mp_cost}})
                settings.WriteBool(slotSection.c_str(), key, value);
            for (auto [key, value] : {std::pair{"fill", style.fill}, {"empty_fill", style.empty_fill}, {"border", style.border}, {"icon_tint", style.icon_tint}, {"unavailable_tint", style.unavailable_tint}, {"inactive_tint", style.inactive_tint}, {"hover_color", style.hover_color}, {"pressed_color", style.pressed_color}, {"cooldown_color", style.cooldown_color}})
                settings.WriteArgb(slotSection.c_str(), key, value);
            for (size_t slot = 0; slot < bar.keys.size(); ++slot)
            {
                const auto key = "slot" + std::to_string(slot + 1);
                settings.WriteInt((section + ".keys").c_str(), key.c_str(), bar.keys[slot].key);
                settings.WriteInt((section + ".modifiers").c_str(), key.c_str(), bar.keys[slot].modifiers);
            }
        }
        for (size_t index = 0; index < action_bar_count; ++index)
            settings.WriteBool((std::string{"actionbars."} + bar_ids[index]).c_str(), "use_global", this->use_global_[index]);
        settings.WriteBool("actionbars", "shared_expanded", this->shared_expanded_);
        settings.WriteInt("actionbars", "crossbar_visibility", this->crossbar_visibility_);
        settings.WriteFloat("actionbars", "crossbar_release_delay", this->crossbar_delay_); settings.WriteFloat("actionbars", "crossbar_fade", this->crossbar_fade_);
        for (size_t barIndex = 0; barIndex <= this->bars_.size(); ++barIndex)
        {
            const auto section = std::string{"actionbars."} + bar_ids[barIndex]; const auto bar = this->SavedPresentation(barIndex);
            settings.WriteFloat(section.c_str(), "vertical_spacing", bar.gap_y); settings.WriteBool(section.c_str(), "custom_text", bar.custom_text);
            settings.WriteBool(section.c_str(), "label_wrap", bar.label_wrap); settings.WriteBool(section.c_str(), "button_icons", bar.button_icons);
            for (const auto& [key, value] : {std::pair{"diamond_spacing", bar.diamond_spacing}, {"group_spacing", bar.group_spacing}, {"prompt_size", bar.prompt_size}, {"prompt_gap_x", bar.prompt_gap_x}, {"prompt_gap_y", bar.prompt_gap_y}}) settings.WriteFloat(section.c_str(), key, value);
            for (size_t role = 0; role < this->text_.size(); ++role) SaveTextProfile(settings, ("text.actionbars.bar" + std::to_string(barIndex) + "." + std::to_string(role)).c_str(), bar.text[role]);
        }
        settings.WriteBool("actionbars", "inactive_crossbar", this->inactive_); settings.WriteBool("actionbars", "controller_prompts", this->prompts_);
        settings.WriteBool("actionbars", "crossbar_animations", this->crossbar_animations_);
        settings.WriteInt("actionbars", "prompt_theme", this->prompt_theme_);
        settings.WriteBool("actionbars", "replace_native_macros", this->allow_native_keys_);
        settings.WriteInt("actionbars", "slot_frame_version", 1);
        settings.WriteFloat("actionbars", "opacity", this->container_.opacity); settings.WriteFloat("actionbars", "padding", this->container_.padding);
        settings.WriteInt("actionbars", "slot_text_version", 2);
        settings.WriteInt("actionbars", "setup_layout_version", 1);
        for (size_t index = 0; index < this->text_.size(); ++index) SaveTextProfile(settings, profile_sections[index], this->text_[index]);
        const auto& controller = this->controller_settings_;
        for (auto [key, value] : {std::pair{"enabled", controller.enabled}, {"replace_native", controller.replace_native}, {"expanded", controller.expanded}, {"double_tap", controller.double_tap}, {"palette_cycle", controller.palette_cycle}, {"palette_left_shoulder", controller.palette_left_shoulder}})
            settings.WriteBool("controller.crossbar.device", key, value);
        settings.WriteInt("controller.crossbar.device", "trigger_press", controller.trigger_press); settings.WriteInt("controller.crossbar.device", "trigger_release", controller.trigger_release);
        settings.WriteInt("controller.crossbar.device", "double_tap_ms", controller.double_tap_ms); settings.WriteInt("controller.crossbar.device", "min_hold_ms", controller.min_hold_ms);
        for (int backend = 0; backend < 2; ++backend)
        {
            const auto& mapping = backend == 0 ? controller.xinput : controller.direct_input;
            const auto section = backend == 0 ? "controller.crossbar.device.xinput" : "controller.crossbar.device.directinput";
            settings.WriteInt(section, "left_trigger", mapping.left_trigger); settings.WriteInt(section, "right_trigger", mapping.right_trigger); settings.WriteInt(section, "palette_modifier", mapping.palette_modifier);
            for (size_t slot = 0; slot < 8; ++slot) settings.WriteInt(section, ("slot" + std::to_string(slot + 1)).c_str(), mapping.slots[slot]);
        }
    }

    auto actionbars_module::ConfigViews(void) const -> std::span<const char* const> { return config_views; }
    auto actionbars_module::SelectConfigView(const size_t value) -> void
    {
        if (value == 11) { this->OpenLibrary(this->Surface() == action_surface::crossbar, this->ConfigBar()); return; }
        if (value == 12) { this->OpenPalettes(this->Surface() == action_surface::crossbar, this->ConfigBar()); return; }
        if (value == 13) { this->OpenImport(); return; }
        this->view_ = value == global_view ? global_view : std::min(value, action_bar_count);
        if (this->view_ < action_bar_count) this->selected_bar_ = static_cast<int>(this->view_);
        this->SyncStorage();
    }
    auto actionbars_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{ {"Actions", "Assignments and action storage.", "actions"}, {"Layout", "Bar shape, slot size and spacing.", "layout"}, {"Display", "Labels, empty slots and container.", "display"},
            {"Slots", "Frame, corners, artwork and colors.", "slots"}, {"Effects", "Unavailable actions, highlights and cooldowns.", "effects"}, {"Overlays", "Key bindings, counts, MP costs and recast text.", "overlays"},
            {"Controller", "Trigger controls and device setup.", "controller"}, {"Text", "Fonts, colors and effects for each text element.", "text"},
            {"Skillchains", "Shared skillchain and Magic Burst predictions for all hotbars and the crossbar. Timings are estimates.", "skillchains"},
            {"Keybinds", "Rebind hotbar slots and crossbar controller buttons.", "keybinds"},
            {"Window", "Bar background, inner padding and opacity.", "window"} };
        static const auto crossbarSections = [] {
            std::array<module_config_section, crossbar_sections.size()> result{};
            for (size_t index = 0; index < result.size(); ++index) result[index] = sections[crossbar_sections[index]];
            result[0].name = "General";
            result[1].name = "Controller Setup";
            return result;
        }();
        static const auto hotbarSections = [] {
            std::array<module_config_section, hotbar_sections.size()> result{};
            for (size_t index = 0; index < result.size(); ++index) result[index] = sections[hotbar_sections[index]];
            return result;
        }();
        static const std::array<module_config_section, 7> globalSections{sections[1], sections[2], sections[3], sections[4], sections[5], sections[7], sections[10]};
        if (this->view_ == global_view) return globalSections;
        return this->view_ == action_bar_count ? std::span<const module_config_section>{crossbarSections} : std::span<const module_config_section>{hotbarSections};
    }
    auto actionbars_module::OpenKeybindsSection(const size_t section) -> bool
    {
        if (!this->ConfigSectionContains(section, keybinds)) return false;
        this->OpenKeybinds(this->Surface() == action_surface::crossbar, this->ConfigBar());
        return true;
    }
    auto actionbars_module::ContentConfigSectionContains(size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        const auto topic = this->ConfigSections()[section].Id();
        if (this->view_ == global_view && !VisualOption(index)) return false;
        if (index == use_global) return this->view_ < action_bar_count && topic == "layout";
        if (index == keybinds) return topic == "keybinds";
        if (index == edit || index == palettes || index == import) return false;
        if (index == bar_selector) return this->view_ < action_bar_count;
        if (index == bank) return this->view_ == action_bar_count && topic == "layout";
        if (index == shared_expanded || index == visibility_mode || index == visibility_delay || index == visibility_fade) return topic == "controller";
        if (index == vertical_gap || index == diamond_spacing || index == group_spacing) return topic == "layout";
        if (index == per_bar_text || index == label_wrap) return topic == "text";
        if (index == button_icons || index == prompt_size || index == prompt_gap_x || index == prompt_gap_y) return topic == "controller";
        if (index == crossbar_animations) return topic == "controller";
        if (index >= skillchain_predictions && index <= magic_burst_settings) return topic == "skillchains";
        if (index >= base_options || index == selected_text) return topic == "text";
        if (index >= setup && index <= double_window) return topic == "controller";
        if (index >= slot_frame && index < base_options)
        {
            if (index >= slot_binding_anchor && index <= slot_cost_y || index == slot_cooldown_text) return topic == "overlays";
            if (index >= slot_unavailable_tint && index <= slot_cooldown_color) return topic == "effects";
            return topic == "slots";
        }
        if (index == bindings || index == resources || index == cooldowns) return topic == "overlays";
        if (index == background || index == opacity || index == padding) return topic == "window";
        if (index >= columns && index <= gap || index == bank || index == shape) return topic == "layout";
        if (index >= heading && index <= padding) return topic == "display";
        return topic == "actions";
    }
    auto actionbars_module::ContentOption(const size_t index) -> module_option
    {
        auto result = this->PresentationOption(index);
        if (this->view_ < action_bar_count && this->use_global_[this->view_] && VisualOption(index) && index != selected_text)
            result.available = false;
        return result;
    }

    auto actionbars_module::PresentationOption(const size_t index) -> module_option
    {
        auto& bar = this->ConfigPresentation();
        static constexpr const char* scopes[]{"Global", "Main job (shared subjob)", "Main job + subjob"};
        static constexpr const char* profiles[]{"Title", "Action label", "Key / button", "Countdown", "Tooltip", "Item count / charges", "MP cost"};
        static constexpr const char* frames[]{"Beveled", "Flat", "Outline", "Icon only", "Window theme"};
        static constexpr const char* cooldownStyles[]{"Clock sweep", "Fill from bottom", "Fill from top", "No overlay"};
        static constexpr const char* anchors[]{"Top left", "Top center", "Top right", "Middle left", "Center", "Middle right", "Bottom left", "Bottom center", "Bottom right"};
        static constexpr const char* promptThemes[]{"Auto", "Xbox", "PlayStation", "Nintendo", "Position labels"};
        if (index >= base_options) return TextProfileOption(index - base_options, bar.custom_text ? bar.text[this->selected_text_] : this->text_[this->selected_text_]);
        const auto controllerOnly = [&](module_option option) { return option.VisibleWhen(this->view_ == action_bar_count); };
        const auto customSlot = [&](module_option option) { return option.VisibleWhen(bar.style.frame != actionbar_slot_style::window_frame); };
        switch (index)
        {
        case use_global: return module_option::Toggle("Use Global Settings", "Follow Global size, spacing, display, slot effects and text. Turn off to restore independent visuals. Shapes, positions, actions and keys stay independent.", this->view_ < action_bar_count ? &this->use_global_[this->view_] : nullptr).Ordered(-100).VisibleWhen(this->view_ < action_bar_count);
        case shared_expanded: return controllerOnly(module_option::Toggle("Shared expanded bank", "Both trigger orders use a separate Shared bank. Ordered-bank assignments are retained.", &this->shared_expanded_));
        case vertical_gap: return module_option::Slider("Vertical spacing", "Row spacing; -1 follows horizontal spacing.", &bar.gap_y, -1, 32, 1).Ordered(122);
        case per_bar_text: return module_option::Toggle("Independent text styles", "Customize text for this bar instead of the shared Action Bars styles.", &bar.custom_text).Ordered(-50).VisibleWhen(this->view_ != global_view);
        case label_wrap: return module_option::Toggle("Wrap action labels", "Allow up to two lines below action icons.", &bar.label_wrap).VisibleWhen(bar.labels);
        case diamond_spacing: return controllerOnly(module_option::Slider("Diamond spacing", "Space between D-pad and face-button diamonds; -1 uses the default.", &bar.diamond_spacing, -1, 128, 1).Ordered(123));
        case group_spacing: return controllerOnly(module_option::Slider("Trigger group spacing", "Space between the left and right crossbar groups.", &bar.group_spacing, 0, 128, 1).Ordered(124));
        case button_icons: return controllerOnly(module_option::Toggle("Button icons", "Show button prompts for crossbar slots.", &bar.button_icons));
        case prompt_size: return controllerOnly(module_option::Slider("Button icon size", "Size of crossbar button artwork.", &bar.prompt_size, 8, 48, 1).VisibleWhen(bar.button_icons));
        case prompt_gap_x: return controllerOnly(module_option::Slider("Button icon horizontal spacing", "Horizontal separation of center button prompts.", &bar.prompt_gap_x, 0, 48, 1).VisibleWhen(bar.button_icons));
        case prompt_gap_y: return controllerOnly(module_option::Slider("Button icon vertical spacing", "Vertical separation of center button prompts.", &bar.prompt_gap_y, 0, 48, 1).VisibleWhen(bar.button_icons));
        case visibility_mode: { static constexpr const char* modes[]{"Normal", "Triggers held", "Combat or triggers"}; return controllerOnly(module_option::Choice("Crossbar visibility", "Show normally, while triggers are held, or during combat and after trigger use.", &this->crossbar_visibility_, modes, 3)); }
        case visibility_delay: return controllerOnly(module_option::Slider("Trigger release delay", "Seconds to retain Combat or triggers mode after releasing a trigger.", &this->crossbar_delay_, 0, 30, .5f).VisibleWhen(this->crossbar_visibility_ == 2));
        case visibility_fade: return controllerOnly(module_option::Slider("Visibility fade", "Seconds for crossbar visibility transitions; zero changes immediately.", &this->crossbar_fade_, 0, 2, .05f).VisibleWhen(this->crossbar_visibility_ != 0));
        case keybinds: return module_option::Action("Keybinds", "Open the slot list to rebind hotbar keys or crossbar controller buttons.", "Open").Ordered(-40);
        case skillchain_predictions: return module_option::Toggle("Action predictions", "Highlight compatible weapon skills/abilities and burstable spells on all hotbars and the crossbar. Works independently of the Magic Burst HUD.", &this->skillchain_predictions_);
        case skillchain_rules: return module_option::Choice("Skillchain rules", "Shared combat profile for predictions, debuff estimates and Phantom Roll base values. Phoenix uses its 75-cap rules. Changing rules clears combat observations.", this->CombatSettings().RulesChoice(), server_rule_names, 3);
        case magic_burst_settings: return module_option::Action("Magic Burst HUD", "Open the separate burst display's visibility, appearance and text settings.", "Open HUD settings");
        case enabled: return module_option::Toggle("Enabled", "Show this bar; other bars keep their own enabled state.", &bar.state.enabled).Ordered(-100);
        case locked: return module_option::Toggle("Lock slot positions", "Prevent dragging existing actions. Click to activate; right-click for slot options.", &bar.locked).Ordered(-90);
        case edit: { auto option = module_option::Action("Actions & macros", "Open Actions & Macros from the destination tabs.", "Open library").Ordered(-70); option.available = false; return option; }
        case palettes: { auto option = module_option::Action("Action sets", "Open Action Sets from the destination tabs.", "Manage sets").Ordered(-60); option.available = false; return option; }
        case import: { auto option = module_option::Action("Import Lua / tHotBar / tCrossBar", "Open Import from the destination tabs.", "Preview import"); option.available = false; return option; }
        case scope: return module_option::Choice("Action storage", "Choose which job context supplies the action sets. A new context starts with a copy; existing contexts keep their saved actions.", &this->storage_scope_, scopes, 3);
        case pet_aware: return module_option::Toggle("Pet-aware assignments", "Use matching pet overrides when available. The editor can save actions for the current pet.", &this->storage_pet_);
        case columns: return module_option::Slider("Columns", "Wrap normal bars into a grid.", &bar.columns, 1, 12, 1).Ordered(110, true).VisibleWhen(this->view_ != action_bar_count && this->shape_ == 6);
        case slots: return module_option::Slider("Visible slots", "Unused assignments beyond this number remain saved.", &bar.slots, 1, 12, 1).Ordered(111).VisibleWhen(this->view_ != action_bar_count && this->shape_ == 6);
        case shape:
        {
            static constexpr const char* shapes[]{"12 x 1", "6 x 2", "4 x 3", "3 x 4", "2 x 6", "1 x 12", "Custom"};
            auto value = module_option::Choice("Bar shape", "Right-click the bar number to cycle shapes. Assignments keep their order.", &this->shape_, shapes, 7).Ordered(100);
            return value.VisibleWhen(this->view_ != action_bar_count);
        }
        case size: return module_option::Slider("Slot size", "Overall slot dimensions; artwork padding is configured under Slots.", &bar.size, 16, 72, 1).Ordered(120, true);
        case gap: return module_option::Slider("Spacing", "Space between slots; crossbar diamonds keep their shape.", &bar.gap, 0, 32, 1).Ordered(121);
        case heading: return module_option::Toggle("Action set caption", "Show the selected action set below the icons. Click it to switch sets.", &bar.heading);
        case labels: return module_option::Toggle("Action labels", "Show action names below their icons.", &bar.labels);
        case bindings: return module_option::Toggle("Key / button labels", "Show configured key bindings or controller positions.", &bar.bindings).Ordered(100);
        case resources: return module_option::Toggle("Item counts and charges", "Show the current inventory quantity or remaining charges.", &bar.resources).Ordered(200);
        case cooldowns: return module_option::Toggle("Recast display", "Show live cooldown overlays and remaining recast time.", &bar.cooldowns).Ordered(400);
        case empty: return module_option::Toggle("Empty slots", "Keep unassigned slots visible. Unlocked bars always show assignment targets.", &bar.empty);
        case background: return module_option::Toggle("Container background", "Use the shared native game window.", &bar.background);
        case tooltips: return module_option::Toggle("Tooltips", "Show action, target, resource and availability details on hover or focus.", &bar.tooltips);
        case opacity: return module_option::Slider("Background opacity", "Opacity shared by action-bar containers.", &this->container_.opacity, 0.1f, 1, 0.05f).Ordered(300, true);
        case padding: return module_option::Slider("Padding", "Space inside action-bar containers.", &this->container_.padding, 0, 24, 1).Ordered(301);
        case bank: return controllerOnly(module_option::Element("Edited bank", "Choose which trigger bank the editor and storage controls modify.", &this->selected_bank_, bank_names, action_crossbar_banks));
        case setup: return controllerOnly(module_option::Action("Controller Setup", "Choose your device, trigger feel and explicit native macro replacement.", "Set up controller").Ordered(-100));
        case crossbar_enabled: return controllerOnly(module_option::Toggle("Use XIUI trigger controls", "Route trigger actions to XIUI. Chat, native menus and target selection retain their controls. Device settings stay local.", &this->trigger_controls_));
        case native_replace: return module_option::Toggle("Native trigger replacement", "", &this->controller_settings_.replace_native).VisibleWhen(false);
        case expanded: return controllerOnly(module_option::Toggle("Ordered two-trigger banks", "Left then right and right then left open separate banks.", &this->controller_settings_.expanded));
        case double_tap: return controllerOnly(module_option::Toggle("Double-tap banks", "Tap and hold the same trigger to open its extra bank.", &this->controller_settings_.double_tap));
        case palette_cycle: return controllerOnly(module_option::Toggle("Shoulder + D-pad palette switching", "While holding a trigger bank, hold the palette shoulder and press D-pad Up/Down to cycle crossbar palettes.", &this->controller_settings_.palette_cycle));
        case inactive: return controllerOnly(module_option::Toggle("Show inactive crossbar", "Keep both base banks visible with no trigger held.", &this->inactive_));
        case prompts: return controllerOnly(module_option::Toggle("Controller prompts", "Show controller button artwork and trigger badges.", &this->prompts_));
        case prompt_theme: return controllerOnly(module_option::Choice("Button prompts", "Auto uses Xbox artwork for XInput and its preview. Choose your controller artwork explicitly for DirectInput; unidentified buttons use numbered labels.", &this->prompt_theme_, promptThemes, 5)).VisibleWhen(this->prompts_);
        case crossbar_animations: return controllerOnly(module_option::Toggle("Crossbar animations", "Animate trigger and button presses, and slide between trigger banks. Actions activate immediately.", &this->crossbar_animations_));
        case double_window: return controllerOnly(module_option::Action("Timing and mappings", "Adjust thresholds and timings, or choose an explicit device preset.", "Open setup"));
        case native_keys: return module_option::Toggle("Replace native macros", "Disable native keyboard/controller macros and their palette UI. Turn off to restore native macros.", &this->allow_native_keys_);
        case slot_frame: return module_option::Choice("Frame style", "Window theme follows the current native Window Type. Other styles use the custom slot colors.", &bar.style.frame, frames, std::size(frames)).Ordered(100);
        case slot_radius: return customSlot(module_option::Slider("Corner radius", "Round custom frames and artwork; Window theme keeps its native corners.", &bar.style.radius, 0, 48, 1).Ordered(110, true));
        case slot_border_width: return module_option::Slider("Border thickness", "Custom frame width or native frame scale; zero removes the border.", &bar.style.border_width, 0, 4, 1).Ordered(111);
        case slot_icon_inset: return module_option::Slider("Icon padding", "Inset artwork without changing the slot's click area or spacing.", &bar.style.icon_inset, 0, 12, 1).Ordered(120, true);
        case slot_icon_opacity: { auto value = module_option::Slider("Icon opacity (%)", "Artwork transparency, independent of the frame and text.", &bar.style.icon_opacity, 0, 1, 0.05f).Ordered(121); value.display_scale = 100; return value; }
        case slot_shadow: return customSlot(module_option::Toggle("Slot shadow", "Draw a small shadow behind custom slot frames.", &bar.style.shadow).Ordered(130, true));
        case slot_sheen: return module_option::Toggle("Top highlight", "Draw the upper edge highlight on beveled frames.", &bar.style.sheen).Ordered(131).VisibleWhen(bar.style.frame == 0);
        case slot_fill: return module_option::Color("Slot background", "Assigned-slot color for custom frames. Window theme uses the native background.", &bar.style.fill).Ordered(200, true).VisibleWhen(bar.style.frame < 2);
        case slot_empty_fill: return customSlot(module_option::Color("Empty background", "Unassigned-slot color for custom frames. Window theme uses the native background.", &bar.style.empty_fill).Ordered(201));
        case slot_border: return module_option::Color("Border color", "Custom frame color. Window theme retains the native border artwork.", &bar.style.border).Ordered(210, true).VisibleWhen(bar.style.frame < 3);
        case slot_icon_tint: return module_option::Color("Icon tint", "Multiply artwork by this color; white keeps the original colors.", &bar.style.icon_tint).Ordered(211);
        case slot_unavailable_tint: return module_option::Color("Unavailable icon tint", "Artwork tint when the action cannot currently be used.", &bar.style.unavailable_tint).Ordered(100);
        case slot_inactive_tint: return controllerOnly(module_option::Color("Inactive bank tint", "Artwork tint for the other crossbar trigger bank.", &bar.style.inactive_tint).Ordered(110, true));
        case slot_inactive_opacity: { auto value = controllerOnly(module_option::Slider("Inactive bank opacity (%)", "Transparency of the other crossbar trigger bank.", &bar.style.inactive_opacity, 0, 1, 0.05f).Ordered(111)); value.display_scale = 100; return value; }
        case slot_hover_highlight: return module_option::Toggle("Hover highlight", "Outline the slot under the pointer.", &bar.style.hover_highlight).Ordered(200, true);
        case slot_hover_color: return module_option::Color("Hover color", "Pointer highlight color and alpha.", &bar.style.hover_color).Ordered(201).VisibleWhen(bar.style.hover_highlight);
        case slot_press_highlight: return module_option::Toggle("Pressed highlight", "Outline a held mouse button or controller action.", &bar.style.press_highlight).Ordered(210, true);
        case slot_pressed_color: return module_option::Color("Pressed color", "Held and activation highlight color and alpha.", &bar.style.pressed_color).Ordered(211).VisibleWhen(bar.style.press_highlight || bar.style.activation_flash);
        case slot_activation_flash: return module_option::Toggle("Activation flash", "Briefly highlight an action when activated.", &bar.style.activation_flash).Ordered(220);
        case slot_cooldown_style: return module_option::Choice("Recast overlay", "How remaining cooldown time covers the icon.", &bar.style.cooldown_style, cooldownStyles, 4).Ordered(300).VisibleWhen(bar.cooldowns);
        case slot_cooldown_color: return module_option::Color("Recast overlay color", "Cooldown shading color and alpha.", &bar.style.cooldown_color).Ordered(310).VisibleWhen(bar.cooldowns && bar.style.cooldown_style != 3);
        case slot_cooldown_text: return module_option::Toggle("Recast countdown", "Show remaining time, even when the overlay is set to None.", &bar.style.cooldown_text).Ordered(410).VisibleWhen(bar.cooldowns);
        case slot_binding_anchor: return module_option::Choice("Key label position", "Anchor the key label inside each slot.", &bar.style.binding_anchor, anchors, 9).Ordered(110).VisibleWhen(bar.bindings && this->view_ < action_bar_count);
        case slot_binding_x: return module_option::Slider("Key X offset", "Horizontal adjustment from the key label anchor.", &bar.style.binding_x, -48, 48, 1).Ordered(120, true).VisibleWhen(bar.bindings && this->view_ < action_bar_count);
        case slot_binding_y: return module_option::Slider("Key Y offset", "Vertical adjustment from the key label anchor.", &bar.style.binding_y, -48, 48, 1).Ordered(121).VisibleWhen(bar.bindings && this->view_ < action_bar_count);
        case slot_count_anchor: return module_option::Choice("Count position", "Anchor item quantities and charges inside each slot.", &bar.style.count_anchor, anchors, 9).Ordered(210).VisibleWhen(bar.resources);
        case slot_count_x: return module_option::Slider("Count X offset", "Horizontal adjustment from the count anchor.", &bar.style.count_x, -48, 48, 1).Ordered(220, true).VisibleWhen(bar.resources);
        case slot_count_y: return module_option::Slider("Count Y offset", "Vertical adjustment from the count anchor.", &bar.style.count_y, -48, 48, 1).Ordered(221).VisibleWhen(bar.resources);
        case slot_timer_anchor: return module_option::Choice("Recast position", "Anchor remaining recast time inside each slot.", &bar.style.timer_anchor, anchors, 9).Ordered(420).VisibleWhen(bar.cooldowns && bar.style.cooldown_text);
        case slot_timer_x: return module_option::Slider("Recast X offset", "Horizontal adjustment from the recast anchor.", &bar.style.timer_x, -48, 48, 1).Ordered(430, true).VisibleWhen(bar.cooldowns && bar.style.cooldown_text);
        case slot_timer_y: return module_option::Slider("Recast Y offset", "Vertical adjustment from the recast anchor.", &bar.style.timer_y, -48, 48, 1).Ordered(431).VisibleWhen(bar.cooldowns && bar.style.cooldown_text);
        case slot_mp_cost: return module_option::Toggle("MP cost", "Show the action's MP cost on its slot.", &bar.style.mp_cost).Ordered(300);
        case slot_cost_anchor: return module_option::Choice("MP cost position", "Anchor MP costs inside each slot.", &bar.style.cost_anchor, anchors, 9).Ordered(310).VisibleWhen(bar.style.mp_cost);
        case slot_cost_x: return module_option::Slider("MP X offset", "Horizontal adjustment from the MP cost anchor.", &bar.style.cost_x, -48, 48, 1).Ordered(320, true).VisibleWhen(bar.style.mp_cost);
        case slot_cost_y: return module_option::Slider("MP Y offset", "Vertical adjustment from the MP cost anchor.", &bar.style.cost_y, -48, 48, 1).Ordered(321).VisibleWhen(bar.style.mp_cost);
        case slot_apply_all: return module_option::Action("Apply design to all bars", "Copy this slot design to every hotbar and the crossbar.", "Apply to all").Ordered(900, true);
        case slot_reset: return module_option::Action("Reset this slot design", "Restore this bar's default slot appearance.", "Reset design").Ordered(901);
        case selected_text:
            return this->view_ < action_bar_count ?
                module_option::Dropdown("Text element", "Independent fonts, colors, stroke, shadow and glow for each text element.", &this->selected_text_, profiles, 7).Ordered(-100) :
                module_option::Element("Selected text", "Independent fonts, colors, stroke, shadow and glow for each text element.", &this->selected_text_, profiles, 7);
        case bar_selector:
        {
            auto option = module_option::Element("Hotbar", "Choose the hotbar to configure. Every bar keeps its own layout, assignments and bindings.", &this->selected_bar_, bar_names, action_bar_count).Ordered(-200);
            option.choice_status_mask = (1u << action_bar_count) - 1;
            for (size_t barIndex = 0; barIndex < action_bar_count; ++barIndex)
                if (this->bars_[barIndex].state.enabled) option.choice_status_enabled |= 1u << barIndex;
            return option;
        }
        default: return {};
        }
    }
    auto actionbars_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index == per_bar_text && this->ConfigPresentation().custom_text) this->ConfigPresentation().text = this->text_;
        if (index == shared_expanded) this->SuspendInput();
        if (index == use_global && this->view_ < action_bar_count)
        {
            if (this->use_global_[this->view_]) this->local_visuals_[this->view_] = this->bars_[this->view_];
            else CopyVisuals(this->local_visuals_[this->view_], this->bars_[this->view_]);
        }
        switch (index)
        {
        case keybinds: this->OpenKeybinds(this->Surface() == action_surface::crossbar, this->ConfigBar()); break;
        case magic_burst_settings: this->magic_burst_settings_request_ = true; break;
        case bar_selector: this->SelectConfigView(static_cast<size_t>(std::clamp(this->selected_bar_, 0, 9))); break;
        case edit: this->OpenLibrary(this->Surface() == action_surface::crossbar, this->ConfigBar()); break;
        case shape:
            if (this->view_ != action_bar_count && this->shape_ < 6) { static constexpr float columns[]{12, 6, 4, 3, 2, 1}; this->ConfigPresentation().columns = columns[this->shape_]; this->ConfigPresentation().slots = 12; }
            break;
        case crossbar_enabled:
            this->controller_settings_.enabled = this->controller_settings_.replace_native = this->trigger_controls_;
            if (this->trigger_controls_) this->bars_[action_bar_count].state.enabled = true;
            break;
        case native_keys: if (!this->allow_native_keys_) this->native_macro_modifiers_ = 0; break;
        case palettes: this->OpenPalettes(this->Surface() == action_surface::crossbar, this->ConfigBar()); break;
        case import: this->OpenImport(); break;
        case setup: case double_window: this->OpenSetup(); break;
        case scope: if (this->store_) this->store_->SetScope(this->Surface(), this->ConfigBar(), static_cast<action_scope>(this->storage_scope_)); break;
        case pet_aware: if (this->store_) this->store_->SetPetAware(this->Surface(), this->ConfigBar(), this->storage_pet_); break;
        case bank: this->SyncStorage(); break;
        case slot_apply_all: { const auto design = this->ConfigPresentation().style; for (auto& bar : this->bars_) bar.style = design; break; }
        case slot_reset: this->ConfigPresentation().style = {}; break;
        default: break;
        }
        if (index == enabled && !this->ConfigPresentation().state.enabled) this->SuspendInput();
        this->ApplyGlobalVisuals();
        for (size_t bar = 0; bar < this->bars_.size(); ++bar) this->Layout(bar);
        this->SyncStorage();
        this->settings_changed_ = true;
    }

    auto actionbars_module::SyncStorage(void) -> void
    {
        if (this->store_) { this->storage_scope_ = static_cast<int>(this->store_->Scope(this->Surface(), this->ConfigBar())); this->storage_pet_ = this->store_->PetAware(this->Surface(), this->ConfigBar()); }
        static constexpr float columns[]{12, 6, 4, 3, 2, 1};
        const auto& bar = this->ConfigPresentation(); const auto found = std::find(std::begin(columns), std::end(columns), bar.columns);
        this->shape_ = bar.slots == 12 && found != std::end(columns) ? static_cast<int>(found - std::begin(columns)) : 6;
        this->trigger_controls_ = this->controller_settings_.enabled && this->controller_settings_.replace_native;
    }







    auto actionbars_module::Update(const uint64_t now) -> void
    {
        const auto elapsed = this->visibility_updated_ && now >= this->visibility_updated_ ? (now - this->visibility_updated_) / 1000.0f : 0.0f;
        this->visibility_updated_ = now; this->now_ = now;
        const auto* memory = this->core_ ? this->core_->GetMemoryManager() : nullptr;
        const auto* party = memory ? memory->GetParty() : nullptr;
        const auto* entity = memory ? memory->GetEntity() : nullptr;
        const auto index = party ? party->GetMemberTargetIndex(0) : 0;
        this->engaged_ = entity && index && index < entity->GetEntityMapSize() && entity->GetStatus(index) == 1;
        if (this->triggers_ != 0) this->last_trigger_ = now;
        const bool wanted = this->crossbar_visibility_ == 0 || this->triggers_ != 0 ||
            (this->crossbar_visibility_ == 2 && (this->engaged_ || (this->last_trigger_ && now - this->last_trigger_ < this->crossbar_delay_ * 1000)));
        const float target = wanted ? 1.0f : 0.0f, step = this->crossbar_fade_ > 0 ? elapsed / this->crossbar_fade_ : 1;
        this->crossbar_alpha_ += std::clamp(target - this->crossbar_alpha_, -step, step);
        if (this->store_ && this->store_revision_ != this->store_->Revision()) { this->store_revision_ = this->store_->Revision(); this->SyncStorage(); }
        if (this->dialog_ == dialog::picker && now - this->last_catalog_refresh_ > 2000) { this->RebuildResults(); this->last_catalog_refresh_ = now; }
        if (this->message_until_ && now > this->message_until_) { this->message_.clear(); this->message_until_ = 0; }
    }
    auto actionbars_module::TakeSettingsChanged(void) -> bool
    { const bool changed = this->settings_changed_; this->settings_changed_ = false; return changed; }
    auto actionbars_module::SuspendInput(void) -> void
    {
        this->FinishWindowGesture(false); this->window_focused_ = false;
        if (this->pointer_down_) this->discard_pointer_release_ = true;
        this->pointer_down_ = this->bar_pointer_down_ = this->dragging_ = this->browsing_ = this->navigation_focus_ = this->binding_capture_ = this->scrolling_ = false;
        this->library_popup_.reset();
        this->input_.Cancel(); this->editing_field_ = 0; this->dialog_ = dialog::none;
        this->CancelSetupControls();
        this->SuspendCrossbarInput();
        this->library_drag_.reset(); this->library_pressed_.reset(); this->placing_ = false;
        this->native_macro_modifiers_ = 0;
    }
    auto actionbars_module::SuspendCrossbarInput(void) -> bool
    {
        const bool active = this->active_bank_ != crossbar_bank::none || this->triggers_ != 0;
        this->active_bank_ = crossbar_bank::none; this->triggers_ = 0; this->held_slots_ = {};
        this->trigger_motion_ = {}; this->button_motion_ = {}; this->bank_motion_ = {};
        return active;
    }
    auto actionbars_module::PointerOwns(const float x, const float y) const -> bool
    {
        if (this->pointer_down_) return true;
        if (this->setup_choice_input_.Opened() && Contains(this->setup_choice_input_.Bounds(this->SetupViewport()), x, y)) return true;
        if (this->IsWindowDialog(this->dialog_)) return this->WindowPointerOwns(x, y) || (this->BarPointerAvailable(x, y) && this->HitTest(x, y).has_value());
        return this->dialog_ != dialog::none || this->HitTest(x, y).has_value();
    }
    auto actionbars_module::Notice(std::string value) -> void
    { this->message_ = std::move(value); this->message_until_ = this->now_ + 5000; }
    auto actionbars_module::Activate(const address& value) -> bool
    {
        if (!this->store_ || !this->executor_) return false;
        const bool accepted = this->executor_->Activate(this->core_, this->store_->Resolve(value.surface, value.bar, value.slot), this->now_);
        if (accepted) this->flashes_[static_cast<uint32_t>((value.surface == action_surface::crossbar ? 256 : 0) + value.bar * 16 + value.slot)] = this->now_ + 180;
        if (!accepted) this->Notice(this->executor_->Status());
        return accepted;
    }

    auto actionbar_module::Id(void) const -> const char* { return bar_ids[std::min(this->index_, action_bar_count)]; }
    auto actionbar_module::DisplayName(void) const -> const char* { return bar_names[std::min(this->index_, action_bar_count)]; }
}

namespace xiui
{
    auto actionbars_module::OpenEditor(const bool crossbar, const size_t bar, const size_t slot) -> void
    {
        this->library_popup_.reset();
        this->editor_options_ = this->editor_recast_ = this->editor_new_ = false;
        this->library_edit_ = false;
        this->CommitField(); this->binding_capture_ = false;
        this->selected_ = {crossbar ? action_surface::crossbar : action_surface::hotbar, std::min(bar, crossbar ? action_crossbar_banks - 1 : action_bar_count - 1), std::min(slot, crossbar ? action_crossbar_slots - 1 : action_bar_slots - 1)};
        this->view_ = crossbar ? action_bar_count : this->selected_.bar;
        this->selected_bank_ = crossbar ? static_cast<int>(this->selected_.bar) : this->selected_bank_;
        if (this->store_)
        {
            const auto& source = this->store_->Resolve(this->selected_.surface, this->selected_.bar, this->selected_.slot);
            this->draft_ = this->store_->ExpandMacro(source); this->draft_.macro_id = source.macro_id;
            this->library_draft_ = !source.macro_id.empty();
        }
        else this->draft_ = {};
        this->draft_pet_ = this->store_ && this->store_->PetAware(this->selected_.surface, this->selected_.bar) && !this->store_->Context().pet.empty();
        this->line_scroll_ = 0; this->dialog_ = dialog::editor; this->focused_control_ = command_kind;
        this->SetWindowFocused(true);
        this->message_.clear(); this->SyncStorage();
        if (this->draft_.Empty()) this->OpenPicker();
        if (this->sounds_) this->sounds_->MenuOpened(this->now_);
    }
    auto actionbars_module::OpenPalettes(const bool crossbar, const size_t bar) -> void
    {
        this->library_popup_.reset();
        this->CommitField(); this->binding_capture_ = false;
        this->selected_ = {crossbar ? action_surface::crossbar : action_surface::hotbar, std::min(bar, crossbar ? action_crossbar_banks - 1 : action_bar_count - 1), 0};
        this->view_ = crossbar ? action_bar_count : this->selected_.bar;
        if (crossbar) this->selected_bank_ = static_cast<int>(this->selected_.bar);
        const auto* active = this->store_ ? this->store_->ActivePalette(this->selected_.surface, this->selected_.bar) : nullptr;
        this->palette_id_ = active ? active->id : std::string{};
        this->dialog_ = dialog::palettes; this->palette_scroll_ = 0; this->focused_control_ = command_cancel;
        this->SetWindowFocused(true);
        this->message_.clear(); this->SyncStorage();
    }
    auto actionbars_module::OpenSetup(void) -> void
    {
        this->library_popup_.reset();
        this->CommitField(); this->binding_capture_ = false; this->dialog_ = dialog::setup; this->setup_step_ = 0;
        this->SetWindowFocused(true);
        this->setup_backend_ = this->controller_status_.backend == controller_backend::direct_input ? 1 : 0;
        this->setup_preset_ = -1; this->focused_control_ = setup_enable; this->message_.clear();
    }
    auto actionbars_module::OpenImport(void) -> void
    {
        this->library_popup_.reset();
        this->CommitField(); this->binding_capture_ = false;
        this->selected_ = {this->Surface(), this->ConfigBar(), 0};
        this->dialog_ = dialog::import; this->focused_control_ = field_path; this->import_preview_ = {}; this->message_.clear();
        this->SetWindowFocused(true);
    }
    auto actionbars_module::OpenPicker(void) -> void
    {
        this->library_popup_.reset();
        this->CommitField(); this->dialog_ = dialog::picker; this->query_.clear(); this->category_ = 0;
        this->SetWindowFocused(true);
        this->result_scroll_ = this->focused_result_ = 0; this->focused_control_ = field_query;
        this->RebuildResults();
    }
    auto actionbars_module::RebuildResults(void) -> void
    {
        this->results_.clear();
        if (!this->executor_) return;
        const auto catalog = this->executor_->Catalog(this->core_); const auto query = Lower(Trim(this->query_));
        for (size_t index = 0; index < catalog.size(); ++index)
        {
            const auto& entry = catalog[index];
            if (this->category_ > 0 && category_kinds[this->category_] != entry.kind) continue;
            if (this->only_learned_ && !entry.learned && entry.kind != action_kind::item && entry.kind != action_kind::equip) continue;
            if (!query.empty() && Lower(entry.name).find(query) == std::string::npos && std::to_string(entry.id) != query) continue;
            action_slot action; action.kind = entry.kind; action.id = entry.id; action.name = entry.name;
            action.icon = entry.icon;
            this->PrepareAction(action);
            this->results_.push_back(std::move(action));
        }
        if (this->store_)
            for (const auto& macro : this->store_->Macros())
            {
                if (!query.empty() && Lower(macro.name).find(query) == std::string::npos) continue;
                action_slot action = macro.definition;
                if (action.Empty()) { action.kind = action_kind::macro; action.name = macro.name; action.icon = macro.icon; action.lines = macro.lines; }
                if (this->category_ != 0 && this->category_ != 7 && category_kinds[this->category_] != action.kind) continue;
                action.macro_id = macro.id; action.inherit_appearance = true;
                this->results_.push_back(std::move(action));
            }
        this->focused_result_ = std::min(this->focused_result_, this->results_.empty() ? 0 : this->results_.size() - 1);
        this->SetScroll(static_cast<int>(this->result_scroll_));
    }
    auto actionbars_module::EditLibraryEntry(const size_t index) -> bool
    {
        if (!this->store_ || index >= this->results_.size() || this->results_[index].macro_id.empty()) return false;
        this->CommitField(); const auto& source = this->results_[index];
        this->draft_ = this->store_->ExpandMacro(source); this->draft_.macro_id = source.macro_id;
        this->library_edit_ = this->library_draft_ = true; this->line_scroll_ = 0; this->dialog_ = dialog::editor;
        this->editor_options_ = this->editor_recast_ = this->editor_new_ = false;
        this->focused_control_ = command_kind; this->message_.clear(); this->SetWindowFocused(true); return true;
    }
    auto actionbars_module::ChooseResult(const size_t index) -> void
    {
        if (index >= this->results_.size()) return;
        this->draft_ = this->results_[index];
        this->input_.Cancel(); this->editing_field_ = 0;
        if (this->navigation_focus_) this->placing_ = true;
        else if (this->AssignAction(this->selected_, this->draft_)) this->CloseDialog();
    }
    auto actionbars_module::NewDraft(const action_kind kind) -> void
    {
        this->editor_options_ = this->editor_recast_ = false; this->editor_new_ = true;
        this->library_edit_ = false; this->library_draft_ = kind == action_kind::macro;
        this->draft_ = {}; this->draft_.kind = kind; this->draft_.target = "<t>";
        if (kind == action_kind::macro || kind == action_kind::command) this->draft_.lines = {""};
        if (kind == action_kind::equip) this->draft_.equipment_slot = "main";
        this->line_scroll_ = 0; this->dialog_ = dialog::editor; this->focused_control_ = command_kind;
        this->SetWindowFocused(true);
    }
    auto actionbars_module::SaveDraft(void) -> void
    {
        this->CommitField();
        this->draft_.name = Trim(this->draft_.name); this->draft_.label = Trim(this->draft_.label); this->draft_.target = Trim(this->draft_.target);
        while (this->draft_.lines.size() > 1 && Trim(this->draft_.lines.back()).empty()) this->draft_.lines.pop_back();
        const auto error = ValidateAction(this->draft_);
        if (!error.empty()) { this->Notice(error); return; }
        if (this->store_ && (this->library_draft_ || !this->draft_.macro_id.empty()))
        {
            auto definition = this->draft_; definition.macro_id.clear();
            const auto name = !this->draft_.label.empty() ? this->draft_.label : this->draft_.name.empty() ? std::string{ActionKindName(this->draft_.kind)} : this->draft_.name;
            action_macro macro{this->draft_.macro_id, name, this->draft_.icon, this->draft_.lines, definition};
            if (macro.id.empty()) { this->draft_.macro_id = this->store_->ImportMacro(std::move(macro)); this->draft_.inherit_appearance = true; }
            else if (!this->store_->SaveMacro(macro)) { this->Notice("The shared action could not be saved."); return; }
            if (this->draft_.macro_id.empty()) { this->Notice("The shared library is full or the action is invalid."); return; }
        }
        if (this->library_edit_) { this->library_edit_ = false; this->OpenPicker(); return; }
        if (this->store_ && this->store_->Assign(this->selected_.surface, this->selected_.bar, this->selected_.slot, this->draft_, this->draft_pet_))
        {
            this->Presentation(this->selected_).state.enabled = true; this->settings_changed_ = true; this->CloseDialog();
        }
        else this->Notice("The action could not be saved in this context.");
    }
    auto actionbars_module::CloseDialog(void) -> void
    {
        this->FinishWindowGesture(false); this->window_focused_ = false;
        this->input_.Cancel(); this->editing_field_ = 0; this->binding_capture_ = false;
        this->CancelSetupControls();
        this->dialog_ = dialog::none; this->library_edit_ = false; this->navigation_focus_ = this->browsing_;
        if (this->pointer_down_) this->discard_pointer_release_ = true;
        this->pointer_down_ = this->bar_pointer_down_ = this->dragging_ = this->scrolling_ = false; this->pressed_control_ = 0;
        this->library_drag_.reset(); this->library_pressed_.reset(); this->placing_ = false;
        if (this->library_popup_)
        {
            this->SwapLibraryContext(*this->library_popup_); this->library_popup_.reset();
            this->dialog_ = dialog::picker; this->window_focused_ = true;
        }
    }
    auto actionbars_module::FieldValue(const int id) const -> std::string
    {
        if (id == field_query) return this->query_;
        if (id == field_icon_query) return this->icon_query_;
        if (id == field_name) return this->dialog_ == dialog::name ? this->name_value_ : this->draft_.name;
        if (id == field_label) return this->draft_.label;
        if (id == field_target) return this->draft_.target;
        if (id == field_icon) return this->draft_.icon;
        if (id == field_recast) return this->draft_.recast_name;
        if (id == field_path) return this->import_path_;
        if (id >= line_first && id < line_first + 32) { const auto line = static_cast<size_t>(id - line_first); return line < this->draft_.lines.size() ? this->draft_.lines[line] : std::string{}; }
        return {};
    }
    auto actionbars_module::SetFieldValue(const int id, std::string value) -> void
    {
        if (id == field_query) { this->query_ = std::move(value); this->result_scroll_ = this->focused_result_ = 0; this->RebuildResults(); }
        else if (id == field_icon_query) { this->icon_query_ = std::move(value); this->RebuildIconResults(); }
        else if (id == field_name) { if (this->dialog_ == dialog::name) this->name_value_ = std::move(value); else if (value != this->draft_.name) { this->draft_.name = std::move(value); this->draft_.id = 0; } }
        else if (id == field_label) this->draft_.label = std::move(value);
        else if (id == field_target) this->draft_.target = std::move(value);
        else if (id == field_icon) this->draft_.icon = std::move(value);
        else if (id == field_recast) { this->draft_.recast_name = Trim(std::move(value)); this->draft_.recast_id = Numeric(this->draft_.recast_name); if (this->draft_.recast_id) this->draft_.recast_name.clear(); }
        else if (id == field_path) { this->import_path_ = std::move(value); this->import_preview_ = {}; }
        else if (id >= line_first && id < line_first + 32)
        { const auto line = static_cast<size_t>(id - line_first); if (line >= this->draft_.lines.size()) this->draft_.lines.resize(line + 1); this->draft_.lines[line] = std::move(value); }
    }
    auto actionbars_module::BeginField(const int id) -> void
    {
        this->CommitField(); this->editing_field_ = id;
        if (id == field_icon_query) this->focused_control_ = id;
        this->input_.Open(this->FieldValue(id), id == field_path ? 1024 : id >= line_first ? 240 : 128);
    }
    auto actionbars_module::CommitField(void) -> void
    {
        if (!this->input_.Focused()) return;
        this->SetFieldValue(this->editing_field_, this->input_.Editor().Text()); this->input_.Cancel(); this->editing_field_ = 0;
    }
    auto actionbars_module::NamePalette(const int action) -> void
    {
        this->name_action_ = action; this->name_return_ = this->dialog_; this->name_value_ = action == command_new ? "New palette" : "";
        if (this->store_) for (const auto& palette : this->store_->AllPalettes()) if (palette.id == this->palette_id_) this->name_value_ = palette.name + (action == command_copy ? " copy" : "");
        this->dialog_ = dialog::name; this->focused_control_ = field_name; this->BeginField(field_name);
    }
    auto actionbars_module::PreviewImport(void) -> void
    {
        this->CommitField(); this->import_preview_ = {};
        std::ifstream stream(this->import_path_, std::ios::binary | std::ios::ate);
        if (!stream) { this->Notice("The local file could not be opened."); return; }
        const auto size = stream.tellg();
        if (size <= 0 || size > 16 * 1024 * 1024) { this->Notice("Choose a non-empty file smaller than 16 MB."); return; }
        std::string text(static_cast<size_t>(size), '\0'); stream.seekg(0); stream.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (!stream) { this->Notice("The local file could not be read completely."); return; }
        this->import_preview_ = PreviewActionImport(text, this->import_path_);
        if (!this->import_preview_.Valid()) this->Notice(this->import_preview_.error.empty() ? "No supported action assignments were found." : this->import_preview_.error);
    }
    auto actionbars_module::Mapping(void) -> controller_crossbar_mapping&
    { return this->setup_backend_ == 0 ? this->controller_settings_.xinput : this->controller_settings_.direct_input; }
    auto actionbars_module::Mapping(void) const -> const controller_crossbar_mapping&
    { return this->setup_backend_ == 0 ? this->controller_settings_.xinput : this->controller_settings_.direct_input; }

    auto actionbars_module::ActivateControl(const int id, const int adjustment) -> void
    {
        const auto found = this->Controls();
        const auto control = std::find_if(found.begin(), found.end(), [&](const auto& value) { return value.id == id; });
        if (control == found.end() || !control->enabled) return;
        if (this->HandleWindowControl(id)) return;
        if (id >= icon_source_first && id < icon_source_first + 4)
        { this->CommitField(); this->icon_source_ = id - icon_source_first; this->RebuildIconResults(); this->focused_control_ = id; return; }
        if (control->kind == control_kind::field) { this->BeginField(id); return; }
        if (control->kind == control_kind::dropdown) { this->OpenSetupChoice(*control); return; }
        if (control->kind == control_kind::slider) { this->setup_slider_adjust_ = this->setup_slider_adjust_ ? 0 : id; return; }
        this->CommitField();
        if (this->sounds_) this->sounds_->Select(this->now_);
        if (this->ActivateSetupControl(id, adjustment)) return;
        if (this->ActivateKeybindControl(id, adjustment)) return;
        if (this->dialog_ == dialog::icons && id >= icon_first) { this->SelectIcon(static_cast<size_t>(id - icon_first)); return; }
        if (id >= category_first && id < category_first + static_cast<int>(std::size(categories)))
        { this->category_ = id - category_first; this->result_scroll_ = this->focused_result_ = 0; this->RebuildResults(); return; }
        if (this->dialog_ == dialog::picker && id >= result_first && static_cast<size_t>(id - result_first) < this->results_.size())
        { this->ChooseResult(static_cast<size_t>(id - result_first)); return; }
        if (id >= palette_first && id < palette_first + 512)
        {
            const auto palettes = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar) : std::vector<const action_palette*>{};
            const auto index = static_cast<size_t>(id - palette_first);
            if (index < palettes.size())
            {
                this->palette_id_ = palettes[index]->id;
                if (this->dialog_ == dialog::palette_dropdown) { this->store_->Select(this->selected_.surface, this->palette_id_, this->selected_.bar); this->CloseDialog(); }
            }
            return;
        }

        switch (id)
        {
        case command_editor_options: this->editor_options_ = !this->editor_options_; this->focused_control_ = command_editor_options; break;
        case command_editor_recast: this->editor_recast_ = !this->editor_recast_; this->line_scroll_ = 0; this->focused_control_ = command_editor_recast; break;
        case command_icon_picker: this->OpenIconPicker(); break;
        case command_icon_back: this->CloseIconPicker(); break;
        case command_icon_default: this->draft_.icon.clear(); this->CloseIconPicker(); break;
        case command_cancel:
            if (this->library_edit_ && this->dialog_ == dialog::editor) { this->library_edit_ = false; this->OpenPicker(); break; }
            if (this->dialog_ == dialog::delete_macro) { this->dialog_ = dialog::editor; this->focused_control_ = command_macro_delete; }
            else if (this->dialog_ == dialog::name || this->dialog_ == dialog::delete_palette) { this->dialog_ = dialog::palettes; this->focused_control_ = command_cancel; }
            else this->CloseDialog();
            break;
        case command_save:
            if (this->dialog_ == dialog::name && this->store_)
            {
                const auto name = Trim(this->name_value_); std::string newId;
                if (name.empty()) { this->Notice("Choose a palette name."); break; }
                if (this->name_action_ == command_new) newId = this->store_->Create(name, this->selected_.surface, this->selected_.bar);
                else if (this->name_action_ == command_copy) newId = this->store_->Copy(this->palette_id_, name);
                else if (this->store_->Rename(this->palette_id_, name)) newId = this->palette_id_;
                if (newId.empty()) { this->Notice("The palette name could not be saved."); break; }
                this->palette_id_ = newId; this->store_->Select(this->selected_.surface, newId, this->selected_.bar); this->dialog_ = dialog::palettes; this->focused_control_ = command_select;
            }
            else this->SaveDraft();
            break;
        case command_picker:
            if (this->dialog_ == dialog::slot_menu) this->OpenLibrary(this->selected_.surface == action_surface::crossbar, this->selected_.bar, this->selected_.slot);
            else this->OpenPicker();
            break;
        case menu_library: this->OpenLibrary(this->selected_.surface == action_surface::crossbar, this->selected_.bar, this->selected_.slot); break;
        case menu_edit: this->OpenEditor(this->selected_.surface == action_surface::crossbar, this->selected_.bar, this->selected_.slot); break;
        case menu_palettes: this->OpenPalettes(this->selected_.surface == action_surface::crossbar, this->selected_.bar); break;
        case menu_keybinds: this->OpenKeybinds(this->selected_.surface == action_surface::crossbar, this->selected_.bar); break;
        case menu_lock: this->Presentation(this->selected_).locked = !this->Presentation(this->selected_).locked; this->settings_changed_ = true; this->CloseDialog(); break;
        case menu_shape: this->CycleShape(this->selected_.bar); this->CloseDialog(); break;
        case menu_settings: this->configuration_request_ = this->view_; this->CloseDialog(); break;
        case new_macro: this->NewDraft(action_kind::macro); break;
        case new_command: this->NewDraft(action_kind::command); break;
        case new_native: this->NewDraft(action_kind::native_macro); break;
        case command_clear:
            if (this->store_) { this->store_->Assign(this->selected_.surface, this->selected_.bar, this->selected_.slot, {}, this->draft_pet_); this->CloseDialog(); }
            break;
        case command_binding: this->binding_capture_ = true; this->Notice("Press a key combination. Escape cancels."); break;
        case command_unbind: this->Presentation(this->selected_).keys[this->selected_.slot] = {}; this->settings_changed_ = true; break;
        case command_new:
            if (this->dialog_ == dialog::picker) this->NewDraft(this->category_ == 7 ? action_kind::macro : this->category_ > 0 ? category_kinds[this->category_] : action_kind::command);
            else this->NamePalette(id);
            break;
        case command_copy: case command_rename: this->NamePalette(id); break;
        case command_delete:
            if (this->dialog_ == dialog::delete_macro)
            {
                if (this->store_ && this->store_->DeleteMacro(this->draft_.macro_id)) { this->draft_.macro_id.clear(); this->library_draft_ = false; if (this->library_edit_) { this->library_edit_ = false; this->OpenPicker(); } else { this->dialog_ = dialog::editor; this->focused_control_ = command_save; } }
                else this->Notice("The macro could not be removed.");
            }
            else if (this->dialog_ == dialog::delete_palette)
            {
                if (this->store_ && this->store_->Delete(this->palette_id_)) this->OpenPalettes(this->selected_.surface == action_surface::crossbar, this->selected_.bar);
                else this->Notice("Keep at least one palette in this context.");
            }
            else { this->dialog_ = dialog::delete_palette; this->focused_control_ = command_cancel; }
            break;
        case command_up: case command_down: if (this->store_) this->store_->Move(this->palette_id_, id == command_up ? -1 : 1); break;
        case command_select:
            if (this->dialog_ == dialog::palette_dropdown) this->OpenPalettes(this->selected_.surface == action_surface::crossbar, this->selected_.bar);
            else if (this->store_ && this->store_->Select(this->selected_.surface, this->palette_id_, this->selected_.bar)) this->CloseDialog();
            break;
        case command_previous: case command_next:
            this->line_scroll_ = static_cast<size_t>(std::clamp(static_cast<int>(this->line_scroll_) + (id == command_previous ? -static_cast<int>(this->EditorLayout().rows) : static_cast<int>(this->EditorLayout().rows)),
                0, std::max(0, static_cast<int>(this->draft_.lines.size()) - static_cast<int>(this->EditorLayout().rows))));
            this->focused_control_ = line_first + static_cast<int>(this->line_scroll_);
            break;
        case command_more_lines: this->draft_.lines.push_back(""); this->RevealEditorLine(this->draft_.lines.size() - 1); this->BeginField(this->focused_control_); break;
        case command_remove_line: this->draft_.lines.pop_back(); this->line_scroll_ = std::min(this->line_scroll_, this->draft_.lines.size() > this->EditorLayout().rows ? this->draft_.lines.size() - this->EditorLayout().rows : 0); this->RevealEditorLine(this->draft_.lines.size() - 1); break;
        case command_preview: this->PreviewImport(); break;
        case command_import:
            if (this->dialog_ == dialog::palettes) this->OpenImport();
            else if (this->store_ && this->import_preview_.Valid())
            {
                const auto count = CommitActionImport(*this->store_, this->import_preview_, this->selected_.surface, this->selected_.bar);
                if (count > 0) this->OpenPalettes(this->selected_.surface == action_surface::crossbar, this->selected_.bar);
                else this->Notice("The import did not create a palette.");
            }
            break;
        case command_learned: case command_all_actions:
            this->only_learned_ = id == command_learned; this->result_scroll_ = this->focused_result_ = 0; this->RebuildResults(); break;
        case command_library:
            if (!this->library_edit_) { if (!this->draft_.macro_id.empty()) { this->draft_.macro_id.clear(); this->library_draft_ = false; } else this->library_draft_ = !this->library_draft_; }
            break;
        case command_available: this->draft_.always_available = !this->draft_.always_available; break;
        case command_recast:
        {
            constexpr action_kind kinds[]{action_kind::none, action_kind::none, action_kind::spell, action_kind::ability, action_kind::pet, action_kind::item};
            auto index = this->draft_.recast_disabled ? ptrdiff_t{1} : this->draft_.recast_kind == action_kind::none ? ptrdiff_t{0} : std::find(std::begin(kinds) + 2, std::end(kinds), this->draft_.recast_kind) - std::begin(kinds);
            index = (index + adjustment + 6) % 6;
            this->draft_.recast_kind = kinds[index]; this->draft_.recast_disabled = index == 1; this->draft_.recast_id = 0; break;
        }
        case command_pet: this->draft_pet_ = !this->draft_pet_; break;
        case command_macro_copy: this->draft_.macro_id.clear(); this->library_draft_ = true; this->draft_.name += " copy"; this->focused_control_ = field_name; break;
        case command_macro_delete: this->dialog_ = dialog::delete_macro; this->focused_control_ = command_cancel; break;
        case command_kind:
            this->draft_.kind = static_cast<action_kind>((std::max(1, static_cast<int>(this->draft_.kind)) - 1 + adjustment + 9) % 9 + 1);
            this->draft_.id = 0;
            if ((this->draft_.kind == action_kind::macro || this->draft_.kind == action_kind::command) && this->draft_.lines.empty()) this->draft_.lines.push_back("");
            break;
        case command_target:
        {
            const auto current = std::find(std::begin(targets), std::end(targets), this->draft_.target);
            const auto count = static_cast<int>(std::size(targets));
            const auto index = current == std::end(targets) ? 0 : static_cast<int>(current - std::begin(targets));
            this->draft_.target = targets[(index + adjustment + count) % count]; break;
        }
        case command_equipment:
        {
            auto equipment = std::find(std::begin(equipment_slots), std::end(equipment_slots), this->draft_.equipment_slot);
            const int current = equipment == std::end(equipment_slots) ? 0 : static_cast<int>(equipment - std::begin(equipment_slots));
            this->draft_.equipment_slot = equipment_slots[(current + adjustment + 16) % 16]; break;
        }
        case command_native_book: this->draft_.native.book = static_cast<uint8_t>((this->draft_.native.book - 1 + adjustment + 40) % 40 + 1); break;
        case command_native_set: this->draft_.native.set = static_cast<uint8_t>((this->draft_.native.set - 1 + adjustment + 10) % 10 + 1); break;
        case command_native_slot: this->draft_.native.slot = static_cast<uint8_t>((this->draft_.native.slot - 1 + adjustment + 10) % 10 + 1); break;
        case command_native_alt: this->draft_.native.alt = !this->draft_.native.alt; break;
        default: break;
        }
    }
}

namespace xiui
{
    auto actionbars_module::DialogBounds(void) const -> ui::rect
    {
        const auto viewport = this->viewport_.width > 0 ? this->viewport_ : ui::rect{0, 0, 1280, 720};
        if (this->dialog_ == dialog::palette_dropdown || this->dialog_ == dialog::slot_menu || this->dialog_ == dialog::bar_menu)
        {
            const auto index = this->selected_.surface == action_surface::crossbar ? action_bar_count : this->selected_.bar;
            const auto bounds = this->dialog_ == dialog::slot_menu ? this->SlotBounds(this->selected_) : this->dialog_ == dialog::bar_menu ? this->MenuBounds(index) : this->PaletteBounds(index);
            const auto count = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar).size() : 0;
            const bool popup = this->dialog_ != dialog::palette_dropdown;
            const auto rows = this->dialog_ == dialog::bar_menu ? (index == action_bar_count ? 5 : 6) : index == action_bar_count ? 3 : 5;
            const float width = std::min(256.0f, viewport.width - 16), height = 16 + static_cast<float>(popup ? rows : std::min(count, visible_results) + 1) * 30;
            float y = bounds.y + bounds.height + 6;
            if (y + height > viewport.y + viewport.height - 8) y = bounds.y - height - 6;
            return {std::clamp(bounds.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8)), std::clamp(y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8)), width, height};
        }
        if (this->IsWindowDialog(this->dialog_)) return this->WindowBounds();
        const bool compact = this->dialog_ == dialog::name || this->dialog_ == dialog::delete_palette || this->dialog_ == dialog::delete_macro;
        const float width = std::min(compact ? 440.0f : 720.0f, std::max(280.0f, viewport.width - 32));
        const float height = std::min(compact ? 220.0f : 548.0f, std::max(220.0f, viewport.height - 32));
        if (compact)
        {
            const auto parent = this->WindowBounds();
            return {std::clamp(parent.x + (parent.width - width) / 2, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8)),
                std::clamp(parent.y + (parent.height - height) / 2, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8)), width, height};
        }
        return {viewport.x + (viewport.width - width) / 2, viewport.y + (viewport.height - height) / 2, width, height};
    }

    auto actionbars_module::Controls(void) const -> std::vector<control>
    {
        std::vector<control> result;
        this->AddWindowControls(result);
        const auto bounds = this->DialogBounds(); const float left = bounds.x + 16, right = bounds.x + bounds.width - 16, top = bounds.y + 52;
        const auto button = [&](int id, float x, float y, float width, const std::string& text, bool enabled = true, bool selected = false, bool danger = false) {
            result.push_back({id, {x, y, width, 28}, text, {}, control_kind::button, enabled, selected, danger});
        };
        const auto field = [&](int id, float y, const std::string& caption, const std::string& value) {
            result.push_back({id, {left + 124, y, right - left - 124, 28}, value, caption, control_kind::field});
        };
        const auto footer = [&]() { button(command_cancel, right - 208, bounds.y + bounds.height - 44, 96, "Cancel"); };
        if (this->dialog_ == dialog::none) return result;
        if (this->dialog_ == dialog::keybinds) return this->KeybindControls(bounds);
        if (this->dialog_ == dialog::icons)
        {
            auto icons = this->IconControls(); result.insert(result.end(), icons.begin(), icons.end()); return result;
        }
        if (this->dialog_ == dialog::slot_menu || this->dialog_ == dialog::bar_menu)
        {
            float y = bounds.y + 8;
            const auto entry = [&](int id, const std::string& text, bool enabled = true, bool danger = false) { button(id, bounds.x + 8, y, bounds.width - 16, text, enabled, false, danger); y += 30; };
            if (this->dialog_ == dialog::slot_menu)
            {
                entry(command_picker, "Change action..."); entry(menu_edit, "Edit action...");
                if (this->selected_.surface == action_surface::hotbar)
                {
                    entry(command_binding, this->binding_capture_ ? "Press a key..." : "Key: " + this->BindingLabel(this->Binding(this->selected_)));
                    entry(command_unbind, "Remove key binding", this->Binding(this->selected_).key != 0);
                }
                entry(command_clear, "Clear slot", true, true);
            }
            else
            {
                entry(menu_library, "Actions & macros..."); entry(menu_palettes, this->selected_.surface == action_surface::hotbar ? "Action sets (all hotbars)..." : "Crossbar action sets...");
                entry(menu_keybinds, "Keybinds...");
                entry(menu_lock, this->Presentation(this->selected_).locked ? "Unlock slot positions" : "Lock slot positions");
                if (this->selected_.surface == action_surface::hotbar) entry(menu_shape, "Change bar shape");
                entry(menu_settings, "Bar settings...");
            }
            return result;
        }
        if (this->dialog_ == dialog::palette_dropdown)
        {
            const auto palettes = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar) : std::vector<const action_palette*>{};
            const auto* active = this->store_ ? this->store_->ActivePalette(this->selected_.surface, this->selected_.bar) : nullptr;
            for (size_t row = 0; row < visible_results && row + this->palette_scroll_ < palettes.size(); ++row)
            {
                const auto* palette = palettes[row + this->palette_scroll_];
                result.push_back({palette_first + static_cast<int>(row + this->palette_scroll_), {bounds.x + 8, bounds.y + 8 + static_cast<float>(row) * 30, bounds.width - 16, 28}, palette->name, {}, control_kind::list, true, active && palette->id == active->id});
            }
            button(command_select, bounds.x + 8, bounds.y + bounds.height - 36, bounds.width - 16, "Manage palettes...");
            return result;
        }
        if (this->dialog_ == dialog::picker)
        {
            const auto layout = this->LibraryLayout();
            result.push_back({field_query, layout.search, this->query_, {}, control_kind::field});
            for (size_t category = 0; category < std::size(categories); ++category)
                result.push_back({category_first + static_cast<int>(category), {layout.sidebar.x, layout.sidebar.y + static_cast<float>(category) * layout.nav_pitch, layout.sidebar.width, layout.nav_pitch - 2}, categories[category], {}, control_kind::list, true, static_cast<int>(category) == this->category_});
            result.push_back({command_learned, layout.your_actions, "Your actions", "Learned for your current jobs; items you own.", control_kind::button, true, this->only_learned_});
            result.push_back({command_all_actions, layout.all_actions, "All actions", "Include actions you have not learned and items you do not own.", control_kind::button, true, !this->only_learned_});
            const auto first = this->result_scroll_ * layout.columns;
            for (size_t offset = 0; offset < layout.rows * layout.columns && first + offset < this->results_.size(); ++offset)
            {
                const auto index = first + offset;
                const auto& entry = this->results_[index];
                const ui::rect cell{layout.list.x + static_cast<float>(offset % layout.columns) * layout.column_pitch,
                    layout.list.y + static_cast<float>(offset / layout.columns) * layout.row_pitch, layout.column_width, layout.row_height};
                result.push_back({result_first + static_cast<int>(index), cell, entry.label.empty() ? entry.name : entry.label, this->category_ == 0 ? ActionKindName(entry.kind) : "", control_kind::list, true, index == this->focused_result_});
            }
            result.push_back({new_macro, layout.create[0], "New macro", "Create a sequence of commands, including waits.", control_kind::button});
            result.push_back({new_command, layout.create[1], "Single command", "Run one command, such as /ws \"Combo\" <t>.", control_kind::button});
            result.push_back({new_native, layout.create[2], "Game macro", "Run a macro from FFXI's existing book, set and Ctrl/Alt row.", control_kind::button});
            result.push_back({command_cancel, layout.done, "Done", {}, control_kind::button});
            return result;
        }
        if (this->dialog_ == dialog::editor)
        {
            auto editor = this->EditorControls(); result.insert(result.end(), editor.begin(), editor.end()); return result;
        }
        if (this->dialog_ == dialog::palettes)
        {
            const auto palettes = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar) : std::vector<const action_palette*>{};
            const float listWidth = std::max(140.0f, (right - left) * 0.55f);
            for (size_t row = 0; row < this->PaletteRows() && row + this->palette_scroll_ < palettes.size(); ++row)
            {
                const auto* palette = palettes[row + this->palette_scroll_];
                result.push_back({palette_first + static_cast<int>(row + this->palette_scroll_), {left, top + static_cast<float>(row) * 30, listWidth, 28}, palette->name, {}, control_kind::list, true, palette->id == this->palette_id_});
            }
            const float x = left + listWidth + 16, width = right - x;
            button(command_select, x, top, width, "Use palette", !this->palette_id_.empty());
            button(command_new, x, top + 38, width, "New palette");
            button(command_copy, x, top + 76, width, "Duplicate", !this->palette_id_.empty());
            button(command_rename, x, top + 114, width, "Rename", !this->palette_id_.empty());
            button(command_up, x, top + 152, width, "Move up", palettes.size() > 1);
            button(command_down, x, top + 190, width, "Move down", palettes.size() > 1);
            button(command_delete, x, top + 242, width, "Delete...", palettes.size() > 1, false, true);
            button(command_import, x, top + 294, width, "Import...");
            button(command_cancel, right - 104, bounds.y + bounds.height - 44, 104, "Done");
            return result;
        }
        if (this->dialog_ == dialog::name)
        {
            field(field_name, top + 20, "Palette name", this->name_value_);
            footer(); button(command_save, right - 104, bounds.y + bounds.height - 44, 104, this->name_action_ == command_copy ? "Duplicate" : this->name_action_ == command_rename ? "Rename" : "Create", !Trim(this->name_value_).empty());
            return result;
        }
        if (this->dialog_ == dialog::delete_palette || this->dialog_ == dialog::delete_macro)
        {
            footer(); button(command_delete, right - 104, bounds.y + bounds.height - 44, 104, "Delete", true, false, true);
            return result;
        }
        if (this->dialog_ == dialog::import)
        {
            field(field_path, top + 10, "Local file", this->import_path_);
            button(command_preview, left, top + 52, 144, "Read and preview");
            button(command_cancel, right - 256, bounds.y + bounds.height - 44, 104, "Cancel"); button(command_import, right - 144, bounds.y + bounds.height - 44, 144, "Import as new", this->import_preview_.Valid());
            return result;
        }
        if (this->dialog_ == dialog::setup)
        {
            auto setup = this->SetupControls(bounds); result.insert(result.end(), setup.begin(), setup.end()); return result;
        }
        return result;
    }
}

namespace xiui
{
    auto actionbars_module::Scrollbar(void) const -> ui::config_scrollbar
    {
        const auto bounds = this->DialogBounds();
        if (this->dialog_ == dialog::icons)
        {
            const auto layout = this->IconPickerLayout();
            const auto rows = (this->icon_results_.size() + layout.columns - 1) / layout.columns;
            return {layout.scrollbar, static_cast<int>(rows), static_cast<int>(layout.rows), static_cast<int>(this->icon_scroll_)};
        }
        if (this->dialog_ == dialog::keybinds)
        {
            const bool crossbar = this->selected_.surface == action_surface::crossbar;
            return {{bounds.x + bounds.width - 20, bounds.y + (crossbar ? 150 : 116), 8, static_cast<float>(this->KeybindRows()) * 32 - 4},
                static_cast<int>(crossbar ? action_crossbar_slots : action_bar_slots), static_cast<int>(this->KeybindRows()), static_cast<int>(this->palette_scroll_)};
        }
        if (this->dialog_ == dialog::picker)
        {
            const auto layout = this->LibraryLayout();
            const auto rows = (this->results_.size() + layout.columns - 1) / layout.columns;
            return {layout.scrollbar, static_cast<int>(rows), static_cast<int>(layout.rows), static_cast<int>(this->result_scroll_)};
        }
        if (this->dialog_ == dialog::palettes || this->dialog_ == dialog::palette_dropdown)
        {
            const auto count = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar).size() : 0;
            const bool dropdown = this->dialog_ == dialog::palette_dropdown;
            return {{dropdown ? bounds.x + bounds.width - 14 : bounds.x + 8 + (bounds.width - 32) * 0.55f,
                bounds.y + (dropdown ? 8 : 52), 8, dropdown ? bounds.height - 48 : static_cast<float>(this->PaletteRows()) * 30 - 2}, static_cast<int>(count), dropdown ? static_cast<int>(visible_results) : static_cast<int>(this->PaletteRows()), static_cast<int>(this->palette_scroll_)};
        }
        return {};
    }
    auto actionbars_module::SetScroll(const int value) -> void
    {
        if (this->dialog_ == dialog::icons) { this->ScrollIcons(value); return; }
        const auto scrollbar = this->Scrollbar();
        const auto position = static_cast<size_t>(std::clamp(value, 0, scrollbar.Maximum()));
        if (this->dialog_ == dialog::picker)
        {
            this->result_scroll_ = position;
            if (this->focused_control_ >= result_first && !this->results_.empty())
            {
                const auto layout = this->LibraryLayout();
                const auto index = std::min(static_cast<size_t>(this->focused_control_ - result_first), this->results_.size() - 1);
                const auto row = std::clamp(index / layout.columns, position, position + layout.rows - 1);
                this->focused_result_ = std::min(row * layout.columns + index % layout.columns, this->results_.size() - 1);
                this->focused_control_ = result_first + static_cast<int>(this->focused_result_);
            }
        }
        else this->palette_scroll_ = position;
    }
    auto actionbars_module::MoveControl(const int dx, const int dy) -> void
    {
        this->CommitField();
        if (this->MoveKeybindControl(dx, dy)) return;
        if (this->MoveIconControl(dx, dy)) return;
        const bool picker = this->dialog_ == dialog::picker;
        const auto focusResult = [&]() {
            if (!picker || this->focused_control_ < result_first || static_cast<size_t>(this->focused_control_ - result_first) >= this->results_.size()) return;
            const auto layout = this->LibraryLayout();
            this->focused_result_ = static_cast<size_t>(this->focused_control_ - result_first);
            const auto row = this->focused_result_ / layout.columns;
            if (row < this->result_scroll_) this->SetScroll(static_cast<int>(row));
            else if (row >= this->result_scroll_ + layout.rows) this->SetScroll(static_cast<int>(row - layout.rows + 1));
        };
        const bool resultFocused = picker && this->focused_control_ >= result_first && static_cast<size_t>(this->focused_control_ - result_first) < this->results_.size();
        if (resultFocused)
        {
            const auto layout = this->LibraryLayout();
            const int current = this->focused_control_ - result_first, columns = static_cast<int>(layout.columns);
            const int next = current + (dy ? dy * columns : dx);
            if (next >= 0 && static_cast<size_t>(next) < this->results_.size() && (dy || next / columns == current / columns))
            {
                this->focused_control_ = result_first + next; focusResult(); return;
            }
            focusResult();
        }
        if ((this->dialog_ == dialog::palettes || this->dialog_ == dialog::palette_dropdown) && this->focused_control_ >= palette_first && this->focused_control_ < palette_first + 512 && dy != 0)
        {
            const auto count = this->store_ ? this->store_->Palettes(this->selected_.surface, this->selected_.bar).size() : 0;
            const auto current = this->focused_control_ - palette_first;
            const auto next = std::clamp(current + dy, 0, std::max(0, static_cast<int>(count) - 1));
            this->focused_control_ = palette_first + next;
            const auto page = this->dialog_ == dialog::palettes ? static_cast<int>(this->PaletteRows()) : static_cast<int>(visible_results);
            if (static_cast<size_t>(next) < this->palette_scroll_) this->palette_scroll_ = static_cast<size_t>(next);
            if (static_cast<size_t>(next) >= this->palette_scroll_ + page) this->palette_scroll_ = static_cast<size_t>(next - page + 1);
            return;
        }
        const auto controls = this->Controls();
        const auto current = std::find_if(controls.begin(), controls.end(), [&](const auto& control) { return control.id == this->focused_control_ && control.enabled; });
        if (current == controls.end())
        {
            const auto first = std::find_if(controls.begin(), controls.end(), [](const auto& control) { return control.enabled && control.kind != control_kind::label; });
            if (first != controls.end()) { this->focused_control_ = first->id; focusResult(); }
            return;
        }
        if (dx && current->kind == control_kind::choice) { this->ActivateControl(current->id, dx); return; }
        const auto center = [](const ui::rect& bounds) { return ui::point{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}; };
        const auto origin = center(current->bounds); float score = std::numeric_limits<float>::max(); const control* best{};
        for (const auto& candidate : controls)
        {
            if (!candidate.enabled || candidate.id == current->id || candidate.kind == control_kind::label) continue;
            if (resultFocused && candidate.id >= result_first) continue;
            const auto target = center(candidate.bounds); const float x = target.x - origin.x, y = target.y - origin.y;
            if (dx && x * dx <= 1 || dy && y * dy <= 1) continue;
            const auto distance = dx ? std::abs(x) + std::abs(y) * 2 : std::abs(y) + std::abs(x) * 2;
            if (distance < score) { score = distance; best = &candidate; }
        }
        if (best) { this->focused_control_ = best->id; focusResult(); }
    }

    auto actionbars_module::BeginControllerBrowse(const bool crossbar) -> void
    {
        this->SuspendInput(); this->browsing_ = this->navigation_focus_ = true;
        this->selected_ = {crossbar ? action_surface::crossbar : action_surface::hotbar, 0, 0};
        if (crossbar) { this->view_ = action_bar_count; this->selected_bank_ = 0; this->SetPreview(false); }
        if (!crossbar)
            for (size_t bar = 0; bar < action_bar_count; ++bar) if (this->BarVisible(bar)) { this->selected_.bar = bar; return; }
    }
    auto actionbars_module::HandleControllerAction(const controller_action action, const bool repeated) -> bool
    {
        if (!this->InputFocused()) return false;
        const bool direction = action == controller_action::up || action == controller_action::down || action == controller_action::left || action == controller_action::right;
        if (repeated && !direction) return true;
        this->navigation_focus_ = true;
        if (this->HandleSetupController(action, repeated)) return true;
        if (this->window_gesture_ != window_gesture::none)
        {
            const bool pointerGesture = this->pointer_down_;
            if (action == controller_action::cancel || action == controller_action::confirm)
            {
                this->FinishWindowGesture(action == controller_action::confirm);
                if (this->pointer_down_) this->discard_pointer_release_ = true;
                this->pointer_down_ = false;
                if (pointerGesture) this->navigation_focus_ = false;
            }
            else if (direction)
            {
                const auto bounds = this->workspace_bounds_[this->window_workspace_];
                const float step = this->WindowDragging() ? 5.0f : 10.0f;
                const float x = this->WindowDragging() ? bounds.x - this->window_start_.x : bounds.width - this->window_start_.width;
                const float y = this->WindowDragging() ? bounds.y - this->window_start_.y : bounds.height - this->window_start_.height;
                this->AdjustWindow(x + (action == controller_action::left ? -step : action == controller_action::right ? step : 0),
                    y + (action == controller_action::up ? -step : action == controller_action::down ? step : 0));
            }
            return true;
        }
        if (this->input_.Focused())
        {
            if (action == controller_action::cancel) { this->input_.Cancel(); this->editing_field_ = 0; }
            else if (action == controller_action::confirm) this->CommitField();
            return true;
        }
        if (this->binding_capture_)
        {
            if (action == controller_action::cancel) this->binding_capture_ = false;
            return true;
        }
        if (this->placing_)
        {
            if (action == controller_action::cancel) { this->placing_ = false; this->focused_control_ = result_first + static_cast<int>(this->focused_result_); }
            else if (action == controller_action::confirm) { if (this->AssignAction(this->selected_, this->draft_)) this->CloseDialog(); }
            else this->MoveAssignment(action);
            return true;
        }
        if (this->dialog_ != dialog::none)
        {
            if (this->dialog_ == dialog::picker && action == controller_action::actions && this->focused_control_ >= result_first && this->EditLibraryEntry(static_cast<size_t>(this->focused_control_ - result_first))) return true;
            if (action == controller_action::cancel)
            {
                if (this->dialog_ == dialog::icons) this->CloseIconPicker();
                else if (this->dialog_ == dialog::palette_dropdown || this->dialog_ == dialog::slot_menu || this->dialog_ == dialog::bar_menu) this->CloseDialog();
                else this->ActivateControl(command_cancel);
            }
            else if (action == controller_action::confirm || action == controller_action::actions) this->ActivateControl(this->focused_control_);
            else if (direction) this->MoveControl(action == controller_action::left ? -1 : action == controller_action::right ? 1 : 0, action == controller_action::up ? -1 : action == controller_action::down ? 1 : 0);
            else if (action == controller_action::next_tab || action == controller_action::previous_tab)
            {
                auto controls = this->Controls(); std::erase_if(controls, [](const auto& control) { return !control.enabled || control.kind == control_kind::label; });
                const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& control) { return control.id == this->focused_control_; });
                if (!controls.empty())
                {
                    const int index = found == controls.end() ? 0 : static_cast<int>(found - controls.begin());
                    const int count = static_cast<int>(controls.size()); this->focused_control_ = controls[(index + (action == controller_action::next_tab ? 1 : -1) + count) % count].id;
                    if (this->dialog_ == dialog::picker && this->focused_control_ >= result_first && static_cast<size_t>(this->focused_control_ - result_first) < this->results_.size())
                    {
                        const auto layout = this->LibraryLayout();
                        this->focused_result_ = static_cast<size_t>(this->focused_control_ - result_first);
                        const auto row = this->focused_result_ / layout.columns;
                        if (row < this->result_scroll_) this->SetScroll(static_cast<int>(row));
                        else if (row >= this->result_scroll_ + layout.rows) this->SetScroll(static_cast<int>(row - layout.rows + 1));
                    }
                }
            }
            return true;
        }
        if (action == controller_action::cancel) { this->SuspendInput(); return true; }
        if (action == controller_action::actions) { this->OpenSlotMenu(this->selected_); return true; }
        if (action == controller_action::confirm)
        {
            if (this->store_ && !this->store_->Resolve(this->selected_.surface, this->selected_.bar, this->selected_.slot).Empty()) this->Activate(this->selected_);
            else this->OpenLibrary(this->selected_.surface == action_surface::crossbar, this->selected_.bar, this->selected_.slot);
            return true;
        }
        if (action == controller_action::next_tab || action == controller_action::previous_tab)
        { if (this->store_) this->store_->Cycle(this->selected_.surface, action == controller_action::next_tab ? 1 : -1, this->selected_.bar); return true; }
        if (action == controller_action::cycle_window)
        {
            const auto previous = this->selected_.surface == action_surface::crossbar ? action_bar_count : this->selected_.bar;
            for (size_t offset = 1; offset <= this->bars_.size(); ++offset)
            {
                const auto next = (previous + offset) % this->bars_.size();
                if (this->BarVisible(next)) { this->selected_ = {next == action_bar_count ? action_surface::crossbar : action_surface::hotbar, next == action_bar_count ? 0 : next, 0}; break; }
            }
            return true;
        }
        if (direction)
        {
            this->MoveAssignment(action);
        }
        return true;
    }
    auto actionbars_module::HandleCrossbarEvent(const controller_event& event) -> bool
    {
        if (event.action == controller_action::crossbar_bank)
        {
            if (event.bank > crossbar_bank::right_double) return false;
            if (event.bank != this->active_bank_)
            {
                this->held_slots_ = {};
                for (auto& bank : this->button_motion_) for (auto& button : bank) button.Set(false, event.timestamp);
            }
            this->active_bank_ = this->available_[action_bar_count] && this->bars_[action_bar_count].state.enabled ? event.bank : crossbar_bank::none;
            this->triggers_ = event.triggers;
            for (size_t trigger = 0; trigger < this->trigger_motion_.size(); ++trigger)
                this->trigger_motion_[trigger].Set((event.triggers & (1u << trigger)) != 0, event.timestamp);
            return true;
        }
        if (event.action != controller_action::crossbar_slot && event.action != controller_action::palette_previous && event.action != controller_action::palette_next) return false;
        if (!event.pressed)
        {
            if (event.action == controller_action::crossbar_slot && event.bank != crossbar_bank::none && BankIndex(event.bank) < action_crossbar_banks && event.slot >= 1 && event.slot <= 8)
            {
                this->held_slots_[BankIndex(event.bank)][event.slot - 1] = false;
                this->button_motion_[BankIndex(event.bank)][event.slot - 1].Set(false, event.timestamp);
            }
            return true;
        }
        if (event.repeated) return true;
        if (this->InputFocused() || !this->controller_settings_.enabled || !this->controller_settings_.replace_native || !this->BarVisible(action_bar_count)) return true;
        if (event.action == controller_action::crossbar_slot)
        {
            if (event.bank != crossbar_bank::none && event.bank == this->active_bank_ && event.slot >= 1 && event.slot <= 8)
            {
                this->held_slots_[BankIndex(event.bank)][event.slot - 1] = true;
                this->button_motion_[BankIndex(event.bank)][event.slot - 1].Set(true, event.timestamp, 150);
                this->Activate({action_surface::crossbar, BankIndex(event.bank), static_cast<size_t>(event.slot - 1)});
            }
        }
        else if (this->store_) this->store_->Cycle(action_surface::crossbar, event.action == controller_action::palette_previous ? -1 : 1, event.bank == crossbar_bank::none ? 0 : BankIndex(event.bank));
        return true;
    }

    auto actionbars_module::Binding(const address& value) const -> actionbar_binding
    { return value.surface == action_surface::hotbar && value.slot < action_bar_slots ? this->Presentation(value).keys[value.slot] : actionbar_binding{}; }
    auto actionbars_module::BindingLabel(const actionbar_binding value) -> std::string
    {
        if (value.key == 0) return "Unbound";
        std::string label;
        if (value.modifiers & 1) label += "Ctrl+"; if (value.modifiers & 2) label += "Alt+"; if (value.modifiers & 4) label += "Shift+";
        char keyName[64]{}; const auto scan = MapVirtualKeyA(value.key, MAPVK_VK_TO_VSC);
        if (GetKeyNameTextA(static_cast<LONG>(scan << 16), keyName, static_cast<int>(sizeof(keyName))) > 0) label += keyName;
        else label += "Key " + std::to_string(value.key);
        return label;
    }
    auto actionbars_module::HandleKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if (key >= this->owned_keys_.size()) return this->InputFocused();
        const bool released = (static_cast<uintptr_t>(flags) & (uintptr_t{1} << 31)) != 0;
        const bool repeated = (static_cast<uintptr_t>(flags) & (uintptr_t{1} << 30)) != 0;
        if (released) { const bool owned = this->owned_keys_[key]; this->owned_keys_[key] = false; return owned; }
        const uint8_t modifiers = ((GetKeyState(VK_CONTROL) & 0x8000) ? 1 : 0) | ((GetKeyState(VK_MENU) & 0x8000) ? 2 : 0) | ((GetKeyState(VK_SHIFT) & 0x8000) ? 4 : 0);
        if (this->FloatingWindowOpen() && key == VK_TAB && (modifiers & 2)) { this->SetWindowFocused(false); return false; }
        if (this->setup_value_input_.Focused())
        {
            this->owned_keys_[key] = true;
            if (key == VK_ESCAPE) this->FinishSetupValue(false);
            else if (key == VK_RETURN || key == VK_TAB)
            {
                if (!repeated && this->FinishSetupValue(true, true) && key == VK_TAB)
                    this->HandleControllerAction((modifiers & 4) ? controller_action::previous_tab : controller_action::next_tab, false);
            }
            else this->setup_value_input_.HandleKey(key, flags, this->core_ && this->core_->GetProperties() ? this->core_->GetProperties()->GetFinalFantasyHwnd() : nullptr);
            return true;
        }
        if (this->setup_choice_input_.Opened())
        {
            this->owned_keys_[key] = true;
            if (key == VK_ESCAPE || key == VK_TAB) this->setup_choice_input_.Cancel();
            else if (key == VK_RETURN && !repeated) this->SelectSetupChoice(this->setup_choice_input_.Highlighted());
            else if (key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END)
                this->setup_choice_input_.Move(key == VK_UP ? -1 : key == VK_DOWN ? 1 : key == VK_HOME ? -4096 : 4096);
            return true;
        }
        if (this->binding_capture_)
        {
            this->owned_keys_[key] = true;
            if (repeated) return true;
            if (key == VK_ESCAPE) { this->binding_capture_ = false; this->message_.clear(); return true; }
            if (key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU || key == VK_LWIN || key == VK_RWIN) return true;
            const actionbar_binding binding{static_cast<uint16_t>(key), modifiers};
            if (!this->allow_native_keys_ && key >= '0' && key <= '9' && (modifiers & 3)) { this->Notice("Enable Replace native macros before binding Ctrl/Alt + numbers."); return true; }
            for (size_t bar = 0; bar < action_bar_count; ++bar)
                for (size_t slot = 0; slot < action_bar_slots; ++slot)
                    if (this->bars_[bar].keys[slot] == binding && !(this->selected_.surface == action_surface::hotbar && this->selected_.bar == bar && this->selected_.slot == slot))
                    { this->Notice("Already bound to " + std::string{bar_names[bar]} + ", slot " + std::to_string(slot + 1) + ". Unbind it first."); return true; }
            this->Presentation(this->selected_).keys[this->selected_.slot] = binding; this->binding_capture_ = false; this->settings_changed_ = true; this->message_.clear(); return true;
        }
        if ((this->dialog_ == dialog::picker || this->dialog_ == dialog::icons) && this->window_focused_ && key == 'F' && (modifiers & 3) == 1)
        {
            this->owned_keys_[key] = true;
            if (!repeated) this->BeginField(this->dialog_ == dialog::icons ? field_icon_query : field_query);
            return true;
        }
        if (this->input_.Focused())
        {
            this->owned_keys_[key] = true;
            if (key == VK_ESCAPE) { this->input_.Cancel(); this->editing_field_ = 0; return true; }
            if (this->dialog_ == dialog::editor && this->draft_.kind == action_kind::macro && this->editing_field_ >= line_first && this->editing_field_ < line_first + 32)
            {
                const auto line = static_cast<size_t>(this->editing_field_ - line_first);
                if ((modifiers & 3) == 1 && key == 'V')
                {
                    const auto window = this->core_ && this->core_->GetProperties() ? this->core_->GetProperties()->GetFinalFantasyHwnd() : nullptr;
                    if (const auto text = ui::ReadClipboardText(window, 32 * 242); text && this->PasteEditorLines(*text)) return true;
                }
                if (key == VK_UP || key == VK_DOWN || (key == VK_RETURN && !repeated))
                {
                    this->CommitField();
                    const auto next = key == VK_UP ? (line > 0 ? line - 1 : 0) : std::min<size_t>(31, line + 1);
                    this->RevealEditorLine(next); this->BeginField(this->focused_control_); return true;
                }
            }
            if (key == VK_RETURN || key == VK_TAB) { if (!repeated) { this->CommitField(); if (key == VK_TAB) this->HandleControllerAction((modifiers & 4) ? controller_action::previous_tab : controller_action::next_tab, false); } return true; }
            this->input_.HandleKey(key, flags, this->core_ && this->core_->GetProperties() ? this->core_->GetProperties()->GetFinalFantasyHwnd() : nullptr);
            this->SetFieldValue(this->editing_field_, this->input_.Editor().Text()); return true;
        }
        if (this->IsWindowDialog(this->dialog_) && !this->navigation_focus_ && !this->WindowDragging() && !this->WindowResizing())
        {
            if (!this->window_focused_ || (key != VK_ESCAPE && key != VK_TAB))
            {
                if (this->ReplacesNativeMacroKey(key)) { this->owned_keys_[key] = true; return true; }
                return false;
            }
        }
        if (this->InputFocused())
        {
            this->owned_keys_[key] = true;
            controller_action action{};
            switch (key)
            {
            case VK_ESCAPE: action = controller_action::cancel; break;
            case VK_RETURN: case VK_SPACE: action = controller_action::confirm; break;
            case VK_UP: action = controller_action::up; break;
            case VK_DOWN: action = controller_action::down; break;
            case VK_LEFT: action = controller_action::left; break;
            case VK_RIGHT: action = controller_action::right; break;
            case VK_TAB: action = modifiers & 4 ? controller_action::previous_tab : controller_action::next_tab; break;
            case VK_APPS: action = controller_action::actions; break;
            case VK_PRIOR: action = controller_action::previous_tab; break;
            case VK_NEXT: action = controller_action::next_tab; break;
            default: return true;
            }
            return this->HandleControllerAction(action, repeated);
        }
        if (this->owned_keys_[key]) return true;
        const bool replacement = this->ReplacesNativeMacroKey(key);
        if (repeated) { if (replacement) this->owned_keys_[key] = true; return replacement; }
        if (key >= '0' && key <= '9' && (modifiers & 3) && !replacement) return false;
        const actionbar_binding binding{static_cast<uint16_t>(key), modifiers};
        for (size_t bar = 0; bar < action_bar_count; ++bar)
        {
            if (!this->BarVisible(bar)) continue;
            for (size_t slot = 0; slot < static_cast<size_t>(this->bars_[bar].slots); ++slot)
                if (this->bars_[bar].keys[slot] == binding && key != 0)
                {
                    this->owned_keys_[key] = true;
                    if (this->store_ && !this->store_->Resolve(action_surface::hotbar, bar, slot).Empty()) this->Activate({action_surface::hotbar, bar, slot});
                    return true;
                }
        }
        if (replacement) { this->owned_keys_[key] = true; return true; }
        return false;
    }

    auto actionbars_module::HandleMouse(const uint32_t message, const WPARAM wParam, const float x, const float y, const bool blocked, const bool editing) -> bool
    {
        this->hover_x_ = x; this->hover_y_ = y;
        if (message == WM_LBUTTONUP && this->discard_pointer_release_) { this->discard_pointer_release_ = false; return true; }
        if (blocked)
        {
            const bool owned = this->pointer_down_;
            if (this->FloatingWindowOpen()) { this->CancelPointerGesture(); this->SetWindowFocused(false); }
            else if (owned) this->SuspendInput();
            if (message == WM_LBUTTONUP) this->discard_pointer_release_ = false;
            return owned;
        }
        if (this->HandleSetupMouse(message, wParam, x, y)) return true;
        if (this->window_gesture_ != window_gesture::none && this->pointer_down_ && (message == WM_MOUSEMOVE || message == WM_LBUTTONUP))
        {
            this->AdjustWindow(x - this->down_x_, y - this->down_y_);
            if (message == WM_LBUTTONUP) { this->FinishWindowGesture(true); this->pointer_down_ = false; }
            return true;
        }
        if (editing && this->dialog_ == dialog::none && !this->pointer_down_)
        {
            const auto hit = this->HitTest(x, y);
            if (!hit || hit->header == 0 && message != WM_RBUTTONDOWN) return false;
        }
        if (this->scrolling_ && this->pointer_down_ && (message == WM_MOUSEMOVE || message == WM_LBUTTONUP))
        {
            this->SetScroll(this->Scrollbar().PositionAt(y, this->scroll_grab_));
            if (message == WM_LBUTTONUP) this->pointer_down_ = this->scrolling_ = false;
            return true;
        }
        if (message == WM_MOUSEWHEEL)
        {
            const int direction = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -1 : 1;
            if (this->dialog_ != dialog::none && Contains(this->DialogBounds(), x, y))
            {
                const auto scrollbar = this->Scrollbar();
                const auto position = this->dialog_ == dialog::picker ? this->result_scroll_ : this->dialog_ == dialog::icons ? this->icon_scroll_ : this->palette_scroll_;
                if (scrollbar.Visible()) this->SetScroll(static_cast<int>(position) + direction * 3);
                return true;
            }
            const auto hit = this->HitTest(x, y);
            if (hit && hit->header == 1 && this->store_) { this->store_->Cycle(hit->slot.surface, direction, hit->slot.bar); return true; }
            return hit.has_value();
        }
        if (message == WM_RBUTTONDOWN)
        {
            const bool bar = this->BarPointerAvailable(x, y) && this->HitTest(x, y).has_value();
            if (this->dialog_ == dialog::picker)
                for (const auto& item : this->Controls())
                    if (item.id >= result_first && Contains(item.bounds, x, y) && this->EditLibraryEntry(static_cast<size_t>(item.id - result_first))) return true;
            if (this->IsWindowDialog(this->dialog_) && !bar)
            {
                if (!Contains(this->WindowBounds(), x, y)) { this->SetWindowFocused(false); return false; }
                this->browsing_ = this->navigation_focus_ = false;
                this->SetWindowFocused(true); return true;
            }
            if (this->dialog_ != dialog::none && !bar) { this->browsing_ = this->navigation_focus_ = false; this->CloseDialog(); return true; }
            const auto hit = this->HitTest(x, y);
            if (hit)
            {
                this->browsing_ = this->navigation_focus_ = false;
                if (hit->header == 5 && hit->slot.surface == action_surface::hotbar) this->CycleShape(hit->slot.bar);
                else if (hit->header == 0) this->OpenSlotMenu(hit->slot);
                else this->OpenBarMenu(hit->slot);
                return true;
            }
            return false;
        }
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
        {
            this->browsing_ = this->navigation_focus_ = false;
            if (this->placing_) { this->placing_ = false; }
            if (this->dialog_ != dialog::none && !(this->BarPointerAvailable(x, y) && this->HitTest(x, y)))
            {
                if (this->IsWindowDialog(this->dialog_))
                {
                    const auto edges = this->WindowResizeEdgesAt(x, y);
                    if (!Contains(this->WindowBounds(), x, y) && edges == 0) { this->SetWindowFocused(false); return false; }
                    this->SetWindowFocused(true);
                    if (edges != 0 || Contains(this->WindowTitleBounds(), x, y))
                    {
                        this->BeginWindowGesture(edges != 0 ? window_gesture::resize : window_gesture::move, edges);
                        this->pointer_down_ = true; this->down_x_ = x; this->down_y_ = y;
                        this->pressed_control_ = 0; return true;
                    }
                }
                if (!Contains(this->DialogBounds(), x, y)) { this->CloseDialog(); this->discard_pointer_release_ = true; return true; }
                const auto scrollbar = this->Scrollbar();
                if (scrollbar.Visible() && Contains(scrollbar.track, x, y))
                {
                    if (Contains(scrollbar.Thumb(), x, y)) { this->scrolling_ = this->pointer_down_ = true; this->scroll_grab_ = y - scrollbar.Thumb().y; }
                    else this->SetScroll(scrollbar.PageAt(y));
                    return true;
                }
                this->CommitField(); this->pointer_down_ = true; this->pressed_control_ = 0;
                for (const auto& control : this->Controls()) if (control.enabled && Contains(control.bounds, x, y)) { this->pressed_control_ = control.id; this->focused_control_ = control.id; break; }
                this->down_x_ = x; this->down_y_ = y; this->library_pressed_.reset();
                if (this->dialog_ == dialog::picker && this->pressed_control_ >= result_first)
                    this->library_pressed_ = static_cast<size_t>(this->pressed_control_ - result_first);
                return true;
            }
            const auto hit = this->HitTest(x, y); if (!hit) return false;
            this->pointer_down_ = this->bar_pointer_down_ = true; this->dragging_ = false; this->pressed_ = hit->slot; this->down_header_ = hit->header;
            if (this->dialog_ == dialog::none) this->selected_ = hit->slot;
            this->down_x_ = x; this->down_y_ = y; this->pressed_control_ = 0;
            return true;
        }
        if (message == WM_MOUSEMOVE && this->pointer_down_)
        {
            if (this->library_pressed_ && *this->library_pressed_ < this->results_.size() && std::hypot(x - this->down_x_, y - this->down_y_) >= 6)
                this->library_drag_ = this->results_[*this->library_pressed_];
            if (this->bar_pointer_down_ && this->down_header_ == 0 && !this->Presentation(this->pressed_).locked && std::hypot(x - this->down_x_, y - this->down_y_) >= 6)
                this->dragging_ = true;
            return true;
        }
        if (message == WM_LBUTTONUP && this->pointer_down_)
        {
            this->pointer_down_ = false;
            const bool bar = std::exchange(this->bar_pointer_down_, false);
            this->library_pressed_.reset();
            if (this->library_drag_)
            {
                const auto hit = this->HitTest(x, y);
                if (!Contains(this->DialogBounds(), x, y) && hit && hit->header == 0) this->AssignAction(hit->slot, *this->library_drag_);
                this->library_drag_.reset(); this->pressed_control_ = 0;
                return true;
            }
            if (this->dialog_ != dialog::none && !bar)
            {
                const auto controls = this->Controls();
                const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& control) { return control.enabled && control.id == this->pressed_control_ && Contains(control.bounds, x, y); });
                if (found != controls.end()) this->ActivateControl(found->id);
                this->pressed_control_ = 0; return true;
            }
            const auto hit = this->BarPointerAvailable(x, y) ? this->HitTest(x, y) : std::nullopt;
            if (this->dragging_)
            {
                this->dragging_ = false;
                if (hit && hit->header == 0 && !this->Presentation(hit->slot).locked && this->store_)
                    this->store_->Swap(this->pressed_.surface, this->pressed_.bar, this->pressed_.slot, hit->slot.surface, hit->slot.bar, hit->slot.slot);
                return true;
            }
            if (!hit || hit->slot != this->pressed_ || hit->header != this->down_header_) return true;
            if (hit->header == 1)
            {
                this->RememberLibrary();
                this->selected_ = hit->slot; this->view_ = hit->slot.surface == action_surface::crossbar ? action_bar_count : hit->slot.bar;
                this->dialog_ = dialog::palette_dropdown; this->palette_scroll_ = 0; this->focused_control_ = palette_first;
            }
            else if (hit->header == 2 || hit->header == 5) this->OpenBarMenu(hit->slot);
            else if (hit->header == 4) { this->Presentation(hit->slot).locked = !this->Presentation(hit->slot).locked; this->settings_changed_ = true; }
            else if (hit->header == 0)
            {
                if (this->dialog_ == dialog::picker) { this->selected_ = hit->slot; this->view_ = hit->slot.surface == action_surface::crossbar ? action_bar_count : hit->slot.bar; }
                else if (this->store_ && !this->store_->Resolve(hit->slot.surface, hit->slot.bar, hit->slot.slot).Empty()) this->Activate(hit->slot);
                else this->OpenLibrary(hit->slot.surface == action_surface::crossbar, hit->slot.bar, hit->slot.slot);
            }
            return true;
        }
        return message == WM_MOUSEMOVE && this->PointerOwns(x, y);
    }
}

namespace xiui
{
    auto actionbars_module::Inspect(const address& value) -> const action_availability&
    {
        const auto key = static_cast<uint32_t>((value.surface == action_surface::crossbar ? 256 : 0) + value.bar * 16 + value.slot);
        auto& cached = this->inspected_[key]; const auto revision = this->store_ ? this->store_->Revision() : 0;
        if (cached.at == 0 || this->now_ - cached.at >= 100 || cached.revision != revision)
        {
            cached.at = this->now_; cached.revision = revision;
            cached.value = this->store_ && this->executor_ ? this->executor_->Inspect(this->core_, this->store_->Resolve(value.surface, value.bar, value.slot), true) : action_availability{};
        }
        return cached.value;
    }




    auto actionbars_module::RenderOverlay(const module_render_context& context) -> void
    {
        this->viewport_ = context.Viewport();
        const auto& design = context.Design(); const auto& theme = design.GetTheme();
        if (this->dialog_ == dialog::picker) this->UpdateLibraryMetrics(design);
        if (this->placing_)
        {
            const auto focus = this->ScreenSlotBounds(this->selected_); design.DrawFocusFrame(focus, this->focus_style_);
            const ui::rect instructions{this->viewport_.x + (this->viewport_.width - 480) / 2, this->viewport_.y + 32, 480, 68};
            context.DrawTooltip(instructions);
            const auto text = "Place " + this->draft_.name + "  /  " + (this->selected_.surface == action_surface::crossbar ? std::string{bank_names[this->selected_.bar]} : std::string{bar_names[this->selected_.bar]}) + " / " + std::to_string(this->selected_.slot + 1);
            context.DrawText(text.c_str(), {instructions.x + 12, instructions.y + 8, instructions.width - 24, 24}, instructions, this->text_[4]);
            context.DrawText("Directions: slot   Tabs: bar / bank   Confirm: assign   Cancel: back", {instructions.x + 12, instructions.y + 34, instructions.width - 24, 24}, instructions, this->text_[2]);
            return;
        }
        if (this->dialog_ == dialog::none)
        {
            if (this->browsing_ && this->navigation_focus_ && this->BarVisible(this->selected_.surface == action_surface::crossbar ? action_bar_count : this->selected_.bar))
            {
                const auto focus = this->ScreenSlotBounds(this->selected_); design.DrawFocusFrame(focus, this->focus_style_);
                this->DrawTooltip(this->selected_, focus, context);
            }
            else if (!this->pointer_down_)
            {
                const auto hit = this->HitTest(this->hover_x_, this->hover_y_);
                if (hit && hit->header == 0) this->DrawTooltip(hit->slot, hit->bounds, context);
                else if (hit && hit->header == 2)
                {
                    const auto viewport = context.Viewport(); const float width = std::min(344.0f, viewport.width - 16);
                    const ui::rect tooltip{std::clamp(hit->bounds.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8)), std::max(viewport.y + 8, hit->bounds.y - 68), width, 60};
                    context.DrawTooltip(tooltip);
                    context.DrawText("Bar options", {tooltip.x + 12, tooltip.y + 6, width - 24, 22}, tooltip, this->text_[0]);
                    const auto help = xiui::ui::FitText(design, "Actions & Macros, Action Sets and settings", width - 24, this->text_[4].style);
                    context.DrawText(help.c_str(), {tooltip.x + 12, tooltip.y + 30, width - 24, 22}, tooltip, this->text_[4]);
                }
            }
            this->DrawSlotDrag(context);
            if (!this->message_.empty())
            {
                const auto anchor = this->ScreenSlotBounds(this->selected_); const auto viewport = context.Viewport();
                const ui::rect bounds{std::clamp(anchor.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - 376)), std::max(viewport.y + 8, anchor.y - 48), 368, 40};
                context.DrawTooltip(bounds); context.DrawText(this->message_.c_str(), {bounds.x + 12, bounds.y + 8, bounds.width - 24, 24}, bounds, this->text_[4]);
            }
            return;
        }
        const bool floating = this->IsWindowDialog(this->dialog_);
        if (!floating && this->FloatingWindowOpen())
        {
            const auto modal = this->dialog_; const auto focused = this->window_focused_, navigation = this->navigation_focus_;
            if (this->library_popup_) this->SwapLibraryContext(*this->library_popup_);
            this->dialog_ = this->WindowDialog(); this->window_focused_ = this->navigation_focus_ = false;
            this->RenderOverlay(context);
            this->dialog_ = modal; this->window_focused_ = focused; this->navigation_focus_ = navigation;
            if (this->library_popup_) this->SwapLibraryContext(*this->library_popup_);
            design.FillRect(this->WindowBounds(), 0x50000000);
        }
        const auto bounds = this->DialogBounds();
        if (floating) this->DrawWindowChrome(context); else context.DrawTooltip(bounds);
        ui::config_controls controls{design, nullptr, context.NativeAssets()};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        auto managementText = theme.body_text; managementText.scale = 0.76f; managementText.alignment = ui::text_alignment::left;
        auto captionText = theme.caption_text; captionText.scale = 0.64f; captionText.alignment = ui::text_alignment::left;
        const bool popup = this->dialog_ == dialog::palette_dropdown || this->dialog_ == dialog::slot_menu || this->dialog_ == dialog::bar_menu;
        if (!popup)
        {
            std::string title;
            switch (this->dialog_)
            {
            case dialog::picker: title = "Actions & Macros"; break;
            case dialog::editor: title = this->editor_new_ ? "Create Macro" : "Edit Macro"; break;
            case dialog::palettes: title = this->selected_.surface == action_surface::crossbar ? "Crossbar action sets" : "Hotbar action sets (all bars)"; break;
            case dialog::name: title = this->name_action_ == command_copy ? "Duplicate palette" : this->name_action_ == command_rename ? "Rename palette" : "New palette"; break;
            case dialog::delete_palette: title = "Delete palette?"; break;
            case dialog::delete_macro: title = "Delete shared macro?"; break;
            case dialog::import: title = "Import action palettes"; break;
            case dialog::setup: title = "Controller Setup"; break;
            case dialog::keybinds: title = "Keybinds"; break;
            case dialog::icons: title = "Choose Icon"; break;
            default: break;
            }
            if (floating)
            {
                const auto clip = design.Clip(this->WindowTitleBounds());
                auto style = theme.accent_text; style.scale = 0.82f;
                controls.Text(title.c_str(), this->WindowTitleBounds(), style);
            }
            else { auto style = theme.accent_text; style.scale = 0.82f; controls.Text(title.c_str(), {bounds.x + 16, bounds.y + 12, bounds.width - 32, 26}, style); }
        }
        if (this->dialog_ == dialog::picker && this->results_.empty())
        {
            const auto layout = this->LibraryLayout(); const auto clip = design.Clip(layout.list);
            controls.Text("No matching actions.", {layout.list.x + 6, layout.list.y + 6, layout.list.width - 12, layout.name_height + 8}, captionText);
        }
        if (this->dialog_ == dialog::editor && bounds.height >= 500 && this->message_.empty() && !this->draft_.macro_id.empty())
            controls.Text("Shared action: edits update every slot using it.", {bounds.x + 16, bounds.y + bounds.height - 68, bounds.width - 32, 16}, captionText);
        if (this->dialog_ == dialog::editor)
        {
            const auto editor = this->EditorLayout(); this->DrawLibraryIcon(this->draft_, editor.icon, context);
            if (!this->editor_options_ && (this->draft_.kind == action_kind::macro || this->draft_.kind == action_kind::command))
            {
                design.FillRoundedRect({editor.commands.x, editor.commands.y - 2, editor.commands.width, editor.commands.height + 2}, 0xEF0D101A, 3);
                design.BorderRect({editor.commands.x, editor.commands.y - 2, editor.commands.width, editor.commands.height + 2}, 0x90696B84);
            }
        }
        if (this->dialog_ == dialog::icons) this->DrawIconPickerDetails(context);
        if (this->dialog_ == dialog::delete_palette || this->dialog_ == dialog::delete_macro)
        {
            std::string description = "Cancel keeps your current setup.";
            if (this->dialog_ == dialog::delete_palette && this->store_)
                for (const auto& palette : this->store_->AllPalettes()) if (palette.id == this->palette_id_) description = "Remove \"" + palette.name + "\" and its assignments?";
            if (this->dialog_ == dialog::delete_macro) description = "Remove \"" + this->draft_.name + "\" from the library?\nAssigned slots keep their commands.";
            controls.Text(description.c_str(), {bounds.x + 16, bounds.y + 60, bounds.width - 32, 72}, managementText);
        }
        if (this->dialog_ == dialog::import)
        {
            float y = bounds.y + 164;
            if (this->import_preview_.Valid())
            {
                const auto summary = std::to_string(this->import_preview_.actions) + " actions  /  " + std::to_string(this->import_preview_.palettes.size()) + " new palettes";
                controls.Text(summary.c_str(), {bounds.x + 16, y, bounds.width - 32, 26}, managementText); y += 32;
                for (const auto& palette : this->import_preview_.palettes)
                { if (y > bounds.y + bounds.height - 130) break; controls.Text(palette.name.c_str(), {bounds.x + 24, y, bounds.width - 48, 24}, captionText); y += 24; }
                for (const auto& warning : this->import_preview_.warnings)
                { if (y > bounds.y + bounds.height - 106) break; controls.Text(warning.c_str(), {bounds.x + 16, y, bounds.width - 32, 24}, captionText); y += 24; }
            }
            else controls.Text("Choose a Lua XIUI, tHotBar or tCrossBar data file.\nThe preview reads assignments without running Lua.\nImport always creates new palettes.", {bounds.x + 16, y, bounds.width - 32, 88}, managementText);
        }
        const auto controlList = this->Controls();
        const auto library = this->dialog_ == dialog::picker ? this->LibraryLayout() : library_layout{};
        for (const auto& control : controlList)
        {
            if (control.id == window_move || control.id == window_resize || control.id == window_close) continue;
            const bool hovered = control.enabled && Contains(control.bounds, this->hover_x_, this->hover_y_);
            auto clipBounds = floating ? ui::rect{bounds.x + 6, bounds.y + 34, bounds.width - 12, bounds.height - 40} : bounds;
            if (this->dialog_ == dialog::picker)
            {
                if (control.id >= category_first && control.id < category_first + static_cast<int>(std::size(categories))) clipBounds = library.sidebar;
                else if (control.id >= result_first) clipBounds = control.bounds;
                else clipBounds = control.bounds;
            }
            const auto clip = design.Clip(clipBounds);
            auto style = managementText; style.alignment = popup ? ui::text_alignment::left : ui::text_alignment::center;
            if (!control.enabled) { style.top_color = style.bottom_color = theme.colors.muted; }
            if (control.kind == control_kind::label)
            {
                if (this->dialog_ == dialog::editor)
                {
                    auto caption = controls.Style(theme.accent_text); caption.scale = std::max(caption.scale, 13 / ui::text_base_pixels);
                    design.DrawText(control.text.c_str(), control.bounds, caption);
                }
                else controls.Text(control.text.c_str(), control.bounds, captionText);
            }
            else if (control.kind == control_kind::dropdown)
                controls.Dropdown(control.bounds, control.text.c_str(), this->setup_choice_input_.Opened() && this->setup_control_ == control.id, hovered);
            else if (control.kind == control_kind::slider)
            {
                float value = control.value;
                auto option = this->SetupSliderOption(control, value);
                controls.Slider(control.bounds, option, control.enabled && (hovered || this->setup_slider_adjust_ == control.id),
                    this->setup_value_input_.Focused() && this->setup_control_ == control.id ? &this->setup_value_input_ : nullptr, control.label_width);
            }
            else if (control.kind == control_kind::field)
            {
                const auto caption = ui::rect{bounds.x + 16, control.bounds.y, control.bounds.x - bounds.x - 24, control.bounds.height};
                if (!control.hint.empty())
                {
                    if (this->dialog_ == dialog::editor)
                    { auto number = controls.Style(captionText); number.scale = std::max(number.scale, 12 / ui::text_base_pixels); design.DrawText(control.hint.c_str(), caption, number); }
                    else controls.Text(control.hint.c_str(), caption, captionText);
                }
                const char* placeholder = control.id == field_icon_query ? "Search icons by name, job or magic type..." : control.id == field_query ? "Search actions and macros..." : control.id == field_icon ? "Default artwork" : control.id == field_label ? "Use action name" : "";
                const char* shortcut = control.id == field_icon_query || (control.id == field_query && control.bounds.width >= this->library_metrics_.search_with_shortcut_width) ? "Ctrl+F" : nullptr;
                const ui::text_input_appearance editorInput{14, this->editing_field_ == control.id && this->input_.Focused() ? 0xFF292A40 : 0xFF121520, 0xFFF4F4F8};
                const auto* appearance = this->dialog_ == dialog::editor ? &editorInput : nullptr;
                if (this->input_.Focused() && this->editing_field_ == control.id) controls.TextInput(control.bounds, this->input_, false, placeholder, shortcut, appearance);
                else { ui::text_input display; display.Open(control.text, 2048); display.Cancel(); controls.TextInput(control.bounds, display, false, placeholder, shortcut, appearance); }
            }
            else if (control.kind == control_kind::icon)
            {
                controls.NavigationItem(control.bounds, "", control.selected, hovered);
                action_slot icon; icon.icon = control.text;
                this->DrawLibraryIcon(icon, {control.bounds.x + 4, control.bounds.y + 4, control.bounds.width - 8, control.bounds.height - 8}, context);
            }
            else if (control.kind == control_kind::list)
            {
                if (this->dialog_ == dialog::picker && control.id >= result_first)
                {
                    controls.NavigationItem(control.bounds, "", control.selected, hovered);
                    const auto index = static_cast<size_t>(control.id - result_first);
                    if (index < this->results_.size()) this->DrawLibraryIcon(this->results_[index], {control.bounds.x + 4, control.bounds.y + (control.bounds.height - 32) / 2, 32, 32}, context);
                    const float textHeight = library.name_height + library.caption_height + (library.caption_height > 0 ? 2 : 0);
                    const float textY = control.bounds.y + (control.bounds.height - textHeight) / 2;
                    controls.Text(control.text.c_str(), {control.bounds.x + 46, textY, control.bounds.width - 52, library.name_height}, managementText);
                    if (!control.hint.empty()) controls.Text(control.hint.c_str(), {control.bounds.x + 46, textY + library.name_height + 2, control.bounds.width - 52, library.caption_height}, captionText);
                }
                else if (this->dialog_ == dialog::picker) controls.NavigationItem(control.bounds, control.text.c_str(), control.selected, hovered, control.enabled);
                else controls.ListItem(control.bounds, control.text.c_str(), control.hint.empty() ? nullptr : control.hint.c_str(), control.selected, hovered);
            }
            else if (control.kind == control_kind::choice)
            {
                if (!control.hint.empty()) controls.Text(control.hint.c_str(), {bounds.x + 16, control.bounds.y, control.bounds.x - bounds.x - 24, control.bounds.height}, captionText);
                const auto label = "< " + control.text + " >"; controls.Button(control.bounds, label.c_str(), style, hovered);
            }
            else controls.Button(control.bounds, control.text.c_str(), style, hovered || control.selected,
                control.danger ? ui::button_tone::danger : (this->dialog_ == dialog::editor && control.id == command_save) ||
                    (this->dialog_ == dialog::picker && control.id == new_macro) ? ui::button_tone::positive : ui::button_tone::normal);
        }
        const auto scrollbar = this->Scrollbar(); if (scrollbar.Visible()) controls.Scrollbar(scrollbar, Contains(scrollbar.track, this->hover_x_, this->hover_y_), this->scrolling_);
        if (this->navigation_focus_ && !this->KeyboardFocused() && !this->setup_choice_input_.Opened())
        {
            const auto focused = std::find_if(controlList.begin(), controlList.end(), [&](const auto& control) { return control.id == this->focused_control_ && control.enabled; });
            if (focused != controlList.end()) design.DrawFocusFrame(focused->bounds, this->focus_style_);
        }
        if ((this->dialog_ == dialog::picker || this->dialog_ == dialog::editor) && !this->pointer_down_ && !this->input_.Focused())
        {
            const auto help = std::find_if(controlList.begin(), controlList.end(), [&](const auto& control) {
                return (control.id == command_learned || control.id == command_all_actions || control.id == new_macro || control.id == new_command || control.id == new_native ||
                    this->dialog_ == dialog::editor && control.kind == control_kind::button && !control.hint.empty()) &&
                    (Contains(control.bounds, this->hover_x_, this->hover_y_) || this->navigation_focus_ && control.id == this->focused_control_);
            });
            if (help != controlList.end())
            {
                const auto resolved = controls.Style(captionText);
                const float width = std::min(this->viewport_.width - 16, std::ceil(design.MeasureText(help->hint.c_str(), resolved)) + 24);
                const float height = std::ceil(design.TextHeight(resolved)) + 16;
                const ui::rect tooltip{std::clamp(help->bounds.x, this->viewport_.x + 8, std::max(this->viewport_.x + 8, this->viewport_.x + this->viewport_.width - width - 8)),
                    std::min(help->bounds.y + help->bounds.height + 6, this->viewport_.y + this->viewport_.height - height - 8), width, height};
                context.DrawTooltip(tooltip); const auto clip = design.Clip(tooltip);
                controls.Text(help->hint.c_str(), {tooltip.x + 12, tooltip.y + 8, tooltip.width - 24, tooltip.height - 16}, captionText);
            }
        }
        if (floating && this->window_gesture_ != window_gesture::none && !this->pointer_down_)
        {
            const auto help = this->WindowDragging() ? "Directions: move window   Confirm: save   Cancel: restore" : "Directions: resize window   Confirm: save   Cancel: restore";
            design.FillRect({bounds.x + 10, bounds.y + bounds.height - 28, bounds.width - 36, 22}, theme.colors.surface);
            controls.Text(help, {bounds.x + 16, bounds.y + bounds.height - 24, bounds.width - 48, 20}, captionText);
        }
        else if (!this->message_.empty()) controls.Text(this->message_.c_str(), {bounds.x + 16, bounds.y + bounds.height - (this->dialog_ == dialog::editor ? 68 : 24), bounds.width - 32, 20}, captionText);
        this->DrawSlotDrag(context);
        if (this->library_drag_)
        {
            this->DrawLibraryIcon(*this->library_drag_, {this->hover_x_ - 18, this->hover_y_ - 18, 36, 36}, context);
            const auto hit = this->HitTest(this->hover_x_, this->hover_y_);
            if (!Contains(bounds, this->hover_x_, this->hover_y_) && hit && hit->header == 0) design.DrawFocusFrame(hit->bounds, this->focus_style_);
        }
        this->DrawSetupChoice(context);
    }
}
