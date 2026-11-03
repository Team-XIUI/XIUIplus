#include "localization/localization.hpp"
#include "chat_module.hpp"
#include "chat_theme.hpp"
#include "chat/chat_stack_layout.hpp"
#include "chat/chat_encoding.hpp"
#include "chat/chat_token.hpp"
#include "chat/chat_text.hpp"
#include "resources/auto_translate.hpp"
#include "utf8.hpp"
#include "ui/text_fit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <charconv>
#include <fstream>

namespace
{
    constexpr float dialogue_prompt_padding = 2.0f;
    constexpr uint64_t input_animation_ms = 240;

    auto InputEase(const float progress) -> float
    { return progress * progress * progress * (progress * (progress * 6 - 15) + 10); }

    auto KeyText(const WPARAM key, const LPARAM flags) -> std::string
    {
        BYTE state[256]{};
        ::GetKeyboardState(state);
        if (key < 256) state[key] |= 0x80;
        wchar_t text[8]{};
        const auto count = ::ToUnicodeEx(static_cast<UINT>(key), (static_cast<uint32_t>(flags) >> 16) & 0xFF, state, text, 8, 4, ::GetKeyboardLayout(0));
        return count > 0 ? xiui::ToUtf8({text, static_cast<size_t>(count)}) : std::string{};
    }

    auto MessageProgress(const uint64_t started, const uint64_t now, const float duration) -> float
    {
        if (now < started) return 1;
        const auto progress = std::min(1.0f, static_cast<float>(now - started) / duration);
        return progress * progress * (3 - 2 * progress);
    }

    constexpr const char* channels[]{"/s", "/p", "/l", "/l2", "/sh", "/y", "/u", "/t", "/r", "Game"};
    constexpr const char* commands[]{"/say ", "/party ", "/linkshell ", "/linkshell2 ", "/shout ", "/yell ", "/unity ", "/tell ", "/reply ", ""};
    auto SoundOption(const char* label, const char* description, int* selection) -> xiui::module_option
    {
        auto option = xiui::module_option::Dropdown(label, description, selection, xiui::chat_sound_names, std::size(xiui::chat_sound_names));
        option.choice_preview = true; option.preview_first = 1;
        return option;
    }
    auto CommandMode(const std::string_view command) -> uint32_t
    {
        constexpr std::pair<std::string_view, uint32_t> modes[]{
            {"/s", 1}, {"/say", 1}, {"/p", 5}, {"/party", 5}, {"/l", 6}, {"/linkshell", 6}, {"/l2", 213}, {"/linkshell2", 213},
            {"/t", 4}, {"/tell", 4}, {"/r", 4}, {"/reply", 4}, {"/sh", 2}, {"/shout", 2}, {"/y", 3}, {"/yell", 3}, {"/u", 211}, {"/unity", 211}};
        for (const auto& [name, mode] : modes) if (command == name) return mode;
        return 0;
    }

    auto SameRecipient(const std::string_view first, const std::string_view second) -> bool
    { return first.size() == second.size() && _strnicmp(first.data(), second.data(), first.size()) == 0; }

    auto IncomingTeller(const xiui::chat_line& line) -> std::string
    {
        std::string prefix;
        for (const auto& span : line.spans)
        {
            prefix.append(span.text, 0, 128 - prefix.size());
            if (prefix.find(">>") != std::string::npos || prefix.size() == 128) break;
        }
        const auto marker = prefix.find(">>");
        if (marker == std::string::npos) return {};
        std::string_view name{prefix.data(), marker};
        const auto trim = [](std::string_view text) {
            const auto start = text.find_first_not_of(" \t");
            return start == std::string_view::npos ? std::string_view{} :
                text.substr(start, text.find_last_not_of(" \t") - start + 1);
        };
        name = trim(name);
        if (name.starts_with('['))
        {
            const auto end = name.find(']');
            if ((end == 6 || end == 9) && name[3] == ':' &&
                std::all_of(name.begin() + 1, name.begin() + end, [](char c) { return (c >= '0' && c <= '9') || c == ':'; }))
                name = trim(name.substr(end + 1));
        }
        return xiui::IsTellRecipient(name) ? std::string{name} : std::string{};
    }
    auto Contains(const xiui::ui::rect& rect, const float x, const float y) -> bool
    { return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height; }

    auto NextDraft(const std::string_view text, size_t& at) -> void
    {
        if (const auto token = xiui::ChatTokenSize(text, at); token != 0) at += token;
        else xiui::NextUtf8(text, at);
    }

    auto FadeColor(const D3DCOLOR color, const float opacity) -> D3DCOLOR
    { return (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * opacity) << 24); }

    auto FadeText(const xiui::module_text_profile& profile, const float opacity) -> xiui::module_text_profile
    {
        auto result = xiui::TintTextProfile(profile, FadeColor(0xFFFFFFFF, opacity));
        result.style.shadow.color = FadeColor(result.style.shadow.color, opacity);
        result.style.stroke.color = FadeColor(result.style.stroke.color, opacity);
        result.style.glow.color = FadeColor(result.style.glow.color, opacity);
        result.style.sheen.color = FadeColor(result.style.sheen.color, opacity);
        return result;
    }
}

namespace xiui
{
    auto chat_module::DescribeColors(const chat_palette& palette) const -> std::string
    {
        std::string report{"[XIUI+] Captured chat colors:"};
        std::vector<std::string> entries;
        const auto& lines = this->History().Lines();
        for (auto line = lines.rbegin(); line != lines.rend() && entries.size() < 24; ++line)
        {
            char text[96]{};
            std::snprintf(text, sizeof(text), " mode=%u captured=%08X palette=%08X inline=", line->mode, line->color, palette.modes[line->mode & 255]);
            std::string entry{text};
            for (const auto& span : line->spans)
            {
                std::snprintf(text, sizeof(text), "%d,", span.color_index);
                if (entry.size() < 180) entry += text;
            }
            if (std::find(entries.begin(), entries.end(), entry) == entries.end()) entries.push_back(std::move(entry));
        }
        for (const auto& entry : entries) report += "\n" + entry;
        return report;
    }

    chat_module::chat_module(const bool secondary) : secondary_(secondary)
    {
        this->editor_.SetConstraint([this](std::string_view text) {
            const auto [used, maximum] = this->InputUsage(text); return used <= maximum;
        });
        if (secondary)
        {
            this->state_.enabled = false;
            this->state_.bounds = {-1, -1, 560, 260};
            this->source_ = 1;
            this->active_tab_ = static_cast<int>(chat_tab::window2);
            this->history_view_.tab = static_cast<size_t>(chat_history_tab::window2);
        }
    }

    auto chat_module::AttachSecondWindow(chat_module& second) -> void
    {
        this->second_window_ = &second;
        second.shared_model_ = &this->model_;
        second.parent_ = this;
    }

    auto chat_module::InitializeNativeWindows(const std::optional<unsigned> count) -> bool
    {
        if (this->native_windows_initialized_ || !count || *count < 1 || *count > 2 || this->second_window_ == nullptr) return false;
        this->second_window_->state_.enabled = *count == 2;
        if (*count == 2) this->second_window_->EnsureWindowDefaults();
        this->native_windows_initialized_ = true;
        return true;
    }

    auto chat_module::WindowLayout(void) const -> std::optional<chat_window_layout>
    {
        if (this->secondary_ || this->second_window_ == nullptr) return {};
        if (this->source_ == 2 && !this->second_window_->state_.enabled) return chat_window_layout::combined;
        if (this->source_ == 0 && this->second_window_->source_ == 1) return chat_window_layout::follow_game;
        return {};
    }

    auto chat_module::SetWindowLayout(const chat_window_layout layout, const std::optional<unsigned> nativeCount) -> void
    {
        if (this->secondary_ || this->second_window_ == nullptr) return;
        const bool combined = layout == chat_window_layout::combined;
        const bool countReady = nativeCount && (*nativeCount == 1 || *nativeCount == 2);
        this->native_windows_initialized_ = combined || countReady;
        this->second_window_->state_.enabled = !combined && countReady && *nativeCount == 2;
        if (this->second_window_->state_.enabled) this->second_window_->EnsureWindowDefaults();
        this->source_ = combined ? 2 : 0;
        this->second_window_->source_ = 1;
        for (auto* window : {this, this->second_window_})
        {
            window->active_tab_ = static_cast<int>(window->source_ == 0 ? chat_tab::window1 : window->source_ == 1 ? chat_tab::window2 : chat_tab::all);
            window->cache_revision_ = UINT64_MAX;
            window->message_entrances_.clear();
            window->ResetCollapse();
        }
    }

    auto chat_module::EnsureWindowDefaults(void) -> void
    {
        if (!this->secondary_ || this->parent_ == nullptr) return;
        if (!this->defaults_initialized_)
        {
            this->ApplyWindowSettings(this->parent_->CaptureWindowSettings());
            this->visible_tabs_ = this->parent_->visible_tabs_;
            this->channel_ = this->parent_->channel_;
            this->defaults_initialized_ = true;
        }
        if (this->MatchingWindow1())
        {
            auto settings = this->parent_->CaptureWindowSettings();
            const auto width = settings.width, height = settings.height;
            if (!this->independent_settings_) this->independent_settings_ = this->CaptureWindowSettings();
            else
            {
                // Keep viewport clamps until the source dimensions change.
                if (width == this->matched_width_) settings.width = this->state_.bounds.width;
                if (height == this->matched_height_) settings.height = this->state_.bounds.height;
            }
            this->ApplyWindowSettings(settings);
            this->matched_width_ = width; this->matched_height_ = height;
        }
        else if (this->independent_settings_)
        {
            this->ApplyWindowSettings(*this->independent_settings_);
            this->independent_settings_.reset();
        }
    }

    auto chat_module::CaptureWindowSettings(void) const -> window_settings
    {
        return {this->state_.bounds.width, this->state_.bounds.height, this->container_, this->text_, this->chrome_, this->line_spacing_,
            this->message_animation_, this->message_animation_speed_, this->collapse_mode_, this->collapse_delay_, this->collapse_min_lines_,
            this->collapse_max_lines_, this->height_limits_lines_, this->timestamp_display_, this->timestamp_color_enabled_, this->timestamp_color_, this->background_};
    }

    auto chat_module::ApplyWindowSettings(const window_settings& settings) -> void
    {
        if (this->state_.bounds.width != settings.width || this->state_.bounds.height != settings.height ||
            this->container_.padding != settings.container.padding || this->line_spacing_ != settings.line_spacing ||
            this->text_.style.font != settings.text.style.font || this->text_.style.scale != settings.text.style.scale ||
            this->text_.style.letter_spacing != settings.text.style.letter_spacing || this->timestamp_display_ != settings.timestamp_display ||
            this->collapse_mode_ != settings.collapse_mode || this->collapse_delay_ != settings.collapse_delay ||
            this->collapse_min_lines_ != settings.collapse_min_lines || this->collapse_max_lines_ != settings.collapse_max_lines ||
            this->height_limits_lines_ != settings.height_limits_lines) this->ResetCollapse();
        if (this->message_animation_ != settings.message_animation) this->message_entrances_.clear();
        if (this->state_.bounds.width != settings.width || this->state_.bounds.height != settings.height) this->state_.content_bounds = {};
        this->state_.bounds.width = settings.width;
        this->state_.bounds.height = settings.height;
        this->container_ = settings.container;
        this->background_ = settings.background;
        this->text_ = settings.text; this->chrome_ = settings.chrome;
        this->line_spacing_ = settings.line_spacing;
        this->message_animation_ = settings.message_animation;
        this->message_animation_speed_ = settings.message_animation_speed;
        this->collapse_mode_ = settings.collapse_mode;
        this->collapse_delay_ = settings.collapse_delay;
        this->collapse_min_lines_ = settings.collapse_min_lines;
        this->collapse_max_lines_ = settings.collapse_max_lines;
        this->height_limits_lines_ = settings.height_limits_lines;
        this->timestamp_display_ = settings.timestamp_display;
        this->timestamp_color_enabled_ = settings.timestamp_color_enabled;
        this->timestamp_color_ = settings.timestamp_color;
    }

    auto chat_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_.style = theme.body_text;
        this->text_.style.top_color = this->text_.style.bottom_color = 0xFFFFFFFF;
        this->chrome_.style = theme.meter_text;
        this->chrome_.style.scale = 0.86f;
    }

    auto chat_module::ResetDefaults(const ui::theme& theme) -> void
    {
        this->Focus(false);
        this->input_reveal_ = this->destination_reveal_ = 0; this->input_animation_tick_ = 0;
        this->channel_picker_open_ = this->destination_hovered_ = false; this->channel_pointer_press_ = -2;
        this->reply_destination_.clear();
        this->message_animation_ = 1; this->message_entrances_.clear();
        this->message_animation_speed_ = 100;
        this->collapse_mode_ = 0; this->collapse_delay_ = 5;
        this->collapse_min_lines_ = 0; this->collapse_max_lines_ = 12;
        this->height_limits_lines_ = false;
        this->shared_windows_ = false;
        this->ResetCollapse();
        this->state_.enabled = !this->secondary_;
        this->state_.bounds.width = 560.0f;
        this->state_.bounds.height = 260.0f;
        this->defaults_initialized_ = false;
        this->match_window1_ = false; this->independent_settings_.reset();
        this->container_ = {};
        this->background_ = true;
        this->text_ = {};
        this->ApplyTextDefaults(theme);
        this->channel_ = game_channel;
        this->source_ = this->secondary_ ? 1 : 0;
        this->active_tab_ = static_cast<int>(this->secondary_ ? chat_tab::window2 : chat_tab::window1);
        this->visible_tabs_ = {};
        this->timestamp_display_ = 0;
        this->input_counter_ = true;
        this->timestamp_color_enabled_ = true;
        this->timestamp_color_ = 0xFFB0B0B0;
        this->line_spacing_ = 2.0f;
        if (!this->secondary_) { this->alerts_ = {}; this->colors_ = {}; }
        this->cache_revision_ = UINT64_MAX;
    }

    auto chat_module::Load(const settings_store& store) -> void
    {
        this->native_windows_initialized_ = store.ReadBool(this->Id(), "native_windows_initialized", true);
        this->independent_settings_.reset();
        this->match_window1_ = this->secondary_ && store.ReadBool(this->Id(), "match_window1", false);
        if (!this->secondary_) { this->alerts_.Load(store); this->colors_.Load(store); }
        if (!this->secondary_) this->shared_windows_ = store.ReadBool("chat", "shared_windows", false);
        this->state_.enabled = store.ReadBool(this->Id(), "enabled", !this->secondary_);
        this->state_.bounds = store.ReadRect(this->secondary_ ? "layout.chat2" : "layout.chat", this->state_.bounds);
        this->container_ = {.opacity = std::clamp(store.ReadFloat(this->Id(), "opacity", 1.0f), 0.0f, 1.0f),
            .padding = std::clamp(store.ReadFloat(this->Id(), "padding", 8.0f), 4.0f, 24.0f),
            .border = store.ReadBool(this->Id(), "borders", true)};
        this->background_ = store.ReadBool(this->Id(), "background", true);
        this->line_spacing_ = std::clamp(store.ReadFloat(this->Id(), "line_spacing", 2.0f), 0.0f, 10.0f);
        this->message_animation_ = std::clamp(store.ReadInt(this->Id(), "message_animation", 1), 0, 2);
        const auto animationSpeed = store.ReadFloat(this->Id(), "message_animation_speed", 100);
        this->message_animation_speed_ = std::isfinite(animationSpeed) ? std::clamp(animationSpeed, 25.0f, 300.0f) : 100;
        this->message_entrances_.clear();
        this->collapse_mode_ = std::clamp(store.ReadInt(this->Id(), "collapse_mode", 0), 0, 2);
        const auto collapseValue = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = store.ReadFloat(this->Id(), key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->collapse_delay_ = collapseValue("collapse_delay", 5, 3, 30);
        this->collapse_max_lines_ = collapseValue("collapse_max_lines", 12, 1, 40);
        this->height_limits_lines_ = store.ReadBool(this->Id(), "height_limits_lines", false);
        this->collapse_min_lines_ = std::min(this->collapse_max_lines_, collapseValue("collapse_min_lines", 0, 0, 40));
        this->ResetCollapse();
        this->input_counter_ = store.ReadBool(this->Id(), "input_counter", true);
        this->timestamp_display_ = std::clamp(store.ReadInt(this->Id(), "timestamp_display", store.ReadBool(this->Id(), "timestamps", false) ? 2 : 0), 0, 3);
        this->timestamp_color_enabled_ = store.ReadInt(this->Id(), "timestamp_color_version", 0) < 1 || store.ReadBool(this->Id(), "timestamp_color_enabled", true);
        this->timestamp_color_ = store.ReadArgb(this->Id(), "timestamp_color", 0xFFB0B0B0);
        const bool legacy = store.ReadInt(this->Id(), "layout_version", 0) < 3;
        this->source_ = legacy ? (this->secondary_ ? 1 : 0) : std::clamp(store.ReadInt(this->Id(), "source", this->secondary_ ? 1 : 0), 0, 2);
        this->active_tab_ = static_cast<int>(this->source_ == 0 ? chat_tab::window1 : this->source_ == 1 ? chat_tab::window2 : chat_tab::all);
        this->visible_tabs_[0] = !legacy && store.ReadBool(this->Id(), "tab_window1", false);
        this->visible_tabs_[1] = !legacy && store.ReadBool(this->Id(), "tab_window2", false);
        this->channel_ = std::clamp(store.ReadInt(this->Id(), "channel", game_channel), 0, static_cast<int>(std::size(channels)) - 1);
        if (store.ReadInt(this->Id(), "channel_version", 0) < 1 && this->channel_ == 0) this->channel_ = game_channel;
        this->text_ = LoadTextProfile(store, this->secondary_ ? "chat2.text" : "chat.text", this->text_);
        this->chrome_ = LoadTextProfile(store, this->secondary_ ? "chat2.chrome" : "chat.chrome", this->chrome_);
        this->cache_revision_ = UINT64_MAX;
        if (this->secondary_)
        {
            const auto& bounds = this->state_.bounds;
            this->defaults_initialized_ = store.ReadBool(this->Id(), "defaults_initialized", bounds.x >= 0 && bounds.y >= 0);
            if (store.ReadInt(this->Id(), "layout_version", 0) < 4 && this->parent_ != nullptr)
            {
                const auto& main = this->parent_->state_.bounds;
                if (bounds.height == 180 && bounds.width == main.width && bounds.x == main.x && bounds.y == main.y - 212)
                {
                    this->state_.bounds.height = main.height;
                    this->state_.bounds.y = main.y - main.height - 32;
                }
            }
        }
        if (this->MatchingWindow1()) this->EnsureWindowDefaults();
        if (!this->state_.enabled) this->Focus(false);
    }

    auto chat_module::LoadScale(const settings_store& store) -> void
    {
        hud_module::LoadScale(store);
        const auto scale = this->state_.scale;
        if (std::abs(scale.x - scale.y) < .0001f) return;
        // Earlier X/Y stretching represented a window resize; retain that footprint.
        if (this->independent_settings_)
        {
            this->independent_settings_->width = std::max(this->state_.minimum_width, this->independent_settings_->width * scale.x);
            this->independent_settings_->height = std::max(this->state_.minimum_height, this->independent_settings_->height * scale.y);
            this->independent_settings_->height_limits_lines = true;
        }
        else
        {
            const auto original = this->state_.bounds;
            this->state_.bounds.width = std::max(this->state_.minimum_width, original.width * scale.x);
            this->state_.bounds.height = std::max(this->state_.minimum_height, original.height * scale.y);
            this->OnBoundsResized(original);
        }
        this->state_.scale = this->state_.scale_checkpoint = {};
        this->state_.content_bounds = {};
        this->cache_revision_ = UINT64_MAX;
    }

    auto chat_module::OnBoundsResized(const ui::rect& original) -> void
    {
        if (this->state_.bounds.height != original.height) this->height_limits_lines_ = true;
        this->ResetCollapse();
        this->cache_revision_ = UINT64_MAX;
    }

    auto chat_module::Save(const settings_store& store) const -> void
    {
        if (!this->secondary_) store.WriteBool(this->Id(), "native_windows_initialized", this->native_windows_initialized_);
        const auto window = this->independent_settings_.value_or(this->CaptureWindowSettings());
        auto bounds = this->state_.bounds;
        bounds.width = window.width; bounds.height = window.height;
        if (!this->secondary_) { this->alerts_.Save(store); this->colors_.Save(store); }
        if (!this->secondary_) store.WriteBool("chat", "shared_windows", this->shared_windows_);
        if (this->secondary_) store.WriteBool(this->Id(), "match_window1", this->match_window1_);
        store.WriteBool(this->Id(), "enabled", this->state_.enabled);
        store.WriteRect(this->secondary_ ? "layout.chat2" : "layout.chat", bounds);
        store.WriteFloat(this->Id(), "opacity", window.container.opacity);
        store.WriteFloat(this->Id(), "padding", window.container.padding);
        store.WriteBool(this->Id(), "background", window.background);
        store.WriteBool(this->Id(), "borders", window.container.border);
        store.WriteFloat(this->Id(), "line_spacing", window.line_spacing);
        store.WriteInt(this->Id(), "message_animation", window.message_animation);
        store.WriteFloat(this->Id(), "message_animation_speed", window.message_animation_speed);
        store.WriteInt(this->Id(), "collapse_mode", window.collapse_mode);
        store.WriteFloat(this->Id(), "collapse_delay", window.collapse_delay);
        store.WriteFloat(this->Id(), "collapse_min_lines", window.collapse_min_lines);
        store.WriteFloat(this->Id(), "collapse_max_lines", window.collapse_max_lines);
        store.WriteBool(this->Id(), "height_limits_lines", window.height_limits_lines);
        store.WriteBool(this->Id(), "input_counter", this->input_counter_);
        store.WriteInt(this->Id(), "timestamp_display", window.timestamp_display);
        store.WriteBool(this->Id(), "timestamp_color_enabled", window.timestamp_color_enabled);
        store.WriteInt(this->Id(), "timestamp_color_version", 1);
        store.WriteArgb(this->Id(), "timestamp_color", window.timestamp_color);
        store.WriteBool(this->Id(), "timestamps", window.timestamp_display >= 2);
        store.WriteInt(this->Id(), "active_tab", this->active_tab_);
        store.WriteInt(this->Id(), "source", this->source_);
        store.WriteBool(this->Id(), "tab_window1", this->visible_tabs_[0]);
        store.WriteBool(this->Id(), "tab_window2", this->visible_tabs_[1]);
        store.WriteInt(this->Id(), "layout_version", 4);
        if (this->secondary_) store.WriteBool(this->Id(), "defaults_initialized", this->defaults_initialized_);
        store.WriteInt(this->Id(), "channel", this->channel_);
        store.WriteInt(this->Id(), "channel_version", 1);
        SaveTextProfile(store, this->secondary_ ? "chat2.text" : "chat.text", window.text);
        SaveTextProfile(store, this->secondary_ ? "chat2.chrome" : "chat.chrome", window.chrome);
    }

    auto chat_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Window visibility, default send channel and the optional second window.", "general"},
            {"Messages", "Native message filters, timestamps and optional window tabs.", "messages"},
            {"Layout", "Window dimensions, inner padding and message spacing.", "layout"},
            {"Appearance", "Chat background, frame and opacity using the current window theme.", "appearance"},
            {"Text", "Message text size, colors, effects and offsets.", "text"},
            {"Sounds", "Choose a sound for each chat type and keyword alerts. Shared by both windows.", "sounds"},
            {"Colors", "XIUI+ colors override your game settings colors. Choose Game to restore native colors.", "colors"},
            {"Native Filters", "The game's Chat Filters and per-window Log routing. Changes apply to the native game and XIUI+.", "native-filters"},
            {"Animation", "How newly arriving messages enter this chat window.", "animation"}};
        return sections;
    }

    auto chat_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= this->ContainerOptionStart()) return section == 3;
        if (this->secondary_ && index == this->MatchWindow1Option()) return section == 0;
        if (index >= this->CollapseOptionStart()) return section == 2;
        if (index == this->AnimationOptionStart() + 2) return !this->secondary_ && section == 0;
        if (index >= this->AnimationOptionStart()) return section == 8;
        if (index >= this->NativeFiltersStart()) return section == 7 && this->native_filters_ != nullptr && this->native_filters_->Visible(index - this->NativeFiltersStart(), this->secondary_);
        if (index >= this->ColorsOptionStart()) return section == 6;
        if (index >= this->SoundsOptionStart()) return section == 5;
        const size_t textStart = this->secondary_ ? 10 : 12;
        if (index >= textStart) return section == 4;
        if (index == 1 || index == 2 || index == 4 || index == 6) return section == 2;
        if (index == 3) return section == 3;
        if (index == 0 || (!this->secondary_ && (index == 7 || index == 11))) return section == 0;
        return section == 1;
    }

    auto chat_module::ContentOption(const size_t index) -> module_option
    {
        this->EnsureWindowDefaults();
        if (this->secondary_ && index == this->MatchWindow1Option())
        {
            auto option = module_option::Toggle("Match Window 1 settings", "Follow Window 1 size, appearance, text, timestamps and animation. Filters, tabs, visibility and position stay independent. Turn off to restore this window's settings.", &this->match_window1_).Ordered(-40);
            option.available = this->parent_ != nullptr;
            return option;
        }
        auto option = this->WindowOption(index);
        if (this->MatchingWindow1() && this->InheritedWindowOption(index))
        {
            option.available = false;
            option.description = "Matches Window 1. Turn off Match Window 1 settings under General to customize this window.";
        }
        return option;
    }

    auto chat_module::InheritedWindowOption(const size_t index) const -> bool
    {
        return index < this->ContentOptionCount() && index != 0 && index != this->MatchWindow1Option() && !(index >= 7 && index <= 9) &&
            !(index >= this->NativeFiltersStart() && index < this->AnimationOptionStart());
    }

    auto chat_module::WindowOption(const size_t index) -> module_option
    {
        if (index == this->ContainerOptionStart())
            return module_option::Toggle("Background", "Show the themed background behind normal and expanded chat.",
                &this->background_, module_option_group::appearance).Ordered(440);
        if (index == this->ContainerOptionStart() + 1)
            return module_option::Toggle("Frame", "Show the current theme's window border around normal and expanded chat.",
                &this->container_.border, module_option_group::appearance).Ordered(470);
        if (index == this->CollapseOptionStart() + (this->secondary_ ? 4 : 5))
            return module_option::Toggle("Use window height for maximum", "Resizing the window enables this automatically. Turn off to follow the maximum line setting again.",
                &this->height_limits_lines_).Ordered(314).VisibleWhen(this->collapse_mode_ != 1);
        if (!this->secondary_ && index == this->CollapseOptionStart() + 4)
            return module_option::Toggle("Shared window stack", "Keep the gap between stacked chat windows as the lower window collapses. Saved positions are retained.",
                &this->shared_windows_).Ordered(320, true).VisibleWhen(this->second_window_ != nullptr && this->second_window_->state_.enabled);
        if (index >= this->CollapseOptionStart() && index < this->CollapseOptionStart() + 4)
        {
            const auto setting = index - this->CollapseOptionStart();
            if (setting == 0)
            {
                static constexpr const char* modes[]{"Follow game", "Off", "Custom"};
                return module_option::Dropdown("Reactive window sizing", "Follow this native window's sizing, line limits and resize delay, keep a fixed height, or customize XIUI+. History and typing keep the window open.",
                    &this->collapse_mode_, modes, std::size(modes)).Ordered(310, true);
            }
            auto option = setting == 1 ? module_option::Slider("Resize delay (seconds)", "After no new messages, fade out the oldest message at this interval. Wrapped lines stay together.", &this->collapse_delay_, 3, 30, 1) :
                setting == 2 ? module_option::Slider("Minimum lines", "Keep this many lines visible after the window collapses. Zero allows it to close completely.", &this->collapse_min_lines_, 0, 40, 1) :
                module_option::Slider("Maximum lines", "Limit expanded live messages, within the configured window height. Full history remains available.", &this->collapse_max_lines_, 1, 40, 1);
            return option.Ordered(310 + static_cast<int>(setting)).VisibleWhen(this->collapse_mode_ == 2);
        }
        if (index == this->AnimationOptionStart() + 2)
            return module_option::Toggle("Character counter", "Show the current length and game limit on the right of the typing row. The limit is always enforced.", &this->input_counter_);
        if (index == this->AnimationOptionStart() + 1)
        {
            auto option = module_option::Slider("Animation speed (%)", "Higher is faster. 100% keeps the original speed; 50% takes twice as long.",
                &this->message_animation_speed_, 25, 300, 5);
            return option.VisibleWhen(this->message_animation_ != 0);
        }
        if (index == this->AnimationOptionStart())
        {
            static constexpr const char* animations[]{"Off", "Fade + slide up", "Fade + slide right"};
            return module_option::Dropdown("New messages", "Scroll the message stack upward as new messages fade in, with optional sliding from the left.",
                &this->message_animation_, animations, std::size(animations));
        }
        if (index >= this->NativeFiltersStart() && this->native_filters_ != nullptr)
            return this->native_filters_->Option(index - this->NativeFiltersStart(), this->secondary_);
        if (index == this->TimestampColorOptionStart())
            return module_option::Toggle("Separate timestamp color", "Use an independent timestamp color in this window; messages retain their channel colors.", &this->timestamp_color_enabled_, module_option_group::colors).VisibleWhen(this->timestamp_display_ != 1);
        if (index == this->TimestampColorOptionStart() + 1)
        {
            auto option = module_option::Color("Timestamp color", "Color for timestamps in this window's normal and expanded chat history.", &this->timestamp_color_, module_option_group::colors);
            return option.VisibleWhen(this->timestamp_display_ != 1 && this->timestamp_color_enabled_);
        }
        if (index >= this->ColorsOptionStart() && index < this->OptionCount())
        {
            const auto type = index - this->ColorsOptionStart();
            return module_option::OverrideColor(chat_types[type].label, "XIUI+ overrides your game settings colors. Game restores native colors.",
                &this->Colors().enabled[type], &this->Colors().colors[type]);
        }
        if (index >= this->SoundsOptionStart())
        {
            const auto soundIndex = index - this->SoundsOptionStart();
            if (soundIndex == 0)
                return module_option::Text("Keywords", "Comma-separated words or phrases. Ignores case; blank disables keyword alerts.", &this->Alerts().keywords, 500);
            if (soundIndex <= std::size(chat_types))
                return SoundOption(chat_types[soundIndex - 1].label, "Choose a sound or Off. Use the play icons in the list to preview.", &this->Alerts().channels[soundIndex - 1]);
            if (soundIndex == std::size(chat_types) + 1)
                return SoundOption("Keyword sound", "Used for keyword matches. Play icons preview without changing your choice.", &this->Alerts().keyword_sound);
            return {};
        }
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show XIUI+ chat. Native Menus controls the native chat windows.", &this->state_.enabled).Ordered(-50);
        case 1: return ModuleWidthOption(this->state_);
        case 2: return module_option::Slider("Height", "Resize the chat viewport without changing text size. The top-right corner resizes the window in edit mode and while the HUD is locked.", &this->state_.bounds.height, this->state_.minimum_height, 900.0f, 1.0f, module_option_group::appearance).Ordered(101);
        case 3: return module_option::Slider("Opacity", "Background and frame opacity. Message text keeps its own opacity.", &this->container_.opacity, 0.0f, 1.0f, 0.05f, module_option_group::appearance).Ordered(450).VisibleWhen(this->background_ || this->container_.border);
        case 4: return module_option::Slider("Padding", "Space inside the chat frame.", &this->container_.padding, 4.0f, 24.0f, 1.0f, module_option_group::appearance).Ordered(402);
        case 5:
        {
            static constexpr const char* modes[]{"Follow game", "Off", "HH:MM", "HH:MM:SS"};
            return module_option::Dropdown("Timestamps", "Follow the game's Windows timestamp setting, or override it for this XIUI+ window. Addon timestamps are not duplicated.",
                &this->timestamp_display_, modes, std::size(modes));
        }
        case 6: return module_option::Slider("Line spacing", "Extra space between wrapped lines.", &this->line_spacing_, 0.0f, 10.0f, 1.0f, module_option_group::appearance).Ordered(300, true);
        default: break;
        }
        if (!this->secondary_ && index == 7)
            return module_option::Choice("Send channel", "Game follows the native chat menu's selected channel. Other choices override it for ordinary messages; typed /commands always take priority.", &this->channel_, channels, std::size(channels));
        constexpr static const char* sources[]{"Window 1", "Window 2", "All"};
        const size_t first = this->secondary_ ? 7 : 8;
        switch (index - first)
        {
        case 0: return module_option::Choice("Messages", "Follow the game's Window 1, Window 2, or their union.", &this->source_, sources, 3);
        case 1: return module_option::Toggle("Window 1 tab", "Optional tab above this window. Hidden by default.", &this->visible_tabs_[0]);
        case 2: return module_option::Toggle("Window 2 tab", "Optional tab above this window. Hidden by default.", &this->visible_tabs_[1]);
        case 3:
            if (!this->secondary_)
            {
                auto option = module_option::Toggle("Second window", "Starts with Window 1 size and style; move and resize independently.", this->second_window_ != nullptr ? &this->second_window_->state_.enabled : nullptr);
                option.available = this->second_window_ != nullptr;
                return option;
            }
            break;
        }
        return TextProfileOption(index - (this->secondary_ ? 10 : 12), this->text_);
    }

    auto chat_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (!this->secondary_ && index == 11) this->native_windows_initialized_ = true;
        if (this->secondary_ && index == 0 && this->parent_ != nullptr) this->parent_->native_windows_initialized_ = true;
        if (this->secondary_ && index == this->MatchWindow1Option())
        {
            this->EnsureWindowDefaults();
            this->CancelPointer(); this->ResetCollapse(); this->cache_revision_ = UINT64_MAX;
            return;
        }
        if (this->MatchingWindow1() && this->InheritedWindowOption(index)) { this->EnsureWindowDefaults(); return; }
        if (index == 2) { this->height_limits_lines_ = true; this->ResetCollapse(); }
        if (index == this->CollapseOptionStart() || index == this->CollapseOptionStart() + 3) this->height_limits_lines_ = false;
        if (index >= this->CollapseOptionStart() && index < this->ContainerOptionStart())
        {
            if (index == this->CollapseOptionStart() + 2) this->collapse_max_lines_ = std::max(this->collapse_max_lines_, this->collapse_min_lines_);
            else this->collapse_min_lines_ = std::min(this->collapse_min_lines_, this->collapse_max_lines_);
            this->ResetCollapse();
        }
        if ((index >= this->AnimationOptionStart() && index < this->AnimationOptionStart() + 2) || index == (this->secondary_ ? 7u : 8u) || !this->WindowVisible()) this->message_entrances_.clear();
        if (!this->WindowVisible() && this->second_window_ != nullptr) this->second_window_->message_entrances_.clear();
        if (index >= this->NativeFiltersStart() && index < this->AnimationOptionStart() && this->native_filters_ != nullptr) this->native_filters_->Apply(index - this->NativeFiltersStart(), this->secondary_);
        if (index == this->SoundsOptionStart()) this->Alerts().UpdateKeywords();
        if (!this->secondary_ && index == 11 && this->second_window_ != nullptr && this->second_window_->state_.enabled)
            this->second_window_->EnsureWindowDefaults();
        if (index == (this->secondary_ ? 7u : 8u))
            this->active_tab_ = static_cast<int>(this->source_ == 0 ? chat_tab::window1 : this->source_ == 1 ? chat_tab::window2 : chat_tab::all);
        if (!this->state_.enabled) this->Focus(false);
        if (this->second_window_ != nullptr && this->second_window_->MatchingWindow1()) this->second_window_->EnsureWindowDefaults();
    }

    auto chat_module::PreviewChoice(const size_t index, const size_t choice) -> bool
    {
        return index > this->SoundsOptionStart() && index < this->ColorsOptionStart() && choice > 0 &&
            choice <= std::size(chat_sound_values) && this->sounds_ != nullptr && this->sounds_->Play(chat_sound_values[choice - 1]);
    }

    auto chat_module::Focus(const bool focused, const bool preserveExpandedHistory) -> void
    {
        if (focused && !this->Focused()) this->editor_.RestartHistory();
        if (focused || !preserveExpandedHistory || !this->HistoryExpanded())
            this->history_view_.focused = this->history_view_.expanded = false;
        this->history_view_.pointer = {-1, -1};
        this->controller_history_ = false;
        this->controller_composition_ = this->controller_channel_adjusting_ = false;
        this->focused_ = focused && this->state_.enabled && !this->secondary_;
        if (!this->focused_)
        {
            this->channel_picker_open_ = false;
            this->editor_.ResetCompletion();
            this->reply_recipient_.clear();
            this->reply_destination_.clear();
        }
    }

    auto chat_module::Capture(runtime_state& state) const -> void
    {
        state.chat_focused = this->Focused();
        state.chat_caret = static_cast<uint32_t>(this->editor_.Caret());
        ::strncpy_s(state.chat_draft, this->editor_.Text().c_str(), _TRUNCATE);
    }

    auto chat_module::UpdateInputAnimation(const uint64_t now) -> void
    {
        const float elapsed = this->input_animation_tick_ != 0 && now >= this->input_animation_tick_ ?
            static_cast<float>(std::min<uint64_t>(now - this->input_animation_tick_, input_animation_ms)) / static_cast<float>(input_animation_ms) : 0;
        this->input_animation_tick_ = now;
        this->input_reveal_ = this->secondary_ ? 0 : std::clamp(this->input_reveal_ + (this->Focused() ? elapsed : -elapsed), 0.0f, 1.0f);
        this->destination_reveal_ = this->input_reveal_;
    }

    auto chat_module::Restore(const runtime_state& state) -> void
    {
        this->editor_.Set(std::string{state.chat_draft, strnlen_s(state.chat_draft, sizeof(state.chat_draft))}, state.chat_caret);
        this->Focus(state.chat_focused != FALSE);
    }

    auto chat_module::Clear(void) -> void
    {
        this->ResetCollapse();
        this->message_entrances_.clear();
        this->model_.Clear();
        this->tabs_ = {};
        this->history_view_.scroll = {};
        this->scrollbar_until_ = 0;
        if (this->second_window_ != nullptr) { this->second_window_->tabs_ = {}; this->second_window_->scrollbar_until_ = 0; this->second_window_->message_entrances_.clear(); this->second_window_->ResetCollapse(); }
    }

    auto chat_module::SetDialogueActive(const bool active) -> void
    {
        this->dialogue_active_ = active;
        if (this->second_window_ != nullptr) this->second_window_->dialogue_active_ = active;
        if (!active && this->shared_model_ == nullptr) this->model_.DismissDialoguePrompt();
    }

    auto chat_module::DialoguePromptActive(const uint64_t sequence) const -> bool
    {
        return this->dialogue_active_ && !this->HistoryExpanded() && this->tabs_[this->active_tab_].scroll == 0 &&
            sequence != 0 && sequence == this->History().DialoguePromptSequence();
    }

    auto chat_module::Receive(IAshitaCore* core, const chat_event& event, const bool audible) -> void
    {
        if (this->shared_model_ != nullptr) return;
        const auto duplicate = !this->model_.Lines().empty() && this->model_.Lines().back().sequence == event.sequence;
        const uint32_t previousWindows = duplicate ? this->model_.Lines().back().windows : 0;
        this->model_.Add(event, duplicate ? std::string{} : ExpandChatText(core != nullptr ? core->GetChatManager() : nullptr, event.text));
        if (!duplicate && (event.mode & 255) == 12 && !this->model_.Lines().empty() &&
            this->model_.Lines().back().sequence == event.sequence)
            this->ObserveTeller(IncomingTeller(this->model_.Lines().back()));
        if (audible && !this->model_.Lines().empty() && this->model_.Lines().back().sequence == event.sequence)
        {
            const auto now = ::GetTickCount64();
            this->TrackMessageEntrance(event, previousWindows, now);
            this->TrackCollapse(event, previousWindows, now);
            if (this->second_window_ != nullptr) this->second_window_->TrackMessageEntrance(event, previousWindows, now);
            if (this->second_window_ != nullptr) this->second_window_->TrackCollapse(event, previousWindows, now);
        }
        if (audible && !duplicate && this->state_.enabled && this->sounds_ != nullptr && !this->model_.Lines().empty() &&
            this->model_.Lines().back().sequence == event.sequence)
        {
            const auto sound = this->alerts_.SoundFor(this->model_.Lines().back());
            if (sound > 0 && sound <= static_cast<int>(std::size(chat_sound_values))) this->sounds_->Play(chat_sound_values[sound - 1]);
        }
        this->MarkUnread(event, previousWindows);
        if (this->second_window_ != nullptr) this->second_window_->MarkUnread(event, previousWindows);
    }

    auto chat_module::TrackMessageEntrance(const chat_event& event, const uint32_t previousWindows, const uint64_t now) -> void
    {
        if (this->message_animation_ == 0 || !this->WindowVisible() || this->HistoryExpanded() || this->CurrentScroll() != 0) return;
        const auto tab = static_cast<chat_tab>(this->active_tab_);
        if (!ChatTabMatches(tab, event.mode, event.windows) || ChatTabMatches(tab, event.mode, previousWindows)) return;
        std::erase_if(this->message_entrances_, [now, duration = this->MessageAnimationDuration()](const auto& entry) { return now < entry.started || now - entry.started >= duration; });
        if (this->message_entrances_.size() >= 128) this->message_entrances_.pop_front();
        this->message_entrances_.push_back({event.sequence, now});
    }

    auto chat_module::MessageMotion(const uint64_t sequence, const uint64_t now) const -> message_motion
    {
        if (this->message_animation_ == 0) return {};
        const auto found = std::find_if(this->message_entrances_.begin(), this->message_entrances_.end(),
            [sequence](const auto& entry) { return entry.sequence == sequence; });
        if (found == this->message_entrances_.end()) return {};
        const auto eased = MessageProgress(found->started, now, this->MessageAnimationDuration());
        const auto offset = 14 * (1 - eased);
        return {eased, this->message_animation_ == 2 ? -offset : 0};
    }

    auto chat_module::MessageScrollOffset(const uint64_t now, const float lineHeight) const -> float
    {
        if (this->message_animation_ == 0 || this->HistoryExpanded() || this->tabs_[this->active_tab_].scroll != 0) return 0;
        float offset{};
        for (const auto& entry : this->message_entrances_)
        {
            const auto remaining = 1 - MessageProgress(entry.started, now, this->MessageAnimationDuration());
            if (remaining <= 0) continue;
            const auto first = std::lower_bound(this->rows_.begin(), this->rows_.end(), entry.sequence,
                [](const rendered_row& row, const uint64_t sequence) { return row.sequence < sequence; });
            const auto last = std::upper_bound(first, this->rows_.end(), entry.sequence,
                [](const uint64_t sequence, const rendered_row& row) { return sequence < row.sequence; });
            offset += static_cast<float>(last - first) * lineHeight * remaining;
        }
        return offset;
    }

    auto chat_module::MarkUnread(const chat_event& event, const uint32_t previousWindows) -> void
    {
        for (size_t index = 0; index < this->tabs_.size(); ++index)
            if (ChatTabMatches(static_cast<chat_tab>(index), event.mode, event.windows) &&
                !ChatTabMatches(static_cast<chat_tab>(index), event.mode, previousWindows) &&
                (static_cast<int>(index) != this->active_tab_ || this->tabs_[index].scroll > 0)) ++this->tabs_[index].unread;
    }

    auto chat_module::CollapseSettings(void) const -> std::optional<native_chat_window_settings>
    {
        if (this->MatchingWindow1()) return this->parent_->CollapseSettings();
        if (this->collapse_mode_ == 0) return this->native_window_settings_;
        if (this->collapse_mode_ == 1) return std::nullopt;
        return native_chat_window_settings{true, static_cast<uint32_t>(this->collapse_min_lines_),
            static_cast<uint32_t>(this->collapse_max_lines_), static_cast<uint32_t>(this->collapse_delay_)};
    }

    auto chat_module::CollapsePinned(void) const -> bool
    {
        return this->layout_editing_ || this->resizing_ || this->Focused() || this->input_reveal_ > 0 || this->HistoryFocused() ||
            this->tabs_[this->active_tab_].scroll != 0 || (this->dialogue_active_ && this->History().DialoguePromptSequence() != 0);
    }

    auto chat_module::ResetCollapse(void) -> void
    {
        this->collapse_due_ = this->collapsed_through_ = this->collapse_fade_sequence_ = 0;
        this->collapse_begin_ = this->collapse_visible_rows_ = 0;
        this->collapse_height_rows_ = 0; this->collapse_opacity_ = 1;
        this->collapse_active_ = false;
    }

    auto chat_module::TrackCollapse(const chat_event& event, const uint32_t previousWindows, const uint64_t now) -> void
    {
        const auto settings = this->CollapseSettings();
        const auto tab = static_cast<chat_tab>(this->active_tab_);
        if (!settings || !settings->reactive || !ChatTabMatches(tab, event.mode, event.windows) ||
            ChatTabMatches(tab, event.mode, previousWindows)) return;
        this->collapse_due_ = now + settings->resize_seconds * 1000ull;
        this->collapse_fade_sequence_ = 0; this->collapse_opacity_ = 1;
    }

    auto chat_module::UpdateCompactLayout(const ui::design_system& design, const uint64_t now) -> void
    {
        const auto settings = this->CollapseSettings();
        this->collapse_active_ = settings && settings->reactive && !this->CollapsePinned();
        if (!settings || !settings->reactive) return;
        const auto interval = settings->resize_seconds * 1000ull;
        if (!this->collapse_active_)
        {
            this->collapse_due_ = now + interval;
            this->collapse_fade_sequence_ = 0; this->collapse_opacity_ = 1;
            return;
        }
        const auto content = ui::container_renderer::ContentBounds(this->state_.bounds, this->container_);
        this->RebuildRows(design, std::max(1.0f, content.width - 14));
        const auto lineHeight = std::max(1.0f, design.TextHeight(ChatThemeStyle(design, this->text_.style)) + this->line_spacing_);
        const auto capacity = std::max<size_t>(1, static_cast<size_t>(content.height / lineHeight));
        const auto maximum = this->height_limits_lines_ ? capacity : std::min<size_t>(capacity, settings->maximum_lines);
        const auto minimum = std::min<size_t>(maximum, settings->minimum_lines);
        const auto floor = this->rows_.size() > maximum ? this->rows_.size() - maximum : 0;
        auto retained = this->rows_.size() > minimum ? this->rows_.size() - minimum : 0;
        if (retained < this->rows_.size())
        {
            const auto sequence = this->rows_[retained].sequence;
            while (retained > 0 && this->rows_[retained - 1].sequence == sequence) --retained;
        }
        const auto first = std::upper_bound(this->rows_.begin(), this->rows_.end(), this->collapsed_through_,
            [](const uint64_t sequence, const rendered_row& row) { return sequence < row.sequence; });
        size_t begin = std::min(retained, std::max(floor, static_cast<size_t>(first - this->rows_.begin())));
        if (this->collapse_due_ == 0) this->collapse_due_ = now + interval;
        this->collapse_fade_sequence_ = 0; this->collapse_opacity_ = 1;
        constexpr uint64_t fadeDuration = 220;
        while (begin < retained && now >= this->collapse_due_)
        {
            const auto sequence = this->rows_[begin].sequence;
            const auto last = std::upper_bound(this->rows_.begin() + begin, this->rows_.end(), sequence,
                [](const uint64_t value, const rendered_row& row) { return value < row.sequence; });
            const auto next = std::min(retained, static_cast<size_t>(last - this->rows_.begin()));
            if (now - this->collapse_due_ < fadeDuration)
            {
                this->collapse_fade_sequence_ = sequence;
                this->collapse_fade_end_ = next;
                this->collapse_opacity_ = 1 - static_cast<float>(now - this->collapse_due_) / fadeDuration;
                this->collapse_height_rows_ = static_cast<float>(this->rows_.size() - begin) -
                    static_cast<float>(next - begin) * (1 - this->collapse_opacity_);
                break;
            }
            this->collapsed_through_ = sequence;
            begin = next;
            this->collapse_due_ += interval;
        }
        this->collapse_begin_ = begin;
        this->collapse_visible_rows_ = this->rows_.size() - begin;
        if (this->collapse_fade_sequence_ == 0) this->collapse_height_rows_ = static_cast<float>(std::max(this->collapse_visible_rows_, minimum));
        if (begin >= retained) this->collapse_due_ = now + interval;
    }

    auto chat_module::CompactBounds(const ui::design_system& design) const -> ui::rect
    {
        auto frame = this->state_.bounds;
        if (!this->collapse_active_ || this->CollapsePinned()) return frame;
        const auto lineHeight = design.TextHeight(ChatThemeStyle(design, this->text_.style)) + this->line_spacing_;
        const auto height = this->collapse_height_rows_ > 0 ? this->collapse_height_rows_ * lineHeight +
            (this->container_.edge_size + this->container_.padding) * 2 : 0;
        frame.height = std::clamp(height, 0.0f, frame.height);
        frame.y += this->state_.bounds.height - frame.height;
        return frame;
    }

    auto chat_module::FrameBounds(const ui::design_system& design) const -> ui::rect
    {
        auto frame = this->CompactBounds(design);
        const auto* owner = this->parent_ != nullptr ? this->parent_ : this;
        const auto* peer = this->secondary_ ? this->parent_ : this->second_window_;
        if (!owner->shared_windows_ || peer == nullptr || !this->stack_peer_visible_ || !peer->WindowVisible() ||
            this->CollapsePinned() || peer->CollapsePinned()) return frame;
        const auto offsets = ChatStackOffsets(this->state_.bounds, peer->state_.bounds, frame.height, peer->CompactBounds(design).height);
        frame.y += offsets[0] * peer->state_.transform.y / this->state_.transform.y;
        return frame;
    }

    auto chat_module::Grip(const ui::design_system& design) const -> ui::rect
    {
        if (this->MatchingWindow1()) return {};
        const auto frame = this->FrameBounds(design);
        return {frame.x + frame.width - 18, frame.y, 18, std::min(18.0f, frame.height)};
    }

    auto chat_module::VisualBounds(const ui::design_system& design) const -> ui::rect
    {
        const auto layout = this->Layout(design);
        const auto frame = this->FrameBounds(design);
        float top = frame.y;
        for (const auto& tab : layout.tabs) if (tab.height > 0) top = std::min(top, tab.y);
        return {frame.x, top, frame.width, frame.y + frame.height - top};
    }

    auto chat_module::LayoutHitTest(const ui::design_system& design, const float x, const float y) const -> bool
    {
        if (this->channel_picker_open_) return true;
        if (this->HistoryExpanded()) return Contains(this->history_view_.viewport, x, y);
        if (Contains(this->FrameBounds(design), x, y)) return true;
        const auto layout = this->Layout(design);
        return std::any_of(layout.tabs.begin(), layout.tabs.end(), [x, y](const auto& tab) { return Contains(tab, x, y); });
    }

    auto chat_module::LayoutSoundTarget(const ui::design_system& design, const float x, const float y, const bool editing) const -> uint32_t
    {
        if (this->HistoryExpanded()) return this->HistorySoundTarget(design, x, y);
        if (!this->WindowVisible() && (!editing || this->secondary_)) return 0;
        if (!this->MatchingWindow1() && Contains(this->Grip(design), x, y)) return 12;
        const auto layout = this->Layout(design);
        if (!editing && this->channel_picker_open_)
        {
            const auto picker = this->ChannelPickerLayout(design);
            for (size_t index = 0; index < picker.rows.size(); ++index)
                if (Contains(picker.rows[index], x, y)) return static_cast<uint32_t>(500 + index);
            return 0;
        }
        if (!editing && layout.destination.height > 0 && Contains(layout.destination, x, y)) return 30;
        if (Contains(layout.completion, x, y) && layout.completion_rows > 0)
        {
            const auto row = static_cast<int>(std::floor((y - layout.completion.y - 24) / layout.completion_row_height));
            const auto first = this->editor_.CompletionIndex() / layout.completion_rows * layout.completion_rows;
            return row >= 0 && static_cast<size_t>(row) < layout.completion_rows && first + row < this->editor_.Completions().size() ?
                static_cast<uint32_t>(100 + first + row) : 0;
        }
        if (!editing)
            for (size_t index = 0; index < layout.tabs.size(); ++index)
                if (Contains(layout.tabs[index], x, y)) return static_cast<uint32_t>(index + 1);
        if (!this->secondary_ && Contains(layout.input, x, y)) return 11;
        if (this->ControllerComposing())
            for (size_t index = 0; index < layout.controller_actions.size(); ++index)
                if (Contains(layout.controller_actions[index], x, y)) return static_cast<uint32_t>(20 + index);
        return 0;
    }

    auto chat_module::InputRowHeight(const ui::design_system& design) const -> float
    {
        return this->secondary_ ? 0 : std::max({26.0f, design.TextHeight(ChatInputStyle(design, this->text_.style)) + 6,
            design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) + 4});
    }

    auto chat_module::Layout(const ui::design_system& design) const -> layout
    {
        layout result{};
        const auto frame = this->FrameBounds(design);
        result.content = ui::container_renderer::ContentBounds(frame, this->container_);
        float x = result.content.x;
        for (size_t window = 0; window < this->visible_tabs_.size(); ++window)
        {
            if (!this->visible_tabs_[window]) continue;
            const size_t tab = static_cast<size_t>(chat_tab::window1) + window;
            const float width = design.MeasureText(chat_tab_names[tab], ChatThemeStyle(design, this->chrome_.style, true)) + 22.0f;
            result.tabs[tab] = {x, frame.y - 28, width, 24};
            x += width + 2;
        }
        const float inputReveal = InputEase(this->input_reveal_);
        const float inputHeight = frame.height <= (this->container_.edge_size + this->container_.padding) * 2 ? 0 :
            std::min(result.content.height, this->InputRowHeight(design) * inputReveal);
        const float channelWidth = this->secondary_ ? 0 : std::min(result.content.width * 0.25f,
            std::max(28.0f, design.MeasureText("WWW", ChatThemeStyle(design, this->chrome_.style)) + 14));
        result.input = {result.content.x, result.content.y + result.content.height - inputHeight, result.content.width, inputHeight};
        result.destination = {result.input.x + 5, result.input.y + 2, std::max(0.0f, channelWidth - 10), std::max(0.0f, inputHeight - 4)};
        result.input_text = {result.input.x + channelWidth + 5, result.input.y + 2,
            std::max(1.0f, result.input.width - channelWidth - 10), std::max(0.0f, inputHeight - 4)};
        if (!this->secondary_ && this->input_counter_)
        {
            const float width = std::min(result.content.width, design.MeasureText("000/000", ChatThemeStyle(design, this->chrome_.style)) + 8);
            result.counter = {result.input.x + result.input.width - width - 5, result.input_text.y, width, result.input_text.height};
            result.input_text.width = std::max(1.0f, result.counter.x - 8 - result.input_text.x);
        }
        result.log = {result.content.x + 2.0f, result.content.y, result.content.width - 14.0f, std::max(1.0f, result.input.y - result.content.y - 5 * inputReveal)};
        if (this->ControllerComposing())
        {
            const float height = std::max(24.0f, design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) + 6);
            const float width = std::max(1.0f, (result.content.width - 8) / 3);
            for (size_t index = 0; index < result.controller_actions.size(); ++index)
                result.controller_actions[index] = {result.content.x + static_cast<float>(index) * (width + 4), result.input.y - height - 4, width, height};
            result.log.height = std::max(1.0f, result.controller_actions[0].y - result.log.y - design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) - 12);
        }
        result.scrollbar = {result.log.x + result.log.width + 4.0f, result.log.y, 6.0f, result.log.height};
        if (this->Focused() && !this->editor_.Completions().empty())
        {
            result.completion_row_height = std::max(22.0f, design.TextHeight(ChatThemeStyle(design, this->chrome_.style)) + 6);
            result.completion_rows = std::min(this->editor_.Completions().size(), static_cast<size_t>(std::clamp(static_cast<int>((result.log.height - 44) / result.completion_row_height), 1, 8)));
            const float height = result.completion_rows * result.completion_row_height + 44;
            const float bottom = this->ControllerComposing() ? result.log.y + result.log.height : result.input.y - 3;
            result.completion = {result.content.x, bottom - height, result.content.width, height};
        }
        return result;
    }

    auto chat_module::Scroll(const int amount) -> void
    {
        if (amount != 0) this->message_entrances_.clear();
        if (amount != 0) this->scrollbar_until_ = ::GetTickCount64() + 1200;
        auto& scroll = this->CurrentScroll();
        if (amount > 0) scroll = std::min(scroll + amount, this->rows_.size());
        else scroll -= std::min(scroll, static_cast<size_t>(-amount));
        if (scroll == 0 && !this->HistoryExpanded()) this->tabs_[this->active_tab_].unread = 0;
    }

    auto chat_module::ScrollbarOpacity(const uint64_t now) const -> float
    { return now >= this->scrollbar_until_ ? 0 : std::min(1.0f, static_cast<float>(this->scrollbar_until_ - now) / 200.0f); }

    auto chat_module::LayoutHandleMouse(const uint32_t message, const WPARAM wParam, const float x, const float y, const bool editing,
        const ui::design_system& design, const ui::rect& viewport, bool& save, IChatManager* chat) -> bool
    {
        if (!this->WindowVisible() && (!editing || this->secondary_)) { this->CancelPointer(); return false; }
        if (editing) return false;
        if (this->HistoryExpanded()) return this->HandleHistoryMouse(message, wParam, x, y, design);
        this->channel_picker_viewport_ = viewport;
        if (!this->resizing_ && this->HandleChannelMouse(message, x, y, design, save)) return true;
        const bool down = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
        if (down && !this->pointer_down_ && this->sounds_ != nullptr && this->LayoutSoundTarget(design, x, y, editing) != 0)
            this->sounds_->Select(::GetTickCount64());
        if (this->resizing_)
        {
            if (message == WM_MOUSEMOVE || message == WM_LBUTTONUP)
            {
                const float bottom = this->resize_bounds_.y + this->resize_bounds_.height;
                this->state_.bounds.width = std::clamp(this->resize_bounds_.width + x - this->resize_x_, this->state_.minimum_width,
                    std::max(this->state_.minimum_width, viewport.x + viewport.width - this->resize_bounds_.x));
                const float tabsHeight = this->state_.bounds.y - this->VisualBounds(design).y;
                this->state_.bounds.height = std::clamp(this->resize_bounds_.height - y + this->resize_y_, this->state_.minimum_height, std::max(this->state_.minimum_height, bottom - viewport.y - tabsHeight));
                if (this->state_.bounds.height != this->resize_bounds_.height) this->height_limits_lines_ = true;
                this->state_.bounds.y = bottom - this->state_.bounds.height;
                this->state_.content_bounds = {};
            }
            if (message == WM_LBUTTONUP) { this->resizing_ = false; this->pointer_down_ = false; save = true; }
            return true;
        }
        if (down && Contains(this->Grip(design), x, y))
        {
            this->resizing_ = this->pointer_down_ = true;
            this->resize_bounds_ = this->state_.bounds;
            this->resize_x_ = x; this->resize_y_ = y;
            return true;
        }
        if (message == WM_LBUTTONUP && this->pointer_down_) { this->pointer_down_ = false; return true; }
        if (!this->LayoutHitTest(design, x, y)) { if (down) this->Focus(false); return false; }
        const auto layout = this->Layout(design);
        if (this->ControllerComposing() && !editing)
            for (size_t index = 0; index < layout.controller_actions.size(); ++index)
                if (Contains(layout.controller_actions[index], x, y))
                {
                    if (message == WM_MOUSEWHEEL && index == 0)
                    {
                        this->AdjustControllerChannel(static_cast<short>(HIWORD(wParam)) > 0 ? -1 : 1);
                        save = true;
                    }
                    if (down)
                    {
                        this->pointer_down_ = true;
                        this->controller_compose_action_ = index;
                        if (index != 0) this->controller_channel_adjusting_ = false;
                        this->ActivateControllerCompose(chat);
                    }
                    return true;
                }
        if (layout.completion_rows > 0 && Contains(layout.completion, x, y))
        {
            const size_t first = this->editor_.CompletionIndex() / layout.completion_rows * layout.completion_rows;
            if (message == WM_MOUSEWHEEL)
            {
                const auto count = this->editor_.Completions().size();
                const int step = static_cast<short>(HIWORD(wParam)) / WHEEL_DELTA;
                this->editor_.SelectCompletion(static_cast<size_t>(std::clamp<int>(static_cast<int>(this->editor_.CompletionIndex()) - step, 0, static_cast<int>(count) - 1)));
            }
            if (down)
            {
                const int row = static_cast<int>(std::floor((y - layout.completion.y - 24) / layout.completion_row_height));
                if (row >= 0 && static_cast<size_t>(row) < layout.completion_rows && first + row < this->editor_.Completions().size())
                { this->editor_.SelectCompletion(first + row); this->editor_.AcceptCompletion(); }
                this->pointer_down_ = true;
            }
            return true;
        }
        if (editing && (!down || this->secondary_ || !Contains(layout.input, x, y)))
        {
            if (down) this->Focus(false);
            return false;
        }
        if (message == WM_MOUSEWHEEL) { this->Scroll(static_cast<short>(HIWORD(wParam)) / WHEEL_DELTA * 3); return true; }
        if (!down) return false;
        this->pointer_down_ = true;
        for (size_t index = 0; index < layout.tabs.size(); ++index)
            if (Contains(layout.tabs[index], x, y))
            {
                this->active_tab_ = static_cast<int>(index);
                this->source_ = index == static_cast<size_t>(chat_tab::window1) ? 0 : 1;
                if (this->tabs_[index].scroll == 0) this->tabs_[index].unread = 0;
                save = true;
                return true;
            }
        if (this->secondary_) return true;
        if (Contains(layout.input, x, y))
        {
            this->Focus(true);
            const auto text = this->editor_.Text();
            this->input_start_ = std::min(this->input_start_, text.size());
            size_t caret = std::min(this->input_start_, text.size());
            const float local = std::clamp(x - layout.input_text.x, 0.0f, layout.input_text.width);
            while (caret < text.size())
            {
                size_t next = caret; NextDraft(text, next);
                const float previousWidth = MeasureChatText(design, this->DisplayDraft(text.substr(this->input_start_, caret - this->input_start_)), ChatInputStyle(design, this->text_.style));
                const float nextWidth = MeasureChatText(design, this->DisplayDraft(text.substr(this->input_start_, next - this->input_start_)), ChatInputStyle(design, this->text_.style));
                if (local < (previousWidth + nextWidth) * 0.5f) break;
                caret = next;
            }
            this->editor_.Set(text, caret);
        }
        return true;
    }

    auto chat_module::Candidates(IAshitaCore* core) -> std::vector<chat_completion>
    {
        constexpr const char* builtin[]{
            "/say", "/s", "/party", "/p", "/linkshell", "/l", "/linkshell2", "/l2", "/tell", "/t", "/reply", "/r",
            "/shout", "/sh", "/yell", "/y", "/unity", "/u", "/echo", "/emote", "/em", "/ma", "/magic", "/ja", "/jobability", "/ws", "/weaponskill",
            "/attack", "/assist", "/target", "/targetnpc", "/item", "/equip", "/equipset", "/follow", "/check", "/invite", "/leave", "/map", "/search", "/sea",
            "/heal", "/sit", "/logout", "/shutdown", "/xiuiplus", "/addon", "/load", "/unload", "/exec", "/wait", "/bind", "/unbind", "/alias", "/unalias",
            "<t>", "<me>", "<st>", "<stpc>", "<stnpc>", "<stpt>", "<stal>", "<bt>", "<pet>", "<p0>", "<p1>", "<p2>", "<p3>", "<p4>", "<p5>"};
        std::vector<chat_completion> values;
        for (const auto* text : builtin) values.push_back({text, text, text[0] == '/' ? chat_completion_kind::command : chat_completion_kind::text});
        if (core == nullptr) return values;
        const auto* memory = core->GetMemoryManager();
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        if (entity != nullptr)
            for (uint32_t index = 0; index < entity->GetEntityMapSize(); ++index)
                if (const auto* name = entity->GetName(index); name != nullptr && name[0] != '\0')
                { auto text = FromGameText(name); values.push_back({text, text, chat_completion_kind::player}); }
        this->LoadCompletions(core);
        values.insert(values.end(), this->resource_completions_.begin(), this->resource_completions_.end());
        return values;
    }

    auto chat_module::LoadCompletions(IAshitaCore* core) -> void
    {
        auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (resources == this->completion_resources_) return;
        this->completion_resources_ = resources;
        this->resource_completions_.clear();
        this->auto_translate_labels_.clear();
        if (resources == nullptr) return;
        for (uint32_t id = 0; id < 2048; ++id)
            if (const auto* spell = resources->GetSpellById(id); spell != nullptr && spell->Name[0] != nullptr && spell->Name[0][0] != '\0')
            { auto text = FromGameText(spell->Name[0]); this->resource_completions_.push_back({text, text}); }
        for (uint32_t id = 0; id < 2048; ++id)
            if (const auto* ability = resources->GetAbilityById(id); ability != nullptr && ability->Name[0] != nullptr && ability->Name[0][0] != '\0')
            { auto text = FromGameText(ability->Name[0]); this->resource_completions_.push_back({text, text}); }
        const auto* current = resources->GetString("jobs.names", 1u);
        const auto* japanese = resources->GetString("jobs.names", 1u, 1u);
        const bool jp = current != nullptr && japanese != nullptr && std::strcmp(current, japanese) == 0;
        char path[2048]{};
        resources->GetFilePath(jp ? 55545 : 55665, path, sizeof(path));
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file || file.tellg() <= 0 || file.tellg() > 1024 * 1024) return;
        std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return;
        const auto entries = resources::DecodeAutoTranslate(bytes);
        if (!entries) return;
        for (const auto& entry : *entries)
        {
            std::string text = entry.text;
            if (text.starts_with('@'))
            {
                if (text.size() < 3) continue;
                uint32_t id{};
                const auto parsed = std::from_chars(text.data() + 2, text.data() + text.size(), id, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) continue;
                const char* table = text[1] == 'A' ? "zones.names" : text[1] == 'C' ? "spells.names" : text[1] == 'J' ? "jobs.names" : text[1] == 'Y' ? "abilities.names" : nullptr;
                const auto* linked = table != nullptr ? resources->GetString(table, id) : nullptr;
                if (linked == nullptr) continue;
                text = linked;
            }
            const auto token = ChatToken(entry.id, entry.language);
            if (text.empty() || token.empty()) continue;
            text = FromGameText(text);
            this->auto_translate_labels_.emplace(token, text);
            this->resource_completions_.push_back({text, token, chat_completion_kind::auto_translate});
        }
    }

    auto chat_module::DisplayDraft(const std::string_view text) const -> std::string
    {
        std::string result;
        for (size_t at = 0; at < text.size();)
        {
            if (const auto length = ChatTokenSize(text, at); length != 0)
            {
                const auto found = this->auto_translate_labels_.find(std::string{text.substr(at, length)});
                result += chat_cap_open; result += found != this->auto_translate_labels_.end() ? found->second : "Auto-translate"; result += chat_cap_close;
                at += length;
            }
            else result += text[at++];
        }
        return result;
    }

    auto chat_module::DescribeCompletions(IAshitaCore* core, const std::string_view prefix) -> std::string
    {
        chat_editor editor;
        editor.Set(std::string{prefix});
        editor.CompleteCandidates(this->Candidates(core), false);
        size_t verified{}, count{};
        std::string names;
        for (const auto& match : editor.Completions())
        {
            if (match.kind != chat_completion_kind::auto_translate) continue;
            std::array<char, 512> expanded{};
            if (core != nullptr && core->GetChatManager() != nullptr && core->GetChatManager()->ParseAutoTranslate(match.value.c_str(), expanded.data(), expanded.size(), false) > 0 &&
                FromGameText(expanded.data()) == match.label) ++verified;
            if (count++ < 16) { if (!names.empty()) names += ", "; names += match.label; }
        }
        const auto report = "[XIUI+] Auto-translate: " + std::to_string(this->auto_translate_labels_.size()) + " cached; " + std::to_string(count) + " matches (" +
            std::to_string(verified) + " native token round-trips): " + names;
        std::string encoded;
        return ToGameText(report, encoded) ? encoded : report;
    }

    auto chat_module::SetInputContext(IChatManager* chat) -> void
    {
        this->input_chat_ = chat;
        const auto limit = chat != nullptr ? chat->GetInputTextParsedLengthMax() : 0;
        this->input_limit_ = limit > 0 ? std::min(limit, 511u) : 255u;
    }

    auto chat_module::InputCommand(const std::string_view text) const -> std::string
    {
        auto draft = std::string{text};
        if (const auto tokenSize = ChatTokenSize(draft, 0); tokenSize != 0)
        {
            const auto token = draft.substr(0, tokenSize);
            const auto found = this->auto_translate_labels_.find(token);
            std::string label;
            if (found != this->auto_translate_labels_.end()) label = found->second;
            else if (this->input_chat_ != nullptr)
            {
                std::array<char, 512> expanded{};
                if (this->input_chat_->ParseAutoTranslate(token.c_str(), expanded.data(), static_cast<int32_t>(expanded.size()), false) > 0)
                    label = FromGameText(expanded.data());
            }
            if (label.starts_with('/')) draft.replace(0, tokenSize, label);
        }
        return !draft.empty() && draft.front() == '/' ? draft : std::string{commands[this->channel_]} + draft;
    }

    auto chat_module::InputDestination(void) const -> chat_destination
    {
        const auto command = this->InputCommand(this->editor_.Text());
        if (this->channel_ == game_channel && (command.empty() || command.front() != '/'))
            return DescribeChatDestination(this->native_send_prefix_.value_or("/say "), this->native_reply_recipient_);
        auto destination = DescribeChatDestination(command, this->native_reply_recipient_);
        if (destination.mode == 4 && !this->reply_destination_.empty() && destination.label.starts_with("Tell: ") &&
            SameRecipient(std::string_view{destination.label}.substr(6), this->reply_destination_)) destination.reply = true;
        return destination;
    }

    auto chat_module::InputUsage(const std::string_view text) const -> std::pair<size_t, size_t>
    {
        const auto command = this->InputCommand(text);
        std::string encoded;
        if (!ToGameText(command, encoded)) encoded = command;
        const size_t overhead = command == std::string{commands[this->channel_]} + std::string{text} ? std::strlen(commands[this->channel_]) : 0;
        return {encoded.size() - std::min(overhead, encoded.size()), this->input_limit_ - std::min<size_t>(overhead, this->input_limit_)};
    }

    auto chat_module::Submit(IChatManager* chat) -> bool
    {
        if (this->editor_.Text().empty()) { this->Focus(false); return true; }
        if (chat == nullptr) { this->error_ = "Chat is unavailable."; return false; }
        this->SetInputContext(chat);
        const auto command = this->InputCommand(this->editor_.Text());
        if (IsTellDraft(command))
        {
            const auto nameStart = command.find_first_not_of(' ', command.find(' '));
            const auto nameEnd = command.find(' ', nameStart);
            if (nameEnd == std::string::npos || command.find_first_not_of(" \t", nameEnd) == std::string::npos)
            {
                this->error_.clear();
                return false;
            }
        }
        std::string encoded;
        if (!ToGameText(command, encoded)) { this->error_ = "This text contains characters the game cannot send."; return false; }
        if (encoded.size() > this->input_limit_) { this->error_ = "Message exceeds the game's input limit."; return false; }
        const auto mode = this->channel_ == game_channel && command.front() != '/' ? Ashita::CommandMode::Typed : Ashita::CommandMode::AshitaParse;
        chat->QueueCommand(static_cast<int32_t>(mode), encoded.c_str());
        if (this->sounds_ != nullptr) this->sounds_->Play(ui::native_ui_sound::chat_send);
        this->editor_.Set(command);
        this->editor_.Remember();
        this->editor_.Set({});
        this->error_.clear();
        this->Focus(false);
        return true;
    }

    auto chat_module::AdjustControllerChannel(const int direction) -> void
    {
        if (!this->editor_.Text().empty() && this->editor_.Text().front() == '/')
        {
            this->error_ = "This draft sets its own channel. Edit its /command to change it.";
            return;
        }
        this->channel_ = (this->channel_ + (direction < 0 ? static_cast<int>(std::size(channels)) - 1 : 1)) % std::size(channels);
        this->error_.clear();
        if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
    }

    auto chat_module::ActivateControllerCompose(IChatManager* chat) -> bool
    {
        if (this->controller_compose_action_ == 0)
        {
            this->controller_channel_adjusting_ = !this->controller_channel_adjusting_;
            if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
            return true;
        }
        if (this->controller_compose_action_ == 1)
        {
            this->Submit(chat);
            return true;
        }
        auto* history = this->controller_return_history_ != nullptr ? this->controller_return_history_ : this;
        this->Focus(false);
        history->BeginControllerHistory();
        return true;
    }

    auto chat_module::HandleControllerCompose(const controller_action action, const bool repeated, IChatManager* chat) -> bool
    {
        if (action == controller_action::confirm)
            return repeated || this->ActivateControllerCompose(chat);
        if (action == controller_action::cancel || action == controller_action::actions)
        {
            if (!repeated)
            {
                if (this->controller_channel_adjusting_ && action == controller_action::cancel) this->controller_channel_adjusting_ = false;
                else { this->controller_compose_action_ = 2; this->ActivateControllerCompose(chat); }
            }
            return true;
        }
        const bool previous = action == controller_action::left || action == controller_action::up || action == controller_action::previous_tab;
        const bool next = action == controller_action::right || action == controller_action::down || action == controller_action::next_tab;
        if (!previous && !next) return false;
        if (this->controller_channel_adjusting_) this->AdjustControllerChannel(previous ? -1 : 1);
        else
        {
            this->controller_compose_action_ = (this->controller_compose_action_ + (previous ? 2 : 1)) % 3;
            if (this->sounds_ != nullptr) this->sounds_->Hover(900 + this->controller_compose_action_, ::GetTickCount64());
        }
        return true;
    }

    auto chat_module::Copy(const HWND window, const bool cut) -> void
    {
        const auto raw = this->editor_.SelectedText();
        const auto selected = FromUtf8(PlainChatText(this->DisplayDraft(raw)));
        if (selected.empty() || !::OpenClipboard(window)) return;
        auto memory = ::GlobalAlloc(GMEM_MOVEABLE, (selected.size() + 1) * sizeof(wchar_t));
        bool copied{};
        if (memory != nullptr)
        {
            if (auto* output = ::GlobalLock(memory); output != nullptr)
            {
                std::memcpy(output, selected.c_str(), (selected.size() + 1) * sizeof(wchar_t));
                ::GlobalUnlock(memory);
                copied = ::EmptyClipboard() && ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
            }
            if (!copied) ::GlobalFree(memory);
        }
        if (copied)
        {
            const auto format = ::RegisterClipboardFormatA("XIUI+.ChatDraft");
            const auto data = ::GlobalAlloc(GMEM_MOVEABLE, raw.size() + 1);
            if (data != nullptr)
            {
                bool stored{};
                if (auto* output = ::GlobalLock(data); output != nullptr)
                { std::memcpy(output, raw.c_str(), raw.size() + 1); ::GlobalUnlock(data); stored = format != 0 && ::SetClipboardData(format, data) != nullptr; }
                if (!stored) ::GlobalFree(data);
            }
        }
        ::CloseClipboard();
        if (cut && copied) this->editor_.Erase(true);
    }

    auto chat_module::Paste(const HWND window) -> void
    {
        if (!::OpenClipboard(window)) return;
        const auto format = ::RegisterClipboardFormatA("XIUI+.ChatDraft");
        if (const auto data = format != 0 ? ::GetClipboardData(format) : nullptr; data != nullptr)
        {
            if (const auto* raw = static_cast<const char*>(::GlobalLock(data)); raw != nullptr)
            {
                this->editor_.Insert({raw, strnlen_s(raw, std::min<size_t>(::GlobalSize(data), 8192))});
                ::GlobalUnlock(data); ::CloseClipboard(); return;
            }
        }
        auto memory = ::GetClipboardData(CF_UNICODETEXT);
        if (memory != nullptr)
        {
            const auto capacity = ::GlobalSize(memory) / sizeof(wchar_t);
            if (const auto* text = static_cast<const wchar_t*>(::GlobalLock(memory)); text != nullptr)
            {
                this->editor_.Insert(ToUtf8({text, wcsnlen_s(text, std::min<size_t>(capacity, 8192))}));
                ::GlobalUnlock(memory);
            }
        }
        ::CloseClipboard();
    }

    auto chat_module::OpenTell(const std::string_view recipient) -> bool
    {
        if (!this->state_.enabled || this->secondary_ || !IsTellRecipient(recipient)) return false;
        std::string_view body = this->editor_.Text();
        if (body.starts_with('/'))
        {
            const auto end = body.find(' ');
            const auto command = body.substr(0, end);
            if (CommandMode(command) != 0)
            {
                body = end == std::string_view::npos ? std::string_view{} : body.substr(end + 1);
                if (command == "/t" || command == "/tell")
                {
                    const auto nameEnd = body.find(' ');
                    body = nameEnd == std::string_view::npos ? std::string_view{} : body.substr(nameEnd + 1);
                }
            }
        }
        this->editor_.Set("/t " + std::string{recipient} + " " + std::string{body});
        this->reply_recipient_.clear();
        this->reply_destination_.clear();
        this->error_.clear();
        this->input_start_ = 0;
        this->Focus(true);
        return true;
    }

    auto chat_module::OpenReply(const std::span<const std::string> targets) -> bool
    {
        auto ordered = this->recent_tellers_;
        for (const auto& target : targets)
            if (ordered.size() < 8 && IsTellRecipient(target) &&
                std::none_of(ordered.begin(), ordered.end(), [&](const auto& name) { return SameRecipient(name, target); }))
                ordered.push_back(target);
        if (ordered.empty()) return false;
        size_t next{};
        const std::string_view draft = this->editor_.Text();
        if (!this->reply_recipient_.empty() && this->Focused() && IsTellDraft(draft))
        {
            const auto start = draft.find_first_not_of(' ', draft.find(' '));
            const auto recipient = draft.substr(start, draft.find(' ', start) - start);
            if (SameRecipient(recipient, this->reply_recipient_))
                for (size_t i = 0; i < ordered.size(); ++i)
                    if (SameRecipient(ordered[i], recipient)) { next = (i + 1) % ordered.size(); break; }
        }
        if (!this->OpenTell(ordered[next])) return false;
        this->reply_recipient_ = ordered[next];
        this->reply_destination_ = ordered[next];
        return true;
    }

    auto chat_module::ObserveTeller(const std::string_view name) -> void
    {
        if (!IsTellRecipient(name)) return;
        std::erase_if(this->recent_tellers_, [&](const auto& entry) { return SameRecipient(entry, name); });
        this->recent_tellers_.insert(this->recent_tellers_.begin(), std::string{name});
        if (this->recent_tellers_.size() > 8) this->recent_tellers_.resize(8);
        this->reply_recipient_.clear();
    }

    auto chat_module::ObserveTellPacket(const uint16_t id, const std::span<const uint8_t> packet) -> void
    {
        // 0x017: incoming tell channel and the fixed 15-byte sender field.
        if (this->secondary_ || id != 0x0017 || packet.size() < 24 || packet[4] != 3) return;
        const auto* name = reinterpret_cast<const char*>(packet.data() + 8);
        this->ObserveTeller({name, strnlen_s(name, 15)});
    }

    auto chat_module::AdoptTell(chat_draft draft, const std::string_view recipient) -> bool
    {
        if (!this->state_.enabled || this->secondary_ || !IsTellDraft(draft.text)) return false;
        if (!recipient.empty())
        {
            if (!IsTellRecipient(recipient)) return false;
            const auto start = draft.text.find_first_not_of(' ', draft.text.find(' '));
            const auto space = draft.text.find(' ', start);
            const auto end = space == std::string::npos ? draft.text.size() : space;
            if (draft.caret > start)
                draft.caret = start + recipient.size() + (draft.caret > end ? draft.caret - end : 0);
            draft.text.replace(start, end - start, recipient);
        }
        return this->AdoptDraft(std::move(draft));
    }

    auto chat_module::AdoptDraft(chat_draft draft) -> bool
    {
        if (!this->state_.enabled || this->secondary_) return false;
        if (!draft.text.empty()) this->editor_.Set(std::move(draft.text), draft.caret);
        this->reply_recipient_.clear();
        this->reply_destination_.clear();
        this->error_.clear();
        this->input_start_ = 0;
        this->Focus(true);
        return true;
    }

    auto chat_module::HandleKey(IAshitaCore* core, const WPARAM key, const LPARAM flags, const bool blocked, const bool replaceNative, const std::span<const std::string> replyTargets, const bool startTyping) -> bool
    {
        if (core != nullptr) this->SetInputContext(core->GetChatManager());
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (released || !this->state_.enabled) return false;
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        if (key == 'R' && (control != alt) && !shift && !blocked && replaceNative)
            return (static_cast<uint32_t>(flags) & 0x40000000u) != 0 ? this->Focused() : this->OpenReply(replyTargets);
        if (!this->Focused())
        {
            if (blocked || !replaceNative || control || alt || (static_cast<uint32_t>(flags) & 0x40000000u) != 0) return false;
            if ((key == VK_OEM_2 || key == VK_DIVIDE) && !shift)
            {
                this->editor_.Set("/");
                this->error_.clear();
                this->input_start_ = 0;
                this->Focus(true);
                return true;
            }
            if (startTyping)
            {
                const auto text = key == VK_SPACE ? std::string{} : KeyText(key, flags);
                if (key != VK_SPACE && text.empty()) return false;
                this->error_.clear(); this->input_start_ = 0;
                this->Focus(true);
                if (!text.empty()) this->editor_.Insert(text);
                return true;
            }
            return false;
        }
        const HWND window = core != nullptr && core->GetProperties() != nullptr ? core->GetProperties()->GetFinalFantasyHwnd() : nullptr;
        if (this->channel_picker_open_)
        {
            this->channel_picker_open_ = false;
            if (key == VK_ESCAPE || key == VK_RETURN) return true;
        }
        this->error_.clear();
        if (key == VK_RETURN && (static_cast<uint32_t>(flags) & 0x40000000u) != 0) return true;
        if (alt && key == VK_TAB) return false;
        switch (key)
        {
        case VK_ESCAPE:
            if (!this->editor_.Completions().empty()) this->editor_.CancelCompletion();
            else this->Focus(false);
            break;
        case VK_RETURN:
            if (!this->editor_.Completions().empty()) this->editor_.AcceptCompletion();
            else this->Submit(core != nullptr ? core->GetChatManager() : nullptr);
            break;
        case VK_TAB:
            if (this->editor_.Completions().empty())
            {
                this->editor_.CompleteCandidates(this->Candidates(core), shift);
                if (this->editor_.Completions().empty()) this->error_ = "No matching completions.";
            }
            else this->editor_.CompleteCandidates({}, shift);
            break;
        case VK_BACK: this->editor_.Erase(true, control); break;
        case VK_DELETE: this->editor_.Erase(false, control); break;
        case VK_LEFT: this->editor_.Move(-1, shift, control); break;
        case VK_RIGHT: this->editor_.Move(1, shift, control); break;
        case VK_HOME: this->editor_.Home(false, shift); break;
        case VK_END: if (control) this->tabs_[this->active_tab_].scroll = this->tabs_[this->active_tab_].unread = 0; else this->editor_.Home(true, shift); break;
        case VK_UP: case VK_DOWN:
            if (!this->editor_.Completions().empty()) this->editor_.CompleteCandidates({}, key == VK_UP);
            else this->editor_.History(key == VK_UP ? -1 : 1);
            break;
        case VK_PRIOR: case VK_NEXT:
            if (!this->editor_.Completions().empty())
                this->editor_.SelectCompletion(static_cast<size_t>(std::clamp<int>(static_cast<int>(this->editor_.CompletionIndex()) + (key == VK_PRIOR ? -8 : 8), 0, static_cast<int>(this->editor_.Completions().size()) - 1)));
            else this->Scroll(key == VK_PRIOR ? 8 : -8);
            break;
        default:
            if (control && !alt)
            {
                if (key == 'A') this->editor_.SelectAll();
                else if (key == 'C' || key == 'X') this->Copy(window, key == 'X');
                else if (key == 'V') this->Paste(window);
            }
            else if (!alt || control)
            {
                this->editor_.Insert(KeyText(key, flags));
            }
        }
        return true;
    }

    auto chat_module::RebuildRows(const ui::design_system& design, const float width) -> void
    {
        const int timestampMode = this->TimestampMode();
        const int filter = this->RowFilter();
        const float hudScale = design.Transform().y;
        const bool themed = design.IsXiv1Theme();
        const auto style = ChatThemeStyle(design, this->text_.style);
        if (this->cache_xiv1_ == themed && this->cache_hud_scale_ == hudScale && this->cache_revision_ == this->History().Revision() && this->cache_width_ == width && this->cache_scale_ == style.scale &&
            this->cache_spacing_ == style.letter_spacing && this->cache_font_ == style.font && this->cache_tab_ == filter && this->cache_timestamp_mode_ == timestampMode) return;
        const bool incremental = this->cache_xiv1_ == themed && this->cache_hud_scale_ == hudScale && this->cache_revision_ != UINT64_MAX && this->cache_revision_ != this->History().Revision() && this->cache_tab_ == filter && this->cache_width_ == width &&
            this->cache_scale_ == style.scale && this->cache_spacing_ == style.letter_spacing && this->cache_font_ == style.font && this->cache_timestamp_mode_ == timestampMode && !this->History().Lines().empty();
        size_t previousRows{};
        const size_t oldLastRows = incremental ? std::count_if(this->rows_.begin(), this->rows_.end(), [this](const rendered_row& row) { return row.sequence == this->cache_last_sequence_; }) : 0;
        if (incremental)
        {
            const auto first = this->History().Lines().front().sequence;
            std::erase_if(this->rows_, [first, this](const rendered_row& row) { return row.sequence < first || row.sequence >= this->cache_last_sequence_; });
            previousRows = this->rows_.size();
        }
        else this->rows_.clear();
        for (const auto& line : this->History().Lines())
        {
            if (incremental && line.sequence < this->cache_last_sequence_) continue;
            if (this->HistoryExpanded() ? !this->HistoryTabMatches(this->history_view_.tab, line.mode, line.windows) :
                !ChatTabMatches(static_cast<chat_tab>(this->active_tab_), line.mode, line.windows)) continue;
            rendered_row row{line.sequence, line.mode, line.color};
            float used{};
            const auto finish = [&] { this->rows_.push_back(std::move(row)); row = {line.sequence, line.mode, line.color}; used = 0.0f; };
            const auto append = [&](const std::string& token, const int color, const bool timestamp = false, const bool prompt = false, const bool emphasis = false, const bool name = false) {
                auto runStyle = style; runStyle.weight = ChatRunWeight(design, emphasis);
                const auto tokenWidth = MeasureChatText(design, token, runStyle) + ChatThemeTracking(runStyle) +
                    (prompt ? dialogue_prompt_padding : 0.0f);
                if (used > 0.0f && used + tokenWidth > width)
                {
                    if (token.find_first_not_of(' ') == std::string::npos) return;
                    finish();
                }
                if (tokenWidth <= width)
                {
                    if (!row.spans.empty() && row.spans.back().color == color && row.spans.back().timestamp == timestamp && row.spans.back().dialogue_prompt == prompt && row.spans.back().emphasis == emphasis && row.spans.back().name == name) { row.spans.back().text += token; row.spans.back().width += tokenWidth; }
                    else row.spans.push_back({token, color, tokenWidth, timestamp, prompt, emphasis, name});
                    used += tokenWidth;
                    return;
                }
                for (size_t pos = 0; pos < token.size();)
                {
                    const size_t begin = pos;
                    NextUtf8(token, pos);
                    const auto glyph = token.substr(begin, pos - begin);
                    const auto glyphWidth = MeasureChatText(design, glyph, runStyle) + ChatThemeTracking(runStyle) +
                        (prompt ? dialogue_prompt_padding : 0.0f);
                    if (used > 0 && used + glyphWidth > width) finish();
                    if (!row.spans.empty() && row.spans.back().color == color && row.spans.back().timestamp == timestamp && row.spans.back().dialogue_prompt == prompt && row.spans.back().emphasis == emphasis && row.spans.back().name == name) { row.spans.back().text += glyph; row.spans.back().width += glyphWidth; }
                    else row.spans.push_back({glyph, color, glyphWidth, timestamp, prompt, emphasis, name});
                    used += glyphWidth;
                }
            };
            const auto appendTimestamp = [&] {
                if (timestampMode == 0) return;
                const auto seconds = line.local_seconds;
                char timestamp[20]{};
                if (timestampMode == 2) std::snprintf(timestamp, sizeof(timestamp), "[%02u:%02u:%02u] ", seconds / 3600, seconds / 60 % 60, seconds % 60);
                else std::snprintf(timestamp, sizeof(timestamp), "[%02u:%02u] ", seconds / 3600, seconds / 60 % 60);
                append(timestamp, -1, true);
            };
            appendTimestamp();
            for (const auto& span : line.spans)
                for (size_t position = 0; position < span.text.size();)
                {
                    if (span.text[position] == '\n') { finish(); appendTimestamp(); ++position; continue; }
                    size_t end = span.text.find_first_of(" \t\n", position);
                    if (end == position) ++end;
                    if (end == std::string::npos) end = span.text.size();
                    auto token = span.text.substr(position, end - position);
                    std::replace(token.begin(), token.end(), '\t', ' ');
                    append(token, span.color_index, false, false, span.emphasis, span.name);
                    position = end;
                }
            if (line.dialogue_prompt) append("\xE2\x96\xBC", -1, false, true);
            if (!row.spans.empty()) finish();
        }
        if (incremental && this->CurrentScroll() > 0 && this->rows_.size() > previousRows + oldLastRows)
            this->CurrentScroll() += this->rows_.size() - previousRows - oldLastRows;
        this->cache_xiv1_ = themed;
        this->cache_revision_ = this->History().Revision(); this->cache_width_ = width; this->cache_hud_scale_ = hudScale;
        this->cache_scale_ = style.scale; this->cache_spacing_ = style.letter_spacing;
        this->cache_font_ = style.font;
        this->cache_tab_ = filter; this->cache_timestamp_mode_ = timestampMode;
        this->cache_last_sequence_ = this->History().Lines().empty() ? 0 : this->History().Lines().back().sequence;
    }

    auto chat_module::DrawCompletions(const layout& layout, const module_render_context& context) const -> void
    {
        if (layout.completion_rows == 0) return;
        const auto& theme = context.Design().GetTheme();
        const auto& bounds = layout.completion;
        context.Design().FillRect(bounds, theme.colors.chrome | 0xFF000000);
        ui::container_style style{}; style.padding = 3;
        context.DrawContainer(bounds, style);
        auto text = ChatThemeProfile(context.Design(), this->chrome_);
        text.placement = 0; text.offset_x = text.offset_y = 0; text.style.alignment = ui::text_alignment::left;
        const size_t selected = this->editor_.CompletionIndex();
        const size_t first = selected / layout.completion_rows * layout.completion_rows;
        const auto& matches = this->editor_.Completions();
        char title[80]{};
        std::snprintf(title, sizeof(title), "Completion  %zu-%zu / %zu", first + 1, std::min(first + layout.completion_rows, matches.size()), matches.size());
        const ui::rect header{bounds.x + 9, bounds.y + 4, bounds.width - 18, 18};
        context.DrawText(title, header, header, text);
        for (size_t row = 0; row < layout.completion_rows && first + row < matches.size(); ++row)
        {
            const auto& match = matches[first + row];
            const ui::rect at{bounds.x + 6, bounds.y + 24 + row * layout.completion_row_height, bounds.width - 12, layout.completion_row_height};
            if (first + row == selected) context.Design().FillRect(at, theme.colors.selected | 0xB0000000);
            const ui::rect label{at.x + 5, at.y, std::max(1.0f, at.width - 95), at.height};
            const auto name = match.kind == chat_completion_kind::auto_translate ? std::string{chat_cap_open} + match.label + std::string{chat_cap_close} : match.label;
            DrawThemedChatText(context, name, label, TintTextProfile(text, theme.colors.text));
            auto badge = text; badge.style.scale *= 0.8f; badge.style.alignment = ui::text_alignment::right;
            const auto* kind = match.kind == chat_completion_kind::auto_translate ? "Auto-translate" : match.kind == chat_completion_kind::command ? "Command" : match.kind == chat_completion_kind::player ? "Player" : "Text";
            const ui::rect badgeBounds{at.x + at.width - 90, at.y, 84, at.height};
            context.DrawText(kind, badgeBounds, badgeBounds, badge);
        }
        auto help = text; help.style.scale *= 0.78f;
        const ui::rect footer{bounds.x + 9, bounds.y + bounds.height - 20, bounds.width - 18, 16};
        context.DrawText(i18n::Text("Tab / arrows: browse   Enter: choose   Esc: cancel"), footer, footer, help);
    }

    auto chat_module::SpanColor(const rendered_row& row, const rendered_span& span, const chat_palette& palette, const bool xiv1) const -> D3DCOLOR
    {
        if (span.timestamp && this->timestamp_color_enabled_) return this->timestamp_color_;
        const auto configured = palette.modes[row.mode & 0xFF];
        const auto base = configured != 0 ? configured : row.color;
        const auto inlineColor = span.color >= 0 && static_cast<size_t>(span.color) < palette.colors.size() ? palette.colors[span.color] : 0;
        const auto color = this->Colors().Resolve(row.mode, ChatThemeColor(row.mode, base, xiv1), inlineColor);
        return span.name ? ChatNameColor(color) : color;
    }

    auto chat_module::DrawLog(const hud_snapshot& snapshot, const module_render_context& context, const ui::rect& log, const ui::rect& scrollbar) -> void
    {
        const auto now = ::GetTickCount64();
        const auto& theme = context.Design().GetTheme();
        const float lineHeight = std::max(1.0f, context.Design().TextHeight(ChatThemeStyle(context.Design(), this->text_.style)) + this->line_spacing_);
        if (this->HistoryExpanded() || this->CurrentScroll() != 0 || this->cache_tab_ != this->RowFilter()) this->message_entrances_.clear();
        std::erase_if(this->message_entrances_, [now, duration = this->MessageAnimationDuration()](const auto& entry) { return now < entry.started || now - entry.started >= duration; });
        this->RebuildRows(context.Design(), log.width);
        auto logClip = log;
        if (!this->HistoryExpanded())
        {
            const auto frame = this->FrameBounds(context.Design());
            const auto top = std::min(log.y, frame.y + std::max(0.0f, this->container_.edge_size) + 1);
            logClip.height += logClip.y - top;
            logClip.y = top;
        }
        const bool collapsed = this->collapse_active_ && !this->CollapsePinned() && !this->HistoryExpanded();
        const size_t visible = collapsed ? this->collapse_visible_rows_ : std::max<size_t>(1, static_cast<size_t>(logClip.height / lineHeight));
        this->visible_rows_ = visible;
        auto& scroll = this->CurrentScroll();
        const size_t maximum = this->rows_.size() > visible ? this->rows_.size() - visible : 0;
        scroll = std::min(scroll, maximum);
        if (scroll == 0 && !this->HistoryExpanded()) this->tabs_[this->active_tab_].unread = 0;
        const size_t end = this->rows_.size() - scroll;
        const auto scrollOffset = this->MessageScrollOffset(now, lineHeight);
        const auto extraRows = static_cast<size_t>(std::ceil(scrollOffset / lineHeight));
        const size_t visibleBegin = end > visible ? end - visible : 0;
        size_t begin = visibleBegin > extraRows ? visibleBegin - extraRows : 0;
        if (collapsed)
        {
            const auto first = std::upper_bound(this->rows_.begin(), this->rows_.end(), this->collapsed_through_,
                [](const uint64_t sequence, const rendered_row& row) { return sequence < row.sequence; });
            begin = std::max(begin, std::min(visibleBegin, static_cast<size_t>(first - this->rows_.begin())));
        }
        {
            const auto clip = context.Design().Clip(logClip);
            const auto fade = context.Design().FadeTop(log.y, std::min(12.0f, lineHeight * .6f));
            float y = log.y + log.height - static_cast<float>(end - begin) * lineHeight + scrollOffset;
            for (size_t index = begin; index < end; ++index, y += lineHeight)
            {
                const auto& row = this->rows_[index];
                auto motion = this->MessageMotion(row.sequence, now);
                if (collapsed && index < this->collapse_fade_end_ && row.sequence == this->collapse_fade_sequence_) motion.opacity *= this->collapse_opacity_;
                float rowWidth{}; for (const auto& span : row.spans) rowWidth += span.width;
                float x = log.x + motion.x;
                if (this->text_.placement == 2) x += std::max(0.0f, (log.width - rowWidth) * 0.5f);
                if (this->text_.placement == 3) x += std::max(0.0f, log.width - rowWidth);
                for (const auto& span : row.spans)
                {
                    const auto color = this->SpanColor(row, span, snapshot.chat_colors, context.Design().IsXiv1Theme());
                    auto profile = FadeText(TintTextProfile(this->text_, color), motion.opacity);
                    if (!span.emphasis && !span.timestamp) profile = ChatBodyProfile(context.Design(), profile);
                    profile.style.weight = ChatRunWeight(context.Design(), span.emphasis);
                    profile.placement = 0; profile.style.alignment = ui::text_alignment::left;
                    const float padding = span.dialogue_prompt ? dialogue_prompt_padding : 0.0f;
                    ui::rect bounds{x + padding, y, std::max(0.0f, log.x + log.width - x - padding), lineHeight};
                    if (span.dialogue_prompt && this->DialoguePromptActive(row.sequence))
                    {
                        const float phase = static_cast<float>(now % 900) * (6.2831853f / 900);
                        bounds.y -= (1 - std::cos(phase)) * 1.5f * profile.style.scale;
                        profile = FadeText(profile, 0.75f + std::cos(phase) * 0.25f);
                    }
                    DrawThemedChatText(context, span.text, bounds, profile);
                    x += span.width;
                }
            }
        }
        if (this->rows_.empty() && this->History().Lines().empty() && !collapsed && !this->HistoryExpanded())
        {
            const auto text = snapshot.chat_capture_available ? (this->secondary_ ? "This window follows your game filters." : "Chat follows your game filters. Press / to chat.") : "Native chat capture unavailable. Keep game chat enabled.";
            context.DrawText(text, log, log, ChatThemeProfile(context.Design(), this->chrome_));
        }
        const float scrollbarOpacity = this->ScrollbarOpacity(now);
        if (maximum > 0 && scrollbarOpacity > 0)
        {
            const auto& track = scrollbar;
            const float thumbHeight = std::min(track.height, std::max(18.0f, track.height * static_cast<float>(visible) / static_cast<float>(this->rows_.size())));
            context.Design().FillRect({track.x + 1, track.y + (track.height - thumbHeight) * (1.0f - static_cast<float>(scroll) / static_cast<float>(maximum)), track.width - 2, thumbHeight}, FadeColor(theme.colors.accent | 0xFF000000, scrollbarOpacity));
        }
    }

    auto chat_module::DrawWindowContainer(const module_render_context& context, const ui::rect& bounds) const -> void
    {
        if (this->background_) context.DrawContainer(bounds, this->container_);
        else if (this->container_.border) context.DrawContainerFrame(bounds, this->container_);
    }

    auto chat_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        if (this->HistoryExpanded()) return;
        const auto now = ::GetTickCount64();
        this->SetLayoutEditing(context.IsEditing());
        this->channel_picker_viewport_ = context.Viewport();
        if (context.IsEditing()) this->channel_picker_open_ = false;
        const auto destinationBounds = this->Layout(context.Design()).destination;
        const ui::rect destinationHit{destinationBounds.x - 3, destinationBounds.y + 1,
            destinationBounds.width + 6, std::max(0.0f, destinationBounds.height - 2)};
        this->destination_hovered_ = this->hovered_ && !context.IsEditing() &&
            Contains(destinationHit, this->hover_point_.x, this->hover_point_.y);
        this->UpdateInputAnimation(now);
        this->UpdateCompactLayout(context.Design(), now);
        const auto frame = this->FrameBounds(context.Design());
        if (frame.height <= 0 && !this->TabsVisible()) return;
        if (frame.height > 0) this->DrawWindowContainer(context, frame);
        const auto layout = this->Layout(context.Design());
        const auto& theme = context.Design().GetTheme();
        for (size_t index = 0; index < layout.tabs.size(); ++index)
        {
            if (layout.tabs[index].height <= 0) continue;
            auto profile = ChatThemeProfile(context.Design(), this->chrome_);
            profile.style.alignment = ui::text_alignment::center;
            DrawChatTab(context, layout.tabs[index], chat_tab_names[index], static_cast<int>(index) == this->active_tab_, profile);
            const ui::rect label{layout.tabs[index].x + 5.0f, layout.tabs[index].y, layout.tabs[index].width - 10.0f, layout.tabs[index].height};
            if (this->tabs_[index].unread > 0) context.Design().FillRect({label.x + label.width - 1.0f, label.y + 3.0f, 3.0f, 3.0f}, theme.colors.accent);
        }
        if (frame.height > 0) this->DrawLog(snapshot, context, layout.log, layout.scrollbar);
        const auto& tab = this->tabs_[this->active_tab_];
        if (this->HistoryFocused())
        {
            auto help = ChatThemeProfile(context.Design(), this->chrome_); help.style.alignment = ui::text_alignment::right;
            const ui::rect hint{frame.x, frame.y - 24, frame.width, 20};
            context.DrawText(this->controller_history_ ? this->controller_compact_prompt_.c_str() : i18n::Text("F: cycle focus   Enter: expand chat   Esc: back"), hint, hint, help);
        }
        if (!this->secondary_ && this->input_reveal_ > 0)
        {
            const float reveal = InputEase(this->input_reveal_);
            const float slide = 4 * (1 - reveal);
            if (!context.Design().IsXiv1Theme()) context.Design().FillRect(layout.input, FadeColor(0x60000000, reveal));
            auto input = layout.input_text; input.y += slide;
            auto profile = FadeText(this->text_, reveal); profile.style = ChatInputStyle(context.Design(), profile.style); profile.placement = 0; profile.offset_x = profile.offset_y = 0.0f; profile.style.alignment = ui::text_alignment::left;
            const auto& text = this->editor_.Text();
            size_t start{};
            while (start < this->editor_.Caret() && MeasureChatText(context.Design(), this->DisplayDraft(text.substr(start, this->editor_.Caret() - start)), profile.style) > input.width - 5.0f) NextDraft(text, start);
            this->input_start_ = start;
            const auto [selectionBegin, selectionEnd] = this->editor_.Selection();
            if (selectionEnd > selectionBegin && selectionEnd > start)
            {
                const auto left = MeasureChatText(context.Design(), this->DisplayDraft(text.substr(start, std::max(start, selectionBegin) - start)), profile.style);
                const auto right = MeasureChatText(context.Design(), this->DisplayDraft(text.substr(start, selectionEnd - start)), profile.style);
                context.Design().FillRect({input.x + std::min(left, input.width), input.y, std::max(0.0f, std::min(right, input.width) - left), input.height}, FadeColor(theme.colors.selected, reveal));
            }
            if (text.empty()) context.DrawText(this->channel_ == 7 ? i18n::Text("Player message") : i18n::Text("Message"), input, input, FadeText(profile, 0.4f));
            else
            {
                const auto commandEnd = text.find(' ');
                const auto prefixEnd = commandEnd == std::string::npos ? text.size() : commandEnd;
                const auto mode = CommandMode(std::string_view{text}.substr(0, prefixEnd));
                const auto nativeColor = this->Colors().Resolve(mode, ChatThemeColor(mode, snapshot.chat_colors.modes[mode], context.Design().IsXiv1Theme()));
                if (mode != 0 && nativeColor != 0 && start < prefixEnd)
                {
                    const auto command = text.substr(start, prefixEnd - start);
                    const float width = context.Design().MeasureText(command.c_str(), profile.style);
                    context.DrawText(command.c_str(), input, input, TintTextProfile(profile, nativeColor));
                    const ui::rect rest{input.x + width, input.y, std::max(0.0f, input.width - width), input.height};
                    DrawThemedChatText(context, this->DisplayDraft(text.substr(prefixEnd)), rest, profile);
                }
                else DrawThemedChatText(context, this->DisplayDraft(text.substr(start)), input, profile);
            }
            if (this->Focused() && (::GetTickCount64() / 500) % 2 == 0)
            {
                const auto caret = MeasureChatText(context.Design(), this->DisplayDraft(text.substr(start, this->editor_.Caret() - start)), profile.style);
                context.Design().FillRect({input.x + caret, input.y + 2.0f, 1.0f, input.height - 4.0f}, FadeColor(theme.colors.text, reveal));
            }
            std::string hint = this->error_;
            if (this->ControllerComposing() && hint.empty()) hint = this->controller_channel_adjusting_ ?
                "Left/Right: channel   Confirm: done   Cancel: back" : "Left/Right: action   Confirm: choose   Actions: history";
            if (hint.empty() && tab.scroll > 0) hint = "Scrollback - Ctrl+End returns to latest";
            if (!hint.empty())
            {
                const float bottom = this->ControllerComposing() ? layout.controller_actions[0].y : layout.input.y;
                const ui::rect hintBounds{layout.log.x, bottom - 23.0f, layout.log.width, 21.0f};
                context.Design().FillRect(hintBounds, FadeColor(theme.colors.surface_raised, reveal));
                context.DrawText(hint.c_str(), hintBounds, hintBounds, FadeText(ChatThemeProfile(context.Design(), this->chrome_), reveal));
            }
        }
        if (layout.destination.height > 0 && this->destination_reveal_ > 0)
        {
            const float reveal = InputEase(this->destination_reveal_);
            auto bounds = layout.destination; bounds.y += 4 * (1 - reveal);
            const ui::rect pill{bounds.x - 3, bounds.y + 1, bounds.width + 6, std::max(0.0f, bounds.height - 2)};
            context.Design().FillRoundedRect(pill, FadeColor(this->destination_hovered_ || this->channel_picker_open_ ? 0x68000000 : 0x50000000, reveal), pill.height * 0.5f);
            const auto destination = this->InputDestination();
            auto profile = ChatThemeProfile(context.Design(), this->chrome_); profile.style.alignment = ui::text_alignment::center;
            profile.placement = 0; profile.offset_x = profile.offset_y = 0;
            if (destination.mode != 0)
            {
                const auto colorMode = destination.mode == 213 ? 6u : destination.mode;
                auto color = this->Colors().Resolve(colorMode,
                    ChatThemeColor(colorMode, snapshot.chat_colors.modes[colorMode], context.Design().IsXiv1Theme()));
                if (destination.mode == 213)
                    color = (color & 0xFF000000) | (((color >> 16) & 255) * 9 / 10 << 16) |
                        (((color >> 8) & 255) * 9 / 10 << 8) | ((color & 255) * 9 / 10);
                if (color != 0) profile = TintTextProfile(profile, color);
            }
            const auto label = ui::FitText(context.Design(), ChatDestinationTag(destination), bounds.width, profile.style);
            context.DrawText(label.c_str(), bounds, bounds, FadeText(profile, reveal));
        }
        if (layout.counter.height > 0)
        {
            const auto [used, maximum] = this->InputUsage(this->editor_.Text());
            char count[32]{}; std::snprintf(count, sizeof(count), "%zu/%zu", used, maximum);
            auto profile = ChatThemeProfile(context.Design(), this->chrome_); profile.style.alignment = ui::text_alignment::right;
            if (used >= maximum) profile = TintTextProfile(profile, 0xFFFF8C8C);
            const float reveal = InputEase(this->input_reveal_);
            auto counter = layout.counter; counter.y += 4 * (1 - reveal);
            context.DrawText(count, counter, counter, FadeText(profile, reveal));
        }
        if (this->ControllerComposing())
        {
            const auto channel = this->controller_channel_adjusting_ ? std::string{"< "} + channels[this->channel_] + " >" : this->InputDestination().label;
            const std::array<const char*, 3> labels{channel.c_str(), "Send", "Back"};
            auto profile = ChatThemeProfile(context.Design(), this->chrome_); profile.style.alignment = ui::text_alignment::center;
            for (size_t index = 0; index < layout.controller_actions.size(); ++index)
                context.DrawButton(layout.controller_actions[index], labels[index], index == this->controller_compose_action_, profile);
        }
        this->DrawCompletions(layout, context);
        if ((this->hovered_ || context.IsEditing()) && !this->MatchingWindow1())
        {
            const auto grip = this->Grip(context.Design());
            context.Design().DrawTriangle(grip.x + 3, grip.y + 3, grip.x + 15, grip.y + 3, grip.x + 15, grip.y + 15, theme.colors.accent);
        }
        if (this->hovered_ && !this->channel_picker_open_ && this->destination_reveal_ >= 1 &&
            !context.IsEditing() && !this->pointer_down_ && !this->resizing_ &&
            layout.destination.height > 0 && Contains(layout.destination, this->hover_point_.x, this->hover_point_.y))
        {
            const auto destination = this->InputDestination();
            auto profile = ChatThemeProfile(context.Design(), this->chrome_); profile.style.alignment = ui::text_alignment::left;
            profile.placement = 0; profile.offset_x = profile.offset_y = 0;
            const auto viewport = context.Viewport();
            const float width = std::min(std::max(40.0f, viewport.width - 16),
                std::max(context.Design().MeasureText("Click to change channel", profile.style),
                    context.Design().MeasureText(destination.label.c_str(), profile.style)) + 26);
            const float lineHeight = context.Design().TextHeight(profile.style) + 2;
            const float height = lineHeight * 2 + 26;
            const float x = std::clamp(layout.destination.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
            const float y = std::clamp(layout.destination.y - height - 6, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
            const auto content = context.DrawTooltip({x, y, width, height});
            const auto label = ui::FitText(context.Design(), destination.label, content.width, profile.style);
            context.DrawText(label.c_str(), {content.x, content.y, content.width, lineHeight}, content, profile);
            context.DrawText(i18n::Text("Click to change channel"), {content.x, content.y + lineHeight, content.width, lineHeight}, content, FadeText(profile, 0.7f));
        }
    }
}
