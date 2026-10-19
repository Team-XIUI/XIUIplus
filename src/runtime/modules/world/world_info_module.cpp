#include "world_info_module.hpp"
#include "ui/native_ui_assets.hpp"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr const char* part_ids[]{"world.time", "world.day", "world.weather", "world.date"};
    constexpr const char* part_names[]{"World Time", "World Weekday", "World Weather", "World Date"};
    constexpr const char* part_layouts[]{"layout.world.time", "layout.world.day", "layout.world.weather", "layout.world.date"};
}

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
        const auto bounds = this->state_.bounds; const auto parts = this->parts_; const auto* data = this->data_;
        *this = {};
        this->state_.bounds = bounds; this->data_ = data;
        for (size_t i = 0; i < this->parts_.size(); ++i) this->parts_[i].bounds = parts[i].bounds;
        this->ApplyTextDefaults(theme);
    }

    auto world_info_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("world", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.world", this->state_.bounds);
        this->time_ = store.ReadBool("world", "time", true);
        this->day_ = store.ReadBool("world", "day", true);
        this->weather_ = store.ReadBool("world", "weather", true);
        this->date_ = store.ReadBool("world", "date", false);
        this->selected_text_ = std::clamp(store.ReadInt("world", "selected_text", 0), 0, 2);
        this->layout_ = std::clamp(store.ReadInt("world", "arrangement", 0), 0, 2);
        this->selected_part_ = std::clamp(store.ReadInt("world", "selected_part", 0), 0, 3);
        for (size_t i = 0; i < this->parts_.size(); ++i)
        {
            this->parts_[i].bounds = store.ReadRect(part_layouts[i], this->parts_[i].bounds);
            this->parts_[i].content_bounds = {};
        }
        constexpr const char* sections[]{"text.world.time", "text.world.day", "text.world.weather"};
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, sections[i], this->text_[i]);
        if (store.ReadInt("world", "layout_version", 0) < 2)
        {
            if (this->state_.bounds.width == 236) this->state_.bounds.width = 160;
            constexpr float oldScales[]{0.95f, 0.78f, 0.82f}, compactScales[]{0.85f, 0.72f, 0.72f};
            for (size_t i = 0; i < this->text_.size(); ++i)
                if (std::abs(this->text_[i].style.scale - oldScales[i]) < 0.001f) this->text_[i].style.scale = compactScales[i];
        }
        this->InitializeParts();
    }

    auto world_info_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("world", "enabled", this->state_.enabled);
        store.WriteRect("layout.world", this->state_.bounds);
        store.WriteBool("world", "time", this->time_);
        store.WriteBool("world", "day", this->day_);
        store.WriteBool("world", "weather", this->weather_);
        store.WriteBool("world", "date", this->date_);
        store.WriteBool("world", "background", false);
        store.WriteInt("world", "layout_version", 2);
        store.WriteInt("world", "selected_text", this->selected_text_);
        store.WriteInt("world", "arrangement", this->layout_);
        store.WriteInt("world", "selected_part", this->selected_part_);
        for (size_t i = 0; i < this->parts_.size(); ++i) store.WriteRect(part_layouts[i], this->parts_[i].bounds);
        constexpr const char* sections[]{"text.world.time", "text.world.day", "text.world.weather"};
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, sections[i], this->text_[i]);
    }

    auto world_info_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Show the Vana'diel weekday, weather and date.", "general"},
            {"Layout", "Stack the information, keep it on one row or move each element independently.", "layout"},
            {"Text", "Choose Time, Day / date or Weather and edit its text profile.", "text"}};
        return sections;
    }

    auto world_info_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == 5 || index >= 9) return section == 2;
        if (index == 1 || index == 6 || index == 7) return section == 1;
        return section == 0;
    }

    auto world_info_module::Option(const size_t index) -> module_option
    {
        constexpr static const char* profiles[]{"Time", "Day / date", "Weather"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Movable Vana'diel time and current weather.", &this->state_.enabled).Ordered(-50);
        case 1: {
            auto option = ModuleWidthOption(this->Independent() ? this->parts_[this->SelectedPart()] : this->state_);
            option.description = "Minimum width. Labels grow to fit their text; Single row uses extra width as spacing.";
            return option;
        }
        case 2: return module_option::Toggle("Weekday", "Show the Vana'diel weekday and its native icon.", &this->day_);
        case 3: return module_option::Toggle("Weather", "Show the client's current weather and native element icons.", &this->weather_);
        case 4: return module_option::Toggle("Date", "Show Vana'diel year, month, and day.", &this->date_);
        case 5: return module_option::Element("Selected text", "Text profile to configure.", &this->selected_text_, profiles, 3);
        case 6: {
            static constexpr const char* layouts[]{"Stacked", "Single row", "Independent"};
            return module_option::Choice("Arrangement", "Use compact rows, one horizontal row or separate movable elements.", &this->layout_, layouts, 3).Ordered(90);
        }
        case 7: {
            static constexpr const char* elements[]{"Time", "Weekday", "Weather", "Date"};
            auto option = module_option::Element("Move element", "Choose an element for Width and the movement toolbar, or drag it directly in edit mode.", &this->selected_part_, elements, 4);
            return option.VisibleWhen(this->Independent());
        }
        case 8: return module_option::Toggle("Time", "Show the Vana'diel clock.", &this->time_).Ordered(90);
        default: return TextProfileOption(index - 9, this->text_[this->selected_text_]).VisibleWhen(
            this->selected_text_ == 0 ? this->time_ : this->selected_text_ == 1 ? this->day_ || this->date_ : this->weather_);
        }
    }

    auto world_info_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == 6)
        {
            this->state_.content_bounds = {};
            for (auto& part : this->parts_) part.content_bounds = {};
            this->InitializeParts();
        }
    }

    auto world_info_module::InitializeParts(void) -> void
    {
        if (!this->Independent() || this->state_.bounds.x < 0 || this->state_.bounds.y < 0) return;
        const auto& bounds = this->state_.bounds;
        const std::array<ui::point, 4> offsets{{{108, 0}, {0, 0}, {0, 24}, {0, 48}}};
        for (size_t i = 0; i < this->parts_.size(); ++i)
        {
            auto& part = this->parts_[i].bounds;
            if (part.x < 0 || part.y < 0) { part.x = bounds.x + offsets[i].x; part.y = bounds.y + offsets[i].y; }
        }
    }

    auto world_info_module::PartEnabled(const size_t index) const -> bool
    {
        const std::array flags{this->time_, this->day_, this->weather_, this->date_};
        return this->state_.enabled && flags[index];
    }

    auto world_info_module::Contents(void) const -> std::array<part_content, 4>
    {
        world_state preview{};
        preview.time_valid = preview.weather_valid = true;
        preview.time = {1510, 4, 12, 2, 18, 36}; preview.weather = 6;
        const auto& data = this->data_ != nullptr && (this->data_->time_valid || this->data_->weather_valid) ? *this->data_ : preview;
        std::array<part_content, 4> parts{};
        for (size_t i = 0; i < parts.size(); ++i) parts[i].visible = this->PartEnabled(i);
        parts[0].profile = 0; parts[1].profile = parts[3].profile = 1; parts[2].profile = 2;
        if (data.time_valid)
        {
            std::snprintf(parts[0].text.data(), parts[0].text.size(), "%02u:%02u", data.time.hour, data.time.minute);
            parts[1].element = static_cast<int>(data.time.weekday % 8);
            std::snprintf(parts[1].text.data(), parts[1].text.size(), "%s", vana_days[data.time.weekday % 8]);
            std::snprintf(parts[3].text.data(), parts[3].text.size(), "%04u/%02u/%02u", data.time.year, data.time.month, data.time.day);
        }
        else
        {
            std::snprintf(parts[0].text.data(), parts[0].text.size(), "--:--");
            parts[1].visible = parts[3].visible = false;
        }
        const bool weatherValid = data.weather_valid && data.weather >= 0 && data.weather < static_cast<int>(weather_names.size());
        parts[2].element = weatherValid ? WeatherElement(data.weather) : -1;
        parts[2].doubled = parts[2].element >= 0 && (data.weather & 1) != 0;
        std::snprintf(parts[2].text.data(), parts[2].text.size(), "%s", weatherValid ? weather_names[data.weather] : "Weather unavailable");
        return parts;
    }

    auto world_info_module::PartSize(const part_content& part, const module_render_context& context) const -> ui::point
    {
        const float inset = part.element < 0 ? 0.0f : part.doubled ? 30.0f : 18.0f;
        const auto& style = this->text_[part.profile].style;
        return {inset + context.Design().MeasureText(part.text.data(), style) + 2, std::max(14.0f, context.Design().TextHeight(style) + 2)};
    }

    auto world_info_module::DrawPart(const part_content& part, const ui::rect& bounds, const module_render_context& context) const -> void
    {
        const float inset = part.element < 0 ? 0.0f : part.doubled ? 30.0f : 18.0f;
        if (auto* assets = context.NativeAssets(); assets != nullptr && part.element >= 0 && part.element < 8)
        {
            auto* texture = assets->Texture(static_cast<ui::native_ui_image>(2 + part.element));
            context.Design().DrawTexture(texture, {bounds.x, bounds.y + (bounds.height - 14) * 0.5f, 14, 14});
            if (part.doubled) context.Design().DrawTexture(texture, {bounds.x + 12, bounds.y + (bounds.height - 14) * 0.5f, 14, 14});
        }
        const ui::rect text{bounds.x + inset, bounds.y, std::max(0.0f, bounds.width - inset), bounds.height};
        context.DrawText(part.text.data(), text, text, this->text_[part.profile]);
    }

    auto world_info_module::RenderPart(const size_t index, const module_render_context& context) -> void
    {
        if (!this->Independent() || !this->PartEnabled(index)) return;
        this->InitializeParts();
        const auto part = this->Contents()[index];
        if (!part.visible) return;
        const auto size = this->PartSize(part, context);
        auto& state = this->parts_[index];
        state.bounds.height = size.y;
        auto bounds = state.bounds; bounds.width = std::max(bounds.width, size.x);
        this->DrawPart(part, bounds, context);
    }

    auto world_info_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        if (this->Independent()) return;
        const auto parts = this->Contents();
        std::array<ui::point, 4> sizes{};
        size_t count{}; float width{}, height{};
        for (size_t i = 0; i < parts.size(); ++i)
            if (parts[i].visible)
            {
                sizes[i] = this->PartSize(parts[i], context);
                width += sizes[i].x; height = std::max(height, sizes[i].y); ++count;
            }
        const auto& content = this->state_.bounds;
        if (this->layout_ == 1)
        {
            const float gap = count > 1 ? std::max(8.0f, (content.width - width) / static_cast<float>(count - 1)) : 0;
            float x = content.x;
            for (const size_t i : {1, 0, 2, 3})
                if (parts[i].visible)
                {
                    this->DrawPart(parts[i], {x, content.y, sizes[i].x, height}, context);
                    x += sizes[i].x + gap;
                }
            this->state_.bounds.height = height;
            return;
        }
        const float primaryHeight = std::max(sizes[0].y, sizes[1].y);
        const float primaryWidth = std::max(content.width, sizes[0].x + sizes[1].x + (parts[0].visible && parts[1].visible ? 8.0f : 0));
        if (parts[0].visible)
            this->DrawPart(parts[0], {parts[1].visible ? content.x + primaryWidth - sizes[0].x : content.x, content.y, sizes[0].x, primaryHeight}, context);
        if (parts[1].visible)
            this->DrawPart(parts[1], {content.x, content.y, primaryWidth - sizes[0].x - (parts[0].visible ? 4.0f : 0), primaryHeight}, context);
        float y = content.y + primaryHeight;
        for (const size_t i : {2, 3})
            if (parts[i].visible)
            {
                if (y > content.y) y += 1;
                this->DrawPart(parts[i], {content.x, y, std::max(content.width, sizes[i].x), sizes[i].y}, context);
                y += sizes[i].y;
            }
        this->state_.bounds.height = y - content.y;
    }

    auto world_info_part_module::Id(void) const -> const char* { return part_ids[this->index_]; }
    auto world_info_part_module::DisplayName(void) const -> const char* { return part_names[this->index_]; }
    auto world_info_part_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->PreviewVisible() && this->parent_.data_ != nullptr &&
            (this->index_ == 2 ? this->parent_.data_->weather_valid : this->parent_.data_->time_valid);
    }
}
