#include "world_info_module.hpp"

#include <algorithm>
#include <cstdio>

namespace xiui
{
    auto world_info_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.body_text}};
        this->text_[0].style.scale = 0.85f;
        this->text_[1].style.scale = this->text_[2].style.scale = 0.72f;
    }

    auto world_info_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; const auto* data = this->data_;
        *this = {};
        this->state_.bounds = bounds; this->data_ = data;
        this->ApplyTextDefaults(theme);
    }

    auto world_info_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("world", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.world", this->state_.bounds);
        this->day_ = store.ReadBool("world", "day", true);
        this->weather_ = store.ReadBool("world", "weather", true);
        this->date_ = store.ReadBool("world", "date", false);
        this->selected_text_ = std::clamp(store.ReadInt("world", "selected_text", 0), 0, 2);
        constexpr const char* sections[]{"text.world.time", "text.world.day", "text.world.weather"};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, sections[i], this->text_[i]);
        if (store.ReadInt("world", "layout_version", 0) < 2)
        {
            if (this->state_.bounds.width == 236) this->state_.bounds.width = 160;
            constexpr float oldScales[]{0.95f, 0.78f, 0.82f}, compactScales[]{0.85f, 0.72f, 0.72f};
            for (size_t i = 0; i < this->text_.size(); ++i)
                if (std::abs(this->text_[i].style.scale - oldScales[i]) < 0.001f) this->text_[i].style.scale = compactScales[i];
        }
    }

    auto world_info_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("world", "enabled", this->state_.enabled);
        store.WriteRect("layout.world", this->state_.bounds);
        store.WriteBool("world", "day", this->day_);
        store.WriteBool("world", "weather", this->weather_);
        store.WriteBool("world", "date", this->date_);
        store.WriteBool("world", "background", false);
        store.WriteInt("world", "layout_version", 2);
        store.WriteInt("world", "selected_text", this->selected_text_);
        constexpr const char* sections[]{"text.world.time", "text.world.day", "text.world.weather"};
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, sections[i], this->text_[i]);
    }

    auto world_info_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Show the Vana'diel weekday, weather and date."},
            {"Layout", "Overall width of the compact world information rows."},
            {"Text", "Choose Time, Day / date or Weather and edit its text profile."}};
        return sections;
    }

    auto world_info_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index >= 5) return section == 2;
        if (index == 1) return section == 1;
        return section == 0;
    }

    auto world_info_module::Option(const size_t index) -> module_option
    {
        constexpr static const char* profiles[]{"Time", "Day / date", "Weather"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Movable Vana'diel time and current weather.", &this->state_.enabled);
        case 1: return ModuleWidthOption(this->state_);
        case 2: return module_option::Toggle("Weekday", "Show the Vana'diel weekday and its native icon.", &this->day_);
        case 3: return module_option::Toggle("Weather", "Show the client's current weather and native element icons.", &this->weather_);
        case 4: return module_option::Toggle("Date", "Show Vana'diel year, month, and day.", &this->date_);
        case 5: return module_option::Element("Selected text", "Text profile to configure.", &this->selected_text_, profiles, 3);
        default: return TextProfileOption(index - 6, this->text_[this->selected_text_]);
        }
    }

    auto world_info_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        world_state preview{};
        preview.time_valid = preview.weather_valid = true;
        preview.time = {1510, 4, 12, 2, 18, 36}; preview.weather = 6;
        const auto& data = this->data_ != nullptr && (this->data_->time_valid || this->data_->weather_valid) ? *this->data_ : preview;
        const auto rowHeight = [&](const size_t profile) { return std::max(14.0f, context.Design().TextHeight(this->text_[profile].style) + 2.0f); };
        const float primaryHeight = std::max(rowHeight(0), this->day_ ? rowHeight(1) : 0.0f);
        this->state_.bounds.height = primaryHeight + (this->weather_ ? rowHeight(2) + 1 : 0) + (this->date_ ? rowHeight(1) + 1 : 0);
        const auto& content = this->state_.bounds;
        const auto icon = [&](const int element, const float x, const float y, const float size) {
            auto* assets = context.NativeAssets();
            if (assets != nullptr && element >= 0 && element < 8)
                context.Design().DrawTexture(assets->Texture(static_cast<ui::native_ui_image>(2 + element)), {x, y, size, size});
        };
        char time[20]{};
        if (data.time_valid) std::snprintf(time, sizeof(time), "%02u:%02u", data.time.hour, data.time.minute);
        else std::snprintf(time, sizeof(time), "--:--");
        const float timeWidth = context.Design().MeasureText(time, this->text_[0].style) + 2;
        const ui::rect timeBounds{this->day_ ? content.x + content.width - timeWidth : content.x, content.y, timeWidth, primaryHeight};
        context.DrawText(time, timeBounds, timeBounds, this->text_[0]);
        if (this->day_ && data.time_valid)
        {
            icon(static_cast<int>(data.time.weekday), content.x, content.y + (primaryHeight - 14) * 0.5f, 14);
            const ui::rect dayBounds{content.x + 18, content.y, std::max(0.0f, content.width - timeWidth - 22), primaryHeight};
            context.DrawText(vana_days[data.time.weekday % 8], dayBounds, dayBounds, this->text_[1]);
        }
        float y = content.y + primaryHeight + 1;
        if (this->weather_)
        {
            const auto element = WeatherElement(data.weather);
            const bool doubled = data.weather_valid && element >= 0 && (data.weather & 1) != 0;
            if (data.weather_valid) icon(element, content.x, y + (rowHeight(2) - 14) * 0.5f, 14);
            if (doubled) icon(element, content.x + 12, y + (rowHeight(2) - 14) * 0.5f, 14);
            const float inset = element < 0 ? 0.0f : doubled ? 30.0f : 18.0f;
            const ui::rect weatherBounds{content.x + inset, y, content.width - inset, rowHeight(2)};
            context.DrawText(data.weather_valid && data.weather >= 0 && data.weather < static_cast<int>(weather_names.size()) ? weather_names[data.weather] : "Weather unavailable", weatherBounds, weatherBounds, this->text_[2]);
            y += rowHeight(2) + 1;
        }
        if (this->date_ && data.time_valid)
        {
            char date[48]{}; std::snprintf(date, sizeof(date), "%04u/%02u/%02u", data.time.year, data.time.month, data.time.day);
            const ui::rect dateBounds{content.x, y, content.width, rowHeight(1)};
            context.DrawText(date, dateBounds, dateBounds, this->text_[1]);
        }
    }
}
