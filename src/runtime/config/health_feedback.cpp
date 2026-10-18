#include "health_feedback.hpp"

namespace xiui
{
    auto health_feedback_settings::Load(const settings_store& store) -> void
    {
        this->enabled = store.ReadBool("health.feedback", "enabled", true);
        this->damage = store.ReadBool("health.feedback", "damage", true);
        this->healing = store.ReadBool("health.feedback", "healing", true);
        this->flashes = store.ReadBool("health.feedback", "flashes", true);
        this->warning_colors = store.ReadBool("health.feedback", "warning_colors", true);
        const auto read = [&](const char* key, float fallback, float minimum, float maximum) {
            const float value = store.ReadFloat("health.feedback", key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->delay = read("delay", .2f, 0, 2);
        this->decay = read("decay", .6f, .05f, 3);
        this->flash_duration = read("flash_duration", .22f, .05f, 1);
        this->flash_strength = read("flash_strength", .25f, 0, 1);
        this->damage_color = store.ReadArgb("health.feedback", "damage_color", 0xDFF7FBFF);
        if (store.ReadInt("health.feedback", "style_version", 0) < 1 && this->damage_color == 0xCFFF7668)
            this->damage_color = 0xDFF7FBFF;
        this->heal_color = store.ReadArgb("health.feedback", "heal_color", 0xBF79E5AC);
    }

    auto health_feedback_settings::Save(const settings_store& store) const -> void
    {
        store.WriteBool("health.feedback", "enabled", this->enabled);
        store.WriteBool("health.feedback", "damage", this->damage);
        store.WriteBool("health.feedback", "healing", this->healing);
        store.WriteBool("health.feedback", "flashes", this->flashes);
        store.WriteBool("health.feedback", "warning_colors", this->warning_colors);
        store.WriteInt("health.feedback", "style_version", 1);
        store.WriteFloat("health.feedback", "delay", this->delay);
        store.WriteFloat("health.feedback", "decay", this->decay);
        store.WriteFloat("health.feedback", "flash_duration", this->flash_duration);
        store.WriteFloat("health.feedback", "flash_strength", this->flash_strength);
        store.WriteArgb("health.feedback", "damage_color", this->damage_color);
        store.WriteArgb("health.feedback", "heal_color", this->heal_color);
    }

    auto health_feedback_settings::Option(size_t index) -> module_option
    {
        switch (index)
        {
        case 0: return module_option::Toggle("HP feedback", "Damage, healing and warning effects across XIUI health bars. Saved fill and text styles are retained.", &this->enabled).Ordered(-50);
        case 1: return module_option::Toggle("Damage trail", "Keep the lost portion briefly before it recedes.", &this->damage).VisibleWhen(this->enabled);
        case 2: return module_option::Toggle("Healing segment", "Highlight restored health while the bar catches up.", &this->healing).VisibleWhen(this->enabled);
        case 3: return module_option::Toggle("Health flashes", "Briefly tint the fill after an observed HP loss or gain.", &this->flashes).VisibleWhen(this->enabled);
        case 4: return module_option::Slider("Trail delay", "Seconds to hold the changed portion.", &this->delay, 0, 2, .05f).Ordered(200, true).VisibleWhen(this->enabled && (this->damage || this->healing));
        case 5: return module_option::Slider("Trail decay", "Seconds for the segment to settle.", &this->decay, .05f, 3, .05f).Ordered(201).VisibleWhen(this->enabled && (this->damage || this->healing));
        case 6: return module_option::Slider("Flash duration", "Seconds before the tint fades.", &this->flash_duration, .05f, 1, .01f).Ordered(210, true).VisibleWhen(this->enabled && this->flashes);
        case 7: return module_option::Slider("Flash strength", "Opacity of the temporary tint.", &this->flash_strength, 0, 1, .05f).Ordered(211).VisibleWhen(this->enabled && this->flashes);
        case 8: return module_option::Color("Damage color", "Lost HP segment glow. The remaining fill flashes with an HP warning tint.", &this->damage_color).Ordered(500, true).VisibleWhen(this->enabled && (this->damage || this->flashes));
        case 9: return module_option::Color("Healing color", "Restored HP segment and healing flash.", &this->heal_color).Ordered(501).VisibleWhen(this->enabled && (this->healing || this->flashes));
        case 10: return module_option::Toggle("HP warning colors", "Shift HP fills to yellow, orange and red below 75%, 50% and 25%. Player's custom HP state colors take priority.", &this->warning_colors).Ordered(100).VisibleWhen(this->enabled);
        default: return {};
        }
    }
}
