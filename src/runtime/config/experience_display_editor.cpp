#include "localization/localization.hpp"
#include "experience_display_editor.hpp"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr const char* views[]{"Bar info", "Readouts"};
    constexpr const char* filters[]{"All", "Session", "Combat", "Progress", "Bonuses"};
    auto Inside(const xiui::ui::rect& bounds, const float x, const float y) -> bool
    { return x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }
}

namespace xiui::ui
{
    experience_display_editor::geometry::geometry(const config_layout& layout)
    {
        const auto area = layout.ContentArea();
        this->stacked = area.width < 560;
        const float width = this->stacked ? area.width : (area.width - 14) * 0.5f;
        this->rows = std::max(1, static_cast<int>((area.height - (this->stacked ? 190 : 168)) / (this->stacked ? 56 : 28)));
        this->shown_title = {area.x, area.y, width, 22};
        this->actions = {area.x, area.y + (this->stacked ? 24 : 42), width, 26};
        this->shown = {area.x, area.y + (this->stacked ? 54 : 76), width, this->rows * 28.0f};
        this->available_title = this->stacked ? rect{area.x, this->shown.y + this->shown.height + 8, width, 22} :
            rect{area.x + width + 14, area.y, width, 22};
        this->filters = {this->available_title.x, this->available_title.y + (this->stacked ? 24 : 42), width, 26};
        this->available = {this->available_title.x, this->available_title.y + (this->stacked ? 54 : 76), width, this->shown.height};
        this->details = {area.x, this->available.y + this->available.height + 8, area.width, this->stacked ? 36.0f : 54.0f};
        this->presets = {area.x, this->details.y + this->details.height + 4, area.width, 26};
    }

    auto experience_display_editor::geometry::Row(const bool enabled, const int index) const -> rect
    {
        const auto& list = enabled ? this->shown : this->available;
        return {list.x, list.y + index * 28, list.width, 26};
    }

    auto experience_display_editor::Items(const experience_module& module, const bool shown) const -> list
    {
        list result{};
        const auto& settings = module.Settings();
        for (size_t index = 0; index < experience_readout_count; ++index)
            if (settings.readouts[index] == shown && (shown || this->filter_ == 0 ||
                static_cast<size_t>(experience_readouts[index].group) + 1 == this->filter_)) result.items[result.count++] = index;
        if (shown) std::sort(result.items.begin(), result.items.begin() + result.count,
            [&](const auto a, const auto b) { return settings.order[a] < settings.order[b]; });
        return result;
    }

    auto experience_display_editor::First(const int scroll, const size_t count, const int rows) -> int
    { return std::clamp(scroll, 0, std::max(0, static_cast<int>(count) - rows)); }

    auto experience_display_editor::Selected(const list& shown) const -> size_t
    {
        if (std::find(shown.items.begin(), shown.items.begin() + shown.count, this->selected_) != shown.items.begin() + shown.count) return this->selected_;
        return shown.count != 0 ? shown.items[0] : experience_readout_count;
    }

    auto experience_display_editor::Reveal(const experience_module& module, const config_layout& layout) -> void
    {
        const auto shown = this->Items(module, true);
        this->selected_ = this->Selected(shown);
        const geometry bounds{layout};
        this->shown_scroll_ = First(this->shown_scroll_, shown.count, bounds.rows);
        for (size_t index = 0; index < shown.count; ++index)
            if (shown.items[index] == this->selected_)
            {
                if (static_cast<int>(index) < this->shown_scroll_) this->shown_scroll_ = static_cast<int>(index);
                if (static_cast<int>(index) >= this->shown_scroll_ + bounds.rows) this->shown_scroll_ = static_cast<int>(index) - bounds.rows + 1;
            }
    }

    auto experience_display_editor::Target(const experience_module& module, const config_layout& layout, const float x, const float y) const -> uint32_t
    {
        for (size_t index = 0; index < std::size(views); ++index)
            if (Inside(config_controls::Segment(layout.ContentTabs(), index, std::size(views)), x, y)) return static_cast<uint32_t>(index + 1);
        if (this->view_ != experience_display_view::readouts) return 0;
        const geometry bounds{layout};
        const auto shown = this->Items(module, true), available = this->Items(module, false);
        const auto selected = this->Selected(shown);
        for (size_t index = 0; index < std::size(filters); ++index)
            if (Inside(config_controls::Segment(bounds.filters, index, std::size(filters)), x, y)) return static_cast<uint32_t>(10 + index);
        for (size_t index = 0; index < 3; ++index)
        {
            if (!Inside(config_controls::Segment(bounds.actions, index, 3), x, y)) continue;
            if (selected == experience_readout_count || (index == 0 && selected == shown.items[0]) ||
                (index == 1 && selected == shown.items[shown.count - 1])) return 0;
            return static_cast<uint32_t>(20 + index);
        }
        const rect recommended{bounds.presets.x, bounds.presets.y, 132, 26};
        const rect clear{recommended.x + recommended.width + 6, recommended.y, 72, 26};
        if (Inside(recommended, x, y)) return 30;
        if (Inside(clear, x, y) && shown.count != 0) return 31;
        for (int row = 0; row < bounds.rows; ++row)
        {
            const auto left = static_cast<size_t>(First(this->shown_scroll_, shown.count, bounds.rows) + row);
            const auto right = static_cast<size_t>(First(this->available_scroll_, available.count, bounds.rows) + row);
            if (left < shown.count && Inside(bounds.Row(true, row), x, y)) return static_cast<uint32_t>(0x100 + shown.items[left]);
            if (right < available.count && Inside(bounds.Row(false, row), x, y)) return static_cast<uint32_t>(0x200 + available.items[right]);
        }
        return 0;
    }

    auto experience_display_editor::Click(experience_module& module, const config_layout& layout, const float x, const float y) -> result
    {
        return this->Activate(this->Target(module, layout, x, y), module, layout);
    }

    auto experience_display_editor::Activate(const uint32_t target, experience_module& module, const config_layout& layout) -> result
    {
        if (target == 0) return result::none;
        if (target <= std::size(views)) { this->view_ = static_cast<experience_display_view>(target - 1); return result::navigation; }
        if (target >= 10 && target < 15) { this->filter_ = target - 10; this->available_scroll_ = 0; return result::navigation; }
        if (target >= 0x100 && target < 0x200) { this->selected_ = target - 0x100; return result::navigation; }
        if (target >= 0x200)
        {
            this->selected_ = target - 0x200;
            module.SetReadoutVisible(this->selected_, true);
        }
        else if (target >= 20 && target <= 22)
        {
            const auto shown = this->Items(module, true);
            this->selected_ = this->Selected(shown);
            if (target == 22)
            {
                const auto removed = this->selected_;
                for (size_t index = 0; index < shown.count; ++index)
                    if (shown.items[index] == removed && shown.count > 1)
                    { this->selected_ = shown.items[index + 1 < shown.count ? index + 1 : index - 1]; break; }
                module.SetReadoutVisible(removed, false);
            }
            else module.MoveReadout(this->selected_, target == 20 ? -1 : 1);
        }
        else if (target == 30 || target == 31)
        {
            module.Settings().readouts = target == 30 ? DefaultExperienceReadouts() : std::array<bool, experience_readout_count>{};
            if (target == 30) module.Settings().order = DefaultExperienceReadoutOrder();
            module.OnOptionChanged(0);
            this->selected_ = experience_readout_count;
            this->shown_scroll_ = this->available_scroll_ = 0;
        }
        this->Reveal(module, layout);
        return result::changed;
    }

    auto experience_display_editor::ControllerItems(const experience_module& module, const config_layout& layout) const -> std::vector<config_controller_item>
    {
        std::vector<config_controller_item> result;
        if (this->view_ != experience_display_view::readouts) return result;
        const geometry bounds{layout};
        const auto shown = this->Items(module, true), available = this->Items(module, false);
        const auto selected = this->Selected(shown);
        const auto add = [&](const char* id, const rect& at, const size_t target, const char* help) {
            result.push_back({id, at, target, help});
        };
        for (size_t index = 0; index < shown.count; ++index)
        {
            const auto readout = shown.items[index];
            add(experience_readouts[readout].key, bounds.Row(true, static_cast<int>(index) - First(this->shown_scroll_, shown.count, bounds.rows)), 0x100 + readout, experience_readouts[readout].description);
        }
        for (size_t index = 0; index < available.count; ++index)
        {
            const auto readout = available.items[index];
            add(experience_readouts[readout].key, bounds.Row(false, static_cast<int>(index) - First(this->available_scroll_, available.count, bounds.rows)), 0x200 + readout, experience_readouts[readout].description);
        }
        static constexpr const char* actions[]{"earlier", "later", "remove"};
        static constexpr const char* help[]{"Move the selected shown readout earlier.", "Move the selected shown readout later.", "Remove the selected readout from the display."};
        if (selected < experience_readout_count)
            for (size_t index = 0; index < 3; ++index)
            {
                if ((index == 0 && selected == shown.items[0]) || (index == 1 && selected == shown.items[shown.count - 1])) continue;
                add(actions[index], config_controls::Segment(bounds.actions, index, 3), 20 + index, help[index]);
            }
        for (size_t index = 0; index < std::size(filters); ++index)
            add(filters[index], config_controls::Segment(bounds.filters, index, std::size(filters)), 10 + index, "Filter the available readouts.");
        const rect recommended{bounds.presets.x, bounds.presets.y, 132, 26};
        add("recommended", recommended, 30, "Use the recommended readouts.");
        if (shown.count != 0) add("clear", {recommended.x + recommended.width + 6, recommended.y, 72, 26}, 31, "Remove all shown readouts.");
        return result;
    }

    auto experience_display_editor::RevealControllerTarget(const uint32_t target, const experience_module& module, const config_layout& layout) -> void
    {
        if (target < 0x100) return;
        const bool shown = target < 0x200;
        const auto items = this->Items(module, shown);
        const auto readout = target - (shown ? 0x100 : 0x200);
        auto& scroll = shown ? this->shown_scroll_ : this->available_scroll_;
        const auto rows = geometry{layout}.rows;
        for (size_t index = 0; index < items.count; ++index)
            if (items.items[index] == readout)
            {
                if (static_cast<int>(index) < scroll) scroll = static_cast<int>(index);
                if (static_cast<int>(index) >= scroll + rows) scroll = static_cast<int>(index) - rows + 1;
                break;
            }
        scroll = First(scroll, items.count, rows);
    }

    auto experience_display_editor::Scroll(const experience_module& module, const config_layout& layout, const float x, const float y, const int steps) -> bool
    {
        if (this->view_ != experience_display_view::readouts) return false;
        const geometry bounds{layout};
        if (Inside(bounds.shown, x, y)) this->shown_scroll_ = First(this->shown_scroll_ - steps * 3, this->Items(module, true).count, bounds.rows);
        else if (Inside(bounds.available, x, y)) this->available_scroll_ = First(this->available_scroll_ - steps * 3, this->Items(module, false).count, bounds.rows);
        else return false;
        return true;
    }

    auto experience_display_editor::Draw(const experience_module& module, const config_controls& controls, const design_system& design, const config_layout& layout, const float x, const float y) const -> void
    {
        controls.Segmented(layout.ContentTabs(), views, static_cast<size_t>(this->view_), x, y);
        if (this->view_ != experience_display_view::readouts) return;
        const geometry bounds{layout};
        const auto shown = this->Items(module, true), available = this->Items(module, false);
        const auto selected = this->Selected(shown);
        const auto target = this->Target(module, layout, x, y);
        auto title = design.GetTheme().body_text; title.scale = 0.78f;
        auto caption = design.GetTheme().caption_text; caption.scale = 0.65f;
        caption.alignment = title.alignment = text_alignment::left;
        char text[96]{};
        std::snprintf(text, sizeof(text), "Shown on HUD (%zu)", shown.count);
        controls.Text(text, bounds.shown_title, title);
        std::snprintf(text, sizeof(text), "Available (%zu)", available.count);
        controls.Text(text, bounds.available_title, title);
        if (!bounds.stacked)
        {
            controls.Text(i18n::Text("Select a row to move or remove it."), {bounds.shown_title.x, bounds.shown_title.y + 22, bounds.shown_title.width, 18}, caption);
            controls.Text(i18n::Text("Click a row to add it to the HUD."), {bounds.available_title.x, bounds.available_title.y + 22, bounds.available_title.width, 18}, caption);
        }
        constexpr const char* actions[]{"Move up", "Move down", "Remove"};
        for (size_t index = 0; index < std::size(actions); ++index)
        {
            const bool enabled = selected != experience_readout_count && (index != 0 || selected != shown.items[0]) && (index != 1 || selected != shown.items[shown.count - 1]);
            auto style = enabled ? title : caption; style.scale = 0.66f; style.alignment = text_alignment::center;
            controls.Button(config_controls::Segment(bounds.actions, index, 3), actions[index], style, enabled && target == 20 + index);
        }
        controls.Segmented(bounds.filters, filters, this->filter_, x, y);
        design.FillRoundedRect(bounds.shown, 0x48000000, 3);
        design.FillRoundedRect(bounds.available, 0x48000000, 3);
        for (int row = 0; row < bounds.rows; ++row)
        {
            const auto left = static_cast<size_t>(First(this->shown_scroll_, shown.count, bounds.rows) + row);
            const auto right = static_cast<size_t>(First(this->available_scroll_, available.count, bounds.rows) + row);
            if (left < shown.count)
            {
                const auto id = shown.items[left];
                std::snprintf(text, sizeof(text), "%zu", left + 1);
                controls.ListItem(bounds.Row(true, row), experience_readouts[id].name, module.ReadoutRepeatsBar(id) ? "Bar" : text, selected == id, target == 0x100 + id);
            }
            if (right < available.count)
            {
                const auto id = available.items[right];
                controls.ListItem(bounds.Row(false, row), experience_readouts[id].name, module.ReadoutRepeatsBar(id) ? "Bar +" : "+", false, target == 0x200 + id);
            }
        }
        if (shown.count == 0) controls.Text(i18n::Text("No readouts enabled"), bounds.Row(true, 0), caption);
        if (available.count == 0) controls.Text(i18n::Text("All items in this group are shown"), bounds.Row(false, 0), caption);
        size_t described = selected;
        if (target >= 0x100) described = target >= 0x200 ? target - 0x200 : target - 0x100;
        if (described < experience_readout_count)
        {
            controls.Text(experience_readouts[described].name, {bounds.details.x, bounds.details.y, bounds.details.width, 18}, title);
            controls.Text(experience_readouts[described].description, {bounds.details.x, bounds.details.y + 18, bounds.details.width, 18}, caption);
        }
        const bool movesBarInfo = described == static_cast<size_t>(experience_readout::current_required) ||
            described == static_cast<size_t>(experience_readout::remaining) || described == static_cast<size_t>(experience_readout::percent);
        if (!bounds.stacked) controls.Text(described < experience_readout_count && module.ReadoutRepeatsBar(described) ? movesBarInfo ?
            i18n::Text("Bar = currently in bar info. Adding this readout moves it into the list.") :
            i18n::Text("Bar = already shown by your current bar-info settings. Adding it is optional.") :
            i18n::Text("Rows follow HUD order. Unknown values show --; hidden items leave no gaps."),
            {bounds.details.x, bounds.details.y + 36, bounds.details.width, 18}, caption);
        auto button = title; button.scale = 0.68f; button.alignment = text_alignment::center;
        controls.Button({bounds.presets.x, bounds.presets.y, 132, 26}, i18n::Text("Recommended"), button, target == 30);
        controls.Button({bounds.presets.x + 138, bounds.presets.y, 72, 26}, i18n::Text("Clear"), shown.count != 0 ? button : caption, target == 31);
        std::snprintf(text, sizeof(text), bounds.stacked ? "%zu / %zu" : "%zu shown / %zu available - wheel to scroll", shown.count, available.count);
        caption.alignment = text_alignment::right;
        controls.Text(text, {bounds.presets.x + 220, bounds.presets.y, std::max(1.0f, bounds.presets.width - 220), 26}, caption);
    }
}
