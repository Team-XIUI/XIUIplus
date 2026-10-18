#include "world_target_bar.hpp"
#include "modules/bar_config.hpp"

#include <cmath>
#include <cstdio>

namespace xiui
{
    auto world_target_bar::Load(const settings_store& store) -> void
    {
        this->heal_preview.Load(store, "target.world");
        this->enabled = store.ReadBool("target.world", "enabled", false);
        this->name = store.ReadBool("target.world", "name", false);
        this->percent = store.ReadBool("target.world", "percent", false);
        this->follow_fill = store.ReadBool("target.world", "follow_fill", true);
        const auto number = [&](const char* key, const float fallback, const float low, const float high) {
            const auto value = store.ReadFloat("target.world", key, fallback);
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        this->width = number("width", 120, 32, 800);
        this->offset_x = number("offset_x", 0, -400, 400);
        this->offset_y = number("offset_y", 10, -400, 400);
        this->fill_top = store.ReadArgb("target.world", "fill_top", this->fill_top);
        this->fill_bottom = store.ReadArgb("target.world", "fill_bottom", this->fill_bottom);
        this->meter = LoadMeterStyle(store, "meter.target.world", this->meter);
        this->text[0] = LoadTextProfile(store, "text.target.world.name", this->text[0]);
        this->text[1] = LoadTextProfile(store, "text.target.world.percent", this->text[1]);
        this->target_id = 0; this->animation = {};
    }
    auto world_target_bar::Save(const settings_store& store) const -> void
    {
        this->heal_preview.Save(store, "target.world");
        store.WriteBool("target.world", "enabled", this->enabled);
        store.WriteBool("target.world", "name", this->name);
        store.WriteBool("target.world", "percent", this->percent);
        store.WriteBool("target.world", "follow_fill", this->follow_fill);
        store.WriteFloat("target.world", "width", this->width);
        store.WriteFloat("target.world", "offset_x", this->offset_x);
        store.WriteFloat("target.world", "offset_y", this->offset_y);
        store.WriteArgb("target.world", "fill_top", this->fill_top);
        store.WriteArgb("target.world", "fill_bottom", this->fill_bottom);
        SaveMeterStyle(store, "meter.target.world", this->meter);
        SaveTextProfile(store, "text.target.world.name", this->text[0]);
        SaveTextProfile(store, "text.target.world.percent", this->text[1]);
    }
    auto world_target_bar::Section(const size_t index) -> size_t
    {
        if (index >= heal_options_start) return heal_preview_settings::ColorOption(index - heal_options_start) ? 5u : 0u;
        if (index >= base_options + meter_style_option_count || index == 9) return 6;
        if (index >= base_options) return static_cast<size_t>(MeterConfigSection(index - base_options));
        return index <= 2 ? 0 : index <= 5 ? 1 : 5;
    }
    auto world_target_bar::Option(const size_t index) -> module_option
    {
        if (index >= heal_options_start) return this->heal_preview.Option(index - heal_options_start, this->healing);
        if (index >= base_options + meter_style_option_count) return TextProfileOption(index - base_options - meter_style_option_count, this->text[this->selected_text]);
        if (index >= base_options) return MeterStyleOption(index - base_options, this->meter);
        static constexpr const char* texts[]{"Name", "HP percent"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show a small HP bar beneath the selected target in the world.", &this->enabled).Ordered(-50);
        case 1: return module_option::Toggle("Name", "Show the target name above the world bar.", &this->name);
        case 2: return module_option::Toggle("HP percent", "Show the target's native HP percent inside the bar.", &this->percent);
        case 3: return module_option::Slider("Bar width", "World bar width in screen pixels.", &this->width, 32, 800, 1).Ordered(100, true);
        case 4: return module_option::Slider("X offset", "Horizontal offset from the target's ground position.", &this->offset_x, -400, 400, 1).Ordered(210, true);
        case 5: return module_option::Slider("Y offset", "Positive moves the bar below the target's ground position.", &this->offset_y, -400, 400, 1).Ordered(211);
        case 6: return module_option::Toggle("Follow Target colors", "Use Target's HP gradient, including its entity color preferences.", &this->follow_fill).Ordered(0);
        case 7:
        case 8:
        {
            auto option = index == 7 ? module_option::Color("Fill top", "Upper world-bar fill gradient.", &this->fill_top).Ordered(500, true) :
                module_option::Color("Fill bottom", "Lower world-bar fill gradient.", &this->fill_bottom).Ordered(501);
            return option.VisibleWhen(!this->follow_fill);
        }
        default: return module_option::Element("Selected text", "Choose the world name or HP percent text style.", &this->selected_text, texts, 2);
        }
    }
    auto world_target_bar::Bounds(const hud_snapshot& snapshot) const -> ui::rect
    { return {snapshot.target_screen_x + this->offset_x - this->width * 0.5f, snapshot.target_screen_y + this->offset_y, this->width, this->meter.height + 2}; }
    auto world_target_bar::Draw(const hud_snapshot& snapshot, const uint32_t id, const D3DCOLOR nameColor, const D3DCOLOR top, const D3DCOLOR bottom, const module_render_context& context) -> void
    {
        if (!this->enabled || !snapshot.target_valid || !snapshot.target_screen_valid || id == 0)
        { this->target_id = 0; this->animation = {}; return; }
        if (id != this->target_id) { this->animation = {}; this->target_id = id; }
        const auto bounds = this->Bounds(snapshot), viewport = context.Viewport();
        if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || bounds.x < viewport.x || bounds.y < viewport.y ||
            bounds.x + bounds.width > viewport.x + viewport.width || bounds.y + bounds.height > viewport.y + viewport.height) return;
        char value[16]{};
        if (this->percent) std::snprintf(value, sizeof(value), "%u%%", snapshot.target_hp_percent);
        const auto ranges = this->heal_preview.Ranges(this->healing, id, ::GetTickCount64());
        context.DrawMeter(bounds, "", value, std::min(snapshot.target_hp_percent, 100u) / 100.0f,
            this->follow_fill ? top : this->fill_top, this->follow_fill ? bottom : this->fill_bottom,
            this->meter, this->animation, false, nullptr, &this->text[1],
            ui::meter_text_anchor::automatic, ui::meter_text_anchor::automatic, meter_fill_mode::health, {}, ranges);
        if (this->name)
        {
            auto text = TintTextProfile(this->text[0], nameColor); text.style.alignment = ui::text_alignment::center;
            const float height = context.Design().TextHeight(text.style);
            const ui::rect line{bounds.x, bounds.y - height - 2, bounds.width, height};
            context.DrawText(snapshot.target_name, line, line, text);
        }
    }
}
