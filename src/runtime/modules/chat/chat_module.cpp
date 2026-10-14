#include "chat_module.hpp"
#include "chat/chat_encoding.hpp"
#include "chat/chat_token.hpp"
#include "chat/chat_text.hpp"
#include "resources/auto_translate.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <charconv>
#include <fstream>

namespace
{
    constexpr const char* channels[]{"/s", "/p", "/l", "/l2", "/sh", "/y", "/u", "/t", "/r"};
    constexpr const char* commands[]{"/say ", "/party ", "/linkshell ", "/linkshell2 ", "/shout ", "/yell ", "/unity ", "/tell ", "/reply "};
    constexpr uint32_t channelModes[]{1, 5, 6, 213, 2, 3, 211, 4, 4};
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
        std::string report{"[XIUI Native] Captured chat colors:"};
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
        if (secondary)
        {
            this->state_.enabled = false;
            this->state_.bounds = {-1, -1, 560, 260};
            this->source_ = 1;
            this->active_tab_ = static_cast<int>(chat_tab::window2);
        }
    }

    auto chat_module::AttachSecondWindow(chat_module& second) -> void
    {
        this->second_window_ = &second;
        second.shared_model_ = &this->model_;
        second.parent_ = this;
    }

    auto chat_module::EnsureWindowDefaults(void) -> void
    {
        if (!this->secondary_ || this->defaults_initialized_ || this->parent_ == nullptr) return;
        this->state_.bounds.width = this->parent_->state_.bounds.width;
        this->state_.bounds.height = this->parent_->state_.bounds.height;
        this->state_.content_bounds = {};
        this->container_ = this->parent_->container_;
        this->text_ = this->parent_->text_;
        this->chrome_ = this->parent_->chrome_;
        this->line_spacing_ = this->parent_->line_spacing_;
        this->timestamp_display_ = this->parent_->timestamp_display_;
        this->visible_tabs_ = this->parent_->visible_tabs_;
        this->channel_ = this->parent_->channel_;
        this->cache_revision_ = UINT64_MAX;
        this->defaults_initialized_ = true;
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
        this->input_reveal_ = 0; this->input_animation_tick_ = 0;
        this->state_.enabled = !this->secondary_;
        this->state_.bounds.width = 560.0f;
        this->state_.bounds.height = 260.0f;
        this->defaults_initialized_ = false;
        this->container_ = {};
        this->text_ = {};
        this->ApplyTextDefaults(theme);
        this->channel_ = 0;
        this->source_ = this->secondary_ ? 1 : 0;
        this->active_tab_ = static_cast<int>(this->secondary_ ? chat_tab::window2 : chat_tab::window1);
        this->visible_tabs_ = {};
        this->timestamp_display_ = 0;
        this->line_spacing_ = 2.0f;
        if (!this->secondary_) { this->alerts_ = {}; this->colors_ = {}; }
        this->cache_revision_ = UINT64_MAX;
    }

    auto chat_module::Load(const settings_store& store) -> void
    {
        if (!this->secondary_) { this->alerts_.Load(store); this->colors_.Load(store); }
        this->state_.enabled = store.ReadBool(this->Id(), "enabled", !this->secondary_);
        this->state_.bounds = store.ReadRect(this->secondary_ ? "layout.chat2" : "layout.chat", this->state_.bounds);
        this->container_ = {.opacity = std::clamp(store.ReadFloat(this->Id(), "opacity", 1.0f), 0.1f, 1.0f),
            .padding = std::clamp(store.ReadFloat(this->Id(), "padding", 8.0f), 4.0f, 24.0f)};
        this->line_spacing_ = std::clamp(store.ReadFloat(this->Id(), "line_spacing", 2.0f), 0.0f, 10.0f);
        this->timestamp_display_ = std::clamp(store.ReadInt(this->Id(), "timestamp_display", store.ReadBool(this->Id(), "timestamps", false) ? 2 : 0), 0, 3);
        const bool legacy = store.ReadInt(this->Id(), "layout_version", 0) < 3;
        this->source_ = legacy ? (this->secondary_ ? 1 : 0) : std::clamp(store.ReadInt(this->Id(), "source", this->secondary_ ? 1 : 0), 0, 2);
        this->active_tab_ = static_cast<int>(this->source_ == 0 ? chat_tab::window1 : this->source_ == 1 ? chat_tab::window2 : chat_tab::all);
        this->visible_tabs_[0] = !legacy && store.ReadBool(this->Id(), "tab_window1", false);
        this->visible_tabs_[1] = !legacy && store.ReadBool(this->Id(), "tab_window2", false);
        this->channel_ = std::clamp(store.ReadInt(this->Id(), "channel", 0), 0, static_cast<int>(std::size(channels)) - 1);
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
        if (!this->state_.enabled) this->Focus(false);
    }

    auto chat_module::Save(const settings_store& store) const -> void
    {
        if (!this->secondary_) { this->alerts_.Save(store); this->colors_.Save(store); }
        store.WriteBool(this->Id(), "enabled", this->state_.enabled);
        store.WriteRect(this->secondary_ ? "layout.chat2" : "layout.chat", this->state_.bounds);
        store.WriteFloat(this->Id(), "opacity", this->container_.opacity);
        store.WriteFloat(this->Id(), "padding", this->container_.padding);
        store.WriteFloat(this->Id(), "line_spacing", this->line_spacing_);
        store.WriteInt(this->Id(), "timestamp_display", this->timestamp_display_);
        store.WriteBool(this->Id(), "timestamps", this->timestamp_display_ >= 2);
        store.WriteInt(this->Id(), "active_tab", this->active_tab_);
        store.WriteInt(this->Id(), "source", this->source_);
        store.WriteBool(this->Id(), "tab_window1", this->visible_tabs_[0]);
        store.WriteBool(this->Id(), "tab_window2", this->visible_tabs_[1]);
        store.WriteInt(this->Id(), "layout_version", 4);
        if (this->secondary_) store.WriteBool(this->Id(), "defaults_initialized", this->defaults_initialized_);
        store.WriteInt(this->Id(), "channel", this->channel_);
        SaveTextProfile(store, this->secondary_ ? "chat2.text" : "chat.text", this->text_);
        SaveTextProfile(store, this->secondary_ ? "chat2.chrome" : "chat.chrome", this->chrome_);
    }

    auto chat_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Window visibility, default send channel and the optional second window."},
            {"Messages", "Native message filters, timestamps and optional window tabs."},
            {"Layout", "Window dimensions, inner padding and message spacing."},
            {"Appearance", "Chat container opacity using the game's Window Type."},
            {"Text", "Message text size, colors, effects and offsets."},
            {"Sounds", "Choose a sound for each chat type and keyword alerts. Shared by both windows."},
            {"Colors", "XIUI colors override your game settings colors. Choose Game to restore native colors."}};
        return sections;
    }

    auto chat_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= this->ColorsOptionStart()) return section == 6;
        if (index >= this->SoundsOptionStart()) return section == 5;
        const size_t textStart = this->secondary_ ? 10 : 12;
        if (index >= textStart) return section == 4;
        if (index == 1 || index == 2 || index == 4 || index == 6) return section == 2;
        if (index == 3) return section == 3;
        if (index == 0 || (!this->secondary_ && (index == 7 || index == 11))) return section == 0;
        return section == 1;
    }

    auto chat_module::Option(const size_t index) -> module_option
    {
        this->EnsureWindowDefaults();
        if (index >= this->ColorsOptionStart() && index < this->OptionCount())
        {
            const auto type = index - this->ColorsOptionStart();
            return module_option::OverrideColor(chat_types[type].label, "XIUI overrides your game settings colors. Game restores native colors.",
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
        case 0: return module_option::Toggle("Enabled", "Show XIUI chat. Game HUD controls the native chat windows.", &this->state_.enabled);
        case 1: return ModuleWidthOption(this->state_);
        case 2: return module_option::Slider("Height", "Also resize from the top-right corner.", &this->state_.bounds.height, 160.0f, 900.0f, 1.0f, module_option_group::appearance);
        case 3: return module_option::Slider("Opacity", "Native window background opacity.", &this->container_.opacity, 0.1f, 1.0f, 0.05f, module_option_group::appearance);
        case 4: return module_option::Slider("Padding", "Space inside the chat frame.", &this->container_.padding, 4.0f, 24.0f, 1.0f, module_option_group::appearance);
        case 5:
        {
            static constexpr const char* modes[]{"Follow game", "Off", "HH:MM", "HH:MM:SS"};
            return module_option::Dropdown("Timestamps", "Follow the game's Windows timestamp setting, or override it for this XIUI window. Addon timestamps are not duplicated.",
                &this->timestamp_display_, modes, std::size(modes));
        }
        case 6: return module_option::Slider("Line spacing", "Extra space between wrapped lines.", &this->line_spacing_, 0.0f, 10.0f, 1.0f, module_option_group::appearance);
        default: break;
        }
        if (!this->secondary_ && index == 7)
            return module_option::Choice("Send channel", "Default channel. /t takes a player name before the message.", &this->channel_, channels, std::size(channels));
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

    auto chat_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == this->SoundsOptionStart()) this->Alerts().UpdateKeywords();
        if (!this->secondary_ && index == 11 && this->second_window_ != nullptr && this->second_window_->state_.enabled)
            this->second_window_->EnsureWindowDefaults();
        if (index == (this->secondary_ ? 7u : 8u))
            this->active_tab_ = static_cast<int>(this->source_ == 0 ? chat_tab::window1 : this->source_ == 1 ? chat_tab::window2 : chat_tab::all);
        if (!this->state_.enabled) this->Focus(false);
    }

    auto chat_module::PreviewChoice(const size_t index, const size_t choice) -> bool
    {
        return index > this->SoundsOptionStart() && index < this->ColorsOptionStart() && choice > 0 &&
            choice <= std::size(chat_sound_values) && this->sounds_ != nullptr && this->sounds_->Play(chat_sound_values[choice - 1]);
    }

    auto chat_module::Focus(const bool focused) -> void
    {
        this->history_view_.focused = this->history_view_.expanded = false;
        this->focused_ = focused && this->state_.enabled && !this->secondary_;
        if (!this->focused_) this->editor_.ResetCompletion();
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
            static_cast<float>(std::min<uint64_t>(now - this->input_animation_tick_, 140)) / 140.0f : 0;
        this->input_animation_tick_ = now;
        this->input_reveal_ = this->secondary_ ? 0 : std::clamp(this->input_reveal_ + (this->Focused() ? elapsed : -elapsed), 0.0f, 1.0f);
    }

    auto chat_module::Restore(const runtime_state& state) -> void
    {
        this->editor_.Set(std::string{state.chat_draft, strnlen_s(state.chat_draft, sizeof(state.chat_draft))}, state.chat_caret);
        this->Focus(state.chat_focused != FALSE);
    }

    auto chat_module::Clear(void) -> void
    {
        this->model_.Clear();
        this->tabs_ = {};
        this->history_view_.scroll = {};
        this->scrollbar_until_ = 0;
        if (this->second_window_ != nullptr) { this->second_window_->tabs_ = {}; this->second_window_->scrollbar_until_ = 0; }
    }

    auto chat_module::Receive(IAshitaCore* core, const chat_event& event, const bool audible) -> void
    {
        if (this->shared_model_ != nullptr) return;
        const auto duplicate = !this->model_.Lines().empty() && this->model_.Lines().back().sequence == event.sequence;
        const uint32_t previousWindows = duplicate ? this->model_.Lines().back().windows : 0;
        this->model_.Add(event, duplicate ? std::string{} : ExpandChatText(core != nullptr ? core->GetChatManager() : nullptr, event.text));
        if (audible && !duplicate && this->state_.enabled && this->sounds_ != nullptr && !this->model_.Lines().empty() &&
            this->model_.Lines().back().sequence == event.sequence)
        {
            const auto sound = this->alerts_.SoundFor(this->model_.Lines().back());
            if (sound > 0 && sound <= static_cast<int>(std::size(chat_sound_values))) this->sounds_->Play(chat_sound_values[sound - 1]);
        }
        this->MarkUnread(event, previousWindows);
        if (this->second_window_ != nullptr) this->second_window_->MarkUnread(event, previousWindows);
    }

    auto chat_module::MarkUnread(const chat_event& event, const uint32_t previousWindows) -> void
    {
        for (size_t index = 0; index < this->tabs_.size(); ++index)
            if (ChatTabMatches(static_cast<chat_tab>(index), event.mode, event.windows) &&
                !ChatTabMatches(static_cast<chat_tab>(index), event.mode, previousWindows) &&
                (static_cast<int>(index) != this->active_tab_ || this->tabs_[index].scroll > 0)) ++this->tabs_[index].unread;
    }

    auto chat_module::Grip(void) const -> ui::rect
    { return {this->state_.bounds.x + this->state_.bounds.width - 18.0f, this->state_.bounds.y, 18.0f, 18.0f}; }

    auto chat_module::VisualBounds(const ui::design_system& design) const -> ui::rect
    {
        const auto layout = this->Layout(design);
        const auto& frame = this->state_.bounds;
        float top = frame.y;
        for (const auto& tab : layout.tabs) if (tab.height > 0) top = std::min(top, tab.y);
        return {frame.x, top, frame.width, frame.y + frame.height - top};
    }

    auto chat_module::HitTest(const ui::design_system& design, const float x, const float y) const -> bool
    {
        if (this->HistoryExpanded()) return Contains(this->history_view_.viewport, x, y);
        if (Contains(this->state_.bounds, x, y)) return true;
        const auto layout = this->Layout(design);
        return std::any_of(layout.tabs.begin(), layout.tabs.end(), [x, y](const auto& tab) { return Contains(tab, x, y); });
    }

    auto chat_module::SoundTarget(const ui::design_system& design, const float x, const float y, const bool editing) const -> uint32_t
    {
        if (this->HistoryExpanded()) return this->HistorySoundTarget(design, x, y);
        if (!this->WindowVisible() && (!editing || this->secondary_)) return 0;
        const auto layout = this->Layout(design);
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
        if (!this->secondary_ && Contains(layout.channel, x, y)) return 10;
        if (!this->secondary_ && Contains(layout.input, x, y)) return 11;
        return 0;
    }

    auto chat_module::ChannelProfile(void) const -> module_text_profile
    {
        auto profile = this->text_;
        profile.style.scale *= 0.8f;
        profile.placement = 0; profile.offset_x = profile.offset_y = 0;
        profile.style.alignment = ui::text_alignment::center;
        return profile;
    }

    auto chat_module::Layout(const ui::design_system& design) const -> layout
    {
        layout result{};
        result.content = ui::container_renderer::ContentBounds(this->state_.bounds, this->container_);
        float x = result.content.x;
        for (size_t window = 0; window < this->visible_tabs_.size(); ++window)
        {
            if (!this->visible_tabs_[window]) continue;
            const size_t tab = static_cast<size_t>(chat_tab::window1) + window;
            const float width = design.MeasureText(chat_tab_names[tab], this->chrome_.style) + 22.0f;
            result.tabs[tab] = {x, this->state_.bounds.y - 28, width, 24};
            x += width + 2;
        }
        const float inputHeight = this->secondary_ || (!this->Focused() && this->input_reveal_ == 0) ? 0 : std::max(26.0f, design.TextHeight(this->text_.style) + 6.0f);
        result.input = {result.content.x, result.content.y + result.content.height - inputHeight, result.content.width, inputHeight};
        const bool command = !this->editor_.Text().empty() && this->editor_.Text().front() == '/';
        const float prefixWidth = command || inputHeight == 0 ? 0.0f : std::max(28.0f, design.MeasureText(channels[this->channel_], this->ChannelProfile().style) + 10.0f);
        result.channel = {result.input.x, result.input.y, prefixWidth, inputHeight};
        result.input.x += prefixWidth; result.input.width = std::max(1.0f, result.input.width - prefixWidth);
        result.log = {result.content.x + 2.0f, result.content.y, result.content.width - 14.0f, std::max(1.0f, result.input.y - result.content.y - (inputHeight > 0 ? 5.0f : 0.0f))};
        result.scrollbar = {result.log.x + result.log.width + 4.0f, result.log.y, 6.0f, result.log.height};
        if (this->Focused() && !this->editor_.Completions().empty())
        {
            result.completion_row_height = std::max(22.0f, design.TextHeight(this->chrome_.style) + 6);
            result.completion_rows = std::min(this->editor_.Completions().size(), static_cast<size_t>(std::clamp(static_cast<int>((result.log.height - 44) / result.completion_row_height), 1, 8)));
            const float height = result.completion_rows * result.completion_row_height + 44;
            result.completion = {result.content.x, result.input.y - height - 3, result.content.width, height};
        }
        return result;
    }

    auto chat_module::Scroll(const int amount) -> void
    {
        if (amount != 0) this->scrollbar_until_ = ::GetTickCount64() + 1200;
        auto& scroll = this->CurrentScroll();
        if (amount > 0) scroll = std::min(scroll + amount, this->rows_.size());
        else scroll -= std::min(scroll, static_cast<size_t>(-amount));
        if (scroll == 0 && !this->HistoryExpanded()) this->tabs_[this->active_tab_].unread = 0;
    }

    auto chat_module::ScrollbarOpacity(const uint64_t now) const -> float
    { return now >= this->scrollbar_until_ ? 0 : std::min(1.0f, static_cast<float>(this->scrollbar_until_ - now) / 200.0f); }

    auto chat_module::HandleMouse(const uint32_t message, const WPARAM wParam, const float x, const float y, const bool editing,
        const ui::design_system& design, const ui::rect& viewport, bool& save) -> bool
    {
        if (!this->WindowVisible() && (!editing || this->secondary_)) { this->CancelPointer(); return false; }
        if (this->HistoryExpanded()) return this->HandleHistoryMouse(message, wParam, x, y, design);
        const bool down = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK;
        if (down && !this->pointer_down_ && this->sounds_ != nullptr && this->SoundTarget(design, x, y, editing) != 0)
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
                this->state_.bounds.y = bottom - this->state_.bounds.height;
                this->state_.content_bounds = {};
            }
            if (message == WM_LBUTTONUP) { this->resizing_ = false; this->pointer_down_ = false; save = true; }
            return true;
        }
        if (down && Contains(this->Grip(), x, y))
        {
            this->resizing_ = this->pointer_down_ = true;
            this->resize_bounds_ = this->state_.bounds;
            this->resize_x_ = x; this->resize_y_ = y;
            return true;
        }
        if (message == WM_LBUTTONUP && this->pointer_down_) { this->pointer_down_ = false; return true; }
        if (!this->HitTest(design, x, y)) { if (down) this->Focus(false); return false; }
        const auto layout = this->Layout(design);
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
        if (editing && (!down || this->secondary_ || (!Contains(layout.input, x, y) && !Contains(layout.channel, x, y))))
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
        if (Contains(layout.channel, x, y)) { this->channel_ = (this->channel_ + 1) % std::size(channels); save = true; return true; }
        if (Contains(layout.input, x, y))
        {
            this->Focus(true);
            const auto text = this->editor_.Text();
            this->input_start_ = std::min(this->input_start_, text.size());
            size_t caret = std::min(this->input_start_, text.size());
            const float local = x - layout.input.x - 5.0f;
            while (caret < text.size())
            {
                size_t next = caret; NextDraft(text, next);
                const float previousWidth = MeasureChatText(design, this->DisplayDraft(text.substr(this->input_start_, caret - this->input_start_)), this->text_.style);
                const float nextWidth = MeasureChatText(design, this->DisplayDraft(text.substr(this->input_start_, next - this->input_start_)), this->text_.style);
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
            "/heal", "/sit", "/logout", "/shutdown", "/xiuin", "/addon", "/load", "/unload", "/exec", "/wait", "/bind", "/unbind", "/alias", "/unalias",
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
        const auto report = "[XIUI Native] Auto-translate: " + std::to_string(this->auto_translate_labels_.size()) + " cached; " + std::to_string(count) + " matches (" +
            std::to_string(verified) + " native token round-trips): " + names;
        std::string encoded;
        return ToGameText(report, encoded) ? encoded : report;
    }

    auto chat_module::Submit(IChatManager* chat) -> bool
    {
        if (this->editor_.Text().empty()) { this->Focus(false); return true; }
        if (chat == nullptr) { this->error_ = "Chat is unavailable."; return false; }
        const auto command = this->editor_.Text().front() == '/' ? this->editor_.Text() : std::string{commands[this->channel_]} + this->editor_.Text();
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
        const auto nativeLimit = chat->GetInputTextParsedLengthMax();
        const auto limit = nativeLimit > 0 ? std::min(nativeLimit, 511u) : 255u;
        if (encoded.size() > limit) { this->error_ = "Message exceeds the game's input limit."; return false; }
        chat->QueueCommand(-1, encoded.c_str());
        if (this->sounds_ != nullptr) this->sounds_->Play(ui::native_ui_sound::chat_send);
        this->editor_.Set(command);
        this->editor_.Remember();
        this->editor_.Set({});
        this->error_.clear();
        this->Focus(false);
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
            const auto format = ::RegisterClipboardFormatA("XIUI.ChatDraft");
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
        const auto format = ::RegisterClipboardFormatA("XIUI.ChatDraft");
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
        this->error_.clear();
        this->input_start_ = 0;
        this->Focus(true);
        return true;
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
        this->editor_.Set(std::move(draft.text), draft.caret);
        this->error_.clear();
        this->input_start_ = 0;
        this->Focus(true);
        return true;
    }

    auto chat_module::HandleKey(IAshitaCore* core, const WPARAM key, const LPARAM flags, const bool blocked, const bool replaceNative, const std::string_view replyTarget) -> bool
    {
        const bool released = (static_cast<uint32_t>(flags) & 0x80000000u) != 0;
        if (released || !this->state_.enabled) return false;
        const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool alt = (::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0;
        if (key == 'R' && alt && !control && !shift && !blocked && replaceNative && (static_cast<uint32_t>(flags) & 0x40000000u) == 0)
            return this->OpenTell(replyTarget);
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
            return false;
        }
        const HWND window = core != nullptr && core->GetProperties() != nullptr ? core->GetProperties()->GetFinalFantasyHwnd() : nullptr;
        this->error_.clear();
        if (key == VK_RETURN && (static_cast<uint32_t>(flags) & 0x40000000u) != 0) return true;
        if (alt && key == VK_TAB) { this->Focus(false); return false; }
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
                BYTE state[256]{};
                ::GetKeyboardState(state);
                if (key < 256) state[key] |= 0x80;
                wchar_t text[8]{};
                const auto count = ::ToUnicodeEx(static_cast<UINT>(key), (static_cast<uint32_t>(flags) >> 16) & 0xFF, state, text, 8, 4, ::GetKeyboardLayout(0));
                if (count > 0) this->editor_.Insert(ToUtf8({text, static_cast<size_t>(count)}));
            }
        }
        return true;
    }

    auto chat_module::RebuildRows(const ui::design_system& design, const float width) -> void
    {
        const int timestampMode = this->TimestampMode();
        const int filter = this->RowFilter();
        if (this->cache_revision_ == this->History().Revision() && this->cache_width_ == width && this->cache_scale_ == this->text_.style.scale &&
            this->cache_spacing_ == this->text_.style.letter_spacing && this->cache_font_ == this->text_.style.font && this->cache_tab_ == filter && this->cache_timestamp_mode_ == timestampMode) return;
        const bool incremental = this->cache_revision_ != UINT64_MAX && this->cache_revision_ != this->History().Revision() && this->cache_tab_ == filter && this->cache_width_ == width &&
            this->cache_scale_ == this->text_.style.scale && this->cache_spacing_ == this->text_.style.letter_spacing && this->cache_font_ == this->text_.style.font && this->cache_timestamp_mode_ == timestampMode && !this->History().Lines().empty();
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
            if (this->HistoryExpanded() ? !ChatHistoryMatches(this->history_view_.tab, line.mode, line.windows) :
                !ChatTabMatches(static_cast<chat_tab>(this->active_tab_), line.mode, line.windows)) continue;
            rendered_row row{line.sequence, line.mode, line.color};
            float used{};
            const auto finish = [&] { this->rows_.push_back(std::move(row)); row = {line.sequence, line.mode, line.color}; used = 0.0f; };
            const auto append = [&](const std::string& token, const int color) {
                const auto tokenWidth = MeasureChatText(design, token, this->text_.style) + this->text_.style.letter_spacing * this->text_.style.scale;
                if (used > 0.0f && used + tokenWidth > width) finish();
                if (tokenWidth <= width)
                {
                    if (!row.spans.empty() && row.spans.back().color == color) { row.spans.back().text += token; row.spans.back().width += tokenWidth; }
                    else row.spans.push_back({token, color, tokenWidth});
                    used += tokenWidth;
                    return;
                }
                for (size_t pos = 0; pos < token.size();)
                {
                    const size_t begin = pos;
                    NextUtf8(token, pos);
                    const auto glyph = token.substr(begin, pos - begin);
                    const auto glyphWidth = MeasureChatText(design, glyph, this->text_.style) + this->text_.style.letter_spacing * this->text_.style.scale;
                    if (used > 0 && used + glyphWidth > width) finish();
                    if (!row.spans.empty() && row.spans.back().color == color) { row.spans.back().text += glyph; row.spans.back().width += glyphWidth; }
                    else row.spans.push_back({glyph, color, glyphWidth});
                    used += glyphWidth;
                }
            };
            const auto appendTimestamp = [&] {
                if (timestampMode == 0) return;
                SYSTEMTIME now{}; ::GetLocalTime(&now);
                const auto elapsed = static_cast<uint32_t>((::GetTickCount64() - std::min(::GetTickCount64(), line.timestamp)) / 1000) % 86400;
                const auto seconds = (now.wHour * 3600u + now.wMinute * 60u + now.wSecond + 86400u - elapsed) % 86400u;
                char timestamp[20]{};
                if (timestampMode == 2) std::snprintf(timestamp, sizeof(timestamp), "[%02u:%02u:%02u] ", seconds / 3600, seconds / 60 % 60, seconds % 60);
                else std::snprintf(timestamp, sizeof(timestamp), "[%02u:%02u] ", seconds / 3600, seconds / 60 % 60);
                append(timestamp, -1);
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
                    append(token, span.color_index);
                    position = end;
                }
            if (!row.spans.empty()) finish();
        }
        if (incremental && this->CurrentScroll() > 0 && this->rows_.size() > previousRows + oldLastRows)
            this->CurrentScroll() += this->rows_.size() - previousRows - oldLastRows;
        this->cache_revision_ = this->History().Revision(); this->cache_width_ = width;
        this->cache_scale_ = this->text_.style.scale; this->cache_spacing_ = this->text_.style.letter_spacing;
        this->cache_font_ = this->text_.style.font;
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
        auto text = this->chrome_;
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
            DrawChatText(context, name, label, TintTextProfile(text, theme.colors.text));
            auto badge = text; badge.style.scale *= 0.8f; badge.style.alignment = ui::text_alignment::right;
            const auto* kind = match.kind == chat_completion_kind::auto_translate ? "Auto-translate" : match.kind == chat_completion_kind::command ? "Command" : match.kind == chat_completion_kind::player ? "Player" : "Text";
            const ui::rect badgeBounds{at.x + at.width - 90, at.y, 84, at.height};
            context.DrawText(kind, badgeBounds, badgeBounds, badge);
        }
        auto help = text; help.style.scale *= 0.78f;
        const ui::rect footer{bounds.x + 9, bounds.y + bounds.height - 20, bounds.width - 18, 16};
        context.DrawText("Tab / arrows: browse   Enter: choose   Esc: cancel", footer, footer, help);
    }

    auto chat_module::DrawLog(const hud_snapshot& snapshot, const module_render_context& context, const ui::rect& log, const ui::rect& scrollbar) -> void
    {
        const auto now = ::GetTickCount64();
        const auto& theme = context.Design().GetTheme();
        const float lineHeight = context.Design().TextHeight(this->text_.style) + this->line_spacing_;
        this->RebuildRows(context.Design(), log.width);
        const size_t visible = std::max<size_t>(1, static_cast<size_t>(log.height / lineHeight));
        this->visible_rows_ = visible;
        auto& scroll = this->CurrentScroll();
        const size_t maximum = this->rows_.size() > visible ? this->rows_.size() - visible : 0;
        scroll = std::min(scroll, maximum);
        if (scroll == 0 && !this->HistoryExpanded()) this->tabs_[this->active_tab_].unread = 0;
        const size_t end = this->rows_.size() - scroll;
        const size_t begin = end > visible ? end - visible : 0;
        float y = this->HistoryExpanded() ? log.y : log.y + log.height - static_cast<float>(end - begin) * lineHeight;
        for (size_t index = begin; index < end; ++index, y += lineHeight)
        {
            const auto& row = this->rows_[index];
            const auto configured = snapshot.chat_colors.modes[row.mode & 0xFF];
            const auto base = configured != 0 ? configured : row.color;
            float rowWidth{}; for (const auto& span : row.spans) rowWidth += span.width;
            float x = log.x;
            if (this->text_.placement == 2) x += std::max(0.0f, (log.width - rowWidth) * 0.5f);
            if (this->text_.placement == 3) x += std::max(0.0f, log.width - rowWidth);
            for (const auto& span : row.spans)
            {
                const auto inlineColor = span.color >= 0 && static_cast<size_t>(span.color) < snapshot.chat_colors.colors.size() ? snapshot.chat_colors.colors[span.color] : 0;
                const auto color = this->Colors().Resolve(row.mode, base, inlineColor);
                auto profile = TintTextProfile(this->text_, color);
                profile.placement = 0; profile.style.alignment = ui::text_alignment::left;
                const ui::rect bounds{x, y, std::max(0.0f, log.x + log.width - x), lineHeight};
                DrawChatText(context, span.text, bounds, profile);
                x += span.width;
            }
        }
        if (this->rows_.empty())
        {
            const auto text = snapshot.chat_capture_available ? (this->secondary_ ? "This window follows your game filters." : "Chat follows your game filters. Press / to chat.") : "Native chat capture unavailable. Keep game chat enabled.";
            context.DrawText(text, log, log, this->chrome_);
        }
        const float scrollbarOpacity = this->ScrollbarOpacity(now);
        if (maximum > 0 && scrollbarOpacity > 0)
        {
            const auto& track = scrollbar;
            const float thumbHeight = std::min(track.height, std::max(18.0f, track.height * static_cast<float>(visible) / static_cast<float>(this->rows_.size())));
            context.Design().FillRect({track.x + 1, track.y + (track.height - thumbHeight) * (1.0f - static_cast<float>(scroll) / static_cast<float>(maximum)), track.width - 2, thumbHeight}, FadeColor(theme.colors.accent | 0xFF000000, scrollbarOpacity));
        }
    }

    auto chat_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        if (this->HistoryExpanded()) return;
        const auto now = ::GetTickCount64();
        this->UpdateInputAnimation(now);
        context.DrawContainer(this->state_.bounds, this->container_);
        const auto layout = this->Layout(context.Design());
        const auto& theme = context.Design().GetTheme();
        for (size_t index = 0; index < layout.tabs.size(); ++index)
        {
            if (layout.tabs[index].height <= 0) continue;
            auto profile = this->chrome_;
            profile.style.alignment = ui::text_alignment::center;
            context.DrawButton(layout.tabs[index], chat_tab_names[index], static_cast<int>(index) == this->active_tab_, profile);
            const ui::rect label{layout.tabs[index].x + 5.0f, layout.tabs[index].y, layout.tabs[index].width - 10.0f, layout.tabs[index].height};
            if (this->tabs_[index].unread > 0) context.Design().FillRect({label.x + label.width - 1.0f, label.y + 3.0f, 3.0f, 3.0f}, theme.colors.accent);
        }
        this->DrawLog(snapshot, context, layout.log, layout.scrollbar);
        const auto& tab = this->tabs_[this->active_tab_];
        if (this->HistoryFocused())
        {
            context.Design().BorderRect(this->state_.bounds, theme.colors.control_selected, 1);
            auto help = this->chrome_; help.style.alignment = ui::text_alignment::right;
            const ui::rect hint{this->state_.bounds.x, this->state_.bounds.y - 24, this->state_.bounds.width, 20};
            context.DrawText("Enter: expand chat   Esc: back", hint, hint, help);
        }
        if (!this->secondary_ && this->input_reveal_ > 0)
        {
            const float reveal = this->input_reveal_ * this->input_reveal_ * (3 - 2 * this->input_reveal_);
            const float slide = 4 * (1 - reveal);
            const ui::rect inputRow{layout.channel.x, layout.input.y, layout.channel.width + layout.input.width, layout.input.height};
            context.Design().FillRect(inputRow, FadeColor(0x60000000, reveal));
            if (layout.channel.width > 0.0f)
            {
                const auto nativeColor = snapshot.chat_colors.modes[channelModes[this->channel_]];
                const auto color = this->Colors().Resolve(channelModes[this->channel_], nativeColor != 0 ? nativeColor : theme.colors.text);
                const auto prefix = FadeText(TintTextProfile(this->ChannelProfile(), color), reveal);
                const ui::rect prefixBounds{layout.channel.x + 2.0f, layout.channel.y + 2.0f + slide, layout.channel.width - 4.0f, layout.channel.height - 4.0f};
                context.DrawText(channels[this->channel_], prefixBounds, prefixBounds, prefix);
            }
            const ui::rect input{layout.input.x + 5.0f, layout.input.y + 2.0f + slide, layout.input.width - 10.0f, layout.input.height - 4.0f};
            auto profile = FadeText(this->text_, reveal); profile.placement = 0; profile.offset_x = profile.offset_y = 0.0f; profile.style.alignment = ui::text_alignment::left;
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
            if (text.empty()) context.DrawText(this->channel_ == 7 ? "Player message" : "Message", input, input, FadeText(this->chrome_, reveal));
            else
            {
                const auto commandEnd = text.find(' ');
                const auto prefixEnd = commandEnd == std::string::npos ? text.size() : commandEnd;
                const auto mode = CommandMode(std::string_view{text}.substr(0, prefixEnd));
                const auto nativeColor = this->Colors().Resolve(mode, snapshot.chat_colors.modes[mode]);
                if (mode != 0 && nativeColor != 0 && start < prefixEnd)
                {
                    const auto command = text.substr(start, prefixEnd - start);
                    const float width = context.Design().MeasureText(command.c_str(), profile.style);
                    context.DrawText(command.c_str(), input, input, TintTextProfile(profile, nativeColor));
                    const ui::rect rest{input.x + width, input.y, std::max(0.0f, input.width - width), input.height};
                    DrawChatText(context, this->DisplayDraft(text.substr(prefixEnd)), rest, profile);
                }
                else DrawChatText(context, this->DisplayDraft(text.substr(start)), input, profile);
            }
            if (this->Focused() && (::GetTickCount64() / 500) % 2 == 0)
            {
                const auto caret = MeasureChatText(context.Design(), this->DisplayDraft(text.substr(start, this->editor_.Caret() - start)), profile.style);
                context.Design().FillRect({input.x + caret, input.y + 2.0f, 1.0f, input.height - 4.0f}, FadeColor(theme.colors.text, reveal));
            }
            std::string hint = this->error_;
            if (hint.empty() && tab.scroll > 0) hint = "Scrollback - Ctrl+End returns to latest";
            if (!hint.empty())
            {
                const ui::rect hintBounds{layout.log.x, layout.input.y - 23.0f, layout.log.width, 21.0f};
                context.Design().FillRect(hintBounds, FadeColor(theme.colors.surface_raised, reveal));
                context.DrawText(hint.c_str(), hintBounds, hintBounds, FadeText(this->chrome_, reveal));
            }
        }
        this->DrawCompletions(layout, context);
        if (this->hovered_)
        {
            const auto grip = this->Grip();
            context.Design().DrawTriangle(grip.x + 3, grip.y + 3, grip.x + 15, grip.y + 3, grip.x + 15, grip.y + 15, theme.colors.accent);
        }
    }
}
