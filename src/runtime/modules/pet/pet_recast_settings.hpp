#pragma once

#include "pet_source.hpp"
#include "modules/recast/recast_tracker.hpp"
#include "modules/module.hpp"

namespace xiui
{
    struct pet_recast_definition { uint16_t timer; const char* key; const char* name; pet_kind kind; };
    inline constexpr pet_recast_definition pet_recast_definitions[]{
        {173, "rage", "Blood Pact: Rage", pet_kind::avatar}, {174, "ward", "Blood Pact: Ward", pet_kind::avatar},
        {108, "apogee", "Apogee", pet_kind::avatar}, {71, "mana_cede", "Mana Cede", pet_kind::avatar},
        {102, "ready", "Ready", pet_kind::jug}, {102, "sic", "Sic", pet_kind::charm},
        {103, "reward", "Reward", pet_kind::jug}, {103, "reward_charm", "Reward", pet_kind::charm},
        {104, "call_beast", "Call Beast", pet_kind::jug}, {104, "bestial_loyalty", "Bestial Loyalty", pet_kind::jug},
        {163, "call_wyvern", "Call Wyvern", pet_kind::wyvern}, {162, "spirit_link", "Spirit Link", pet_kind::wyvern},
        {164, "deep_breathing", "Deep Breathing", pet_kind::wyvern}, {70, "steady_wing", "Steady Wing", pet_kind::wyvern},
        {205, "activate", "Activate", pet_kind::automaton}, {206, "repair", "Repair", pet_kind::automaton},
        {115, "deus_ex_automata", "Deus Ex Automata", pet_kind::automaton}, {207, "deploy", "Deploy", pet_kind::automaton},
        {208, "deactivate", "Deactivate", pet_kind::automaton}, {209, "retrieve", "Retrieve", pet_kind::automaton},
        {210, "maneuvers", "Maneuvers", pet_kind::automaton}, {0, "two_hour", "Two-hour ability", pet_kind::count}};

    struct pet_recast_style
    {
        bool enabled{true};
        D3DCOLOR ready_top{0xFF416348}, ready_bottom{0xFF94D89D}, cooldown_top{0xFF555865}, cooldown_bottom{0xFF9196A6};
    };

    struct pet_recast_settings
    {
        static constexpr size_t option_count = 19;
        int display{}, selected{}, alignment{}, fill{};
        bool names{true}, timers{true}, absolute{};
        float width{}, height{18}, size{32}, gap{4}, top_gap{}, offset_x{}, offset_y{};
        std::array<pet_recast_style, std::size(pet_recast_definitions)> abilities{};

        static auto Indices(const pet_kind kind) -> std::vector<size_t>
        {
            std::vector<size_t> result;
            for (size_t i = 0; i < std::size(pet_recast_definitions); ++i)
                if (pet_recast_definitions[i].kind == kind || pet_recast_definitions[i].kind == pet_kind::count) result.push_back(i);
            return result;
        }
        auto Style(const recast_entry& entry, const pet_kind kind) const -> const pet_recast_style&
        {
            for (const auto index : Indices(kind))
                if (pet_recast_definitions[index].timer == entry.id && (entry.name == pet_recast_definitions[index].name || entry.id != 104)) return this->abilities[index];
            return this->abilities[8];
        }
        auto Load(const settings_store& store, const std::string& section) -> void
        {
            const auto number = [&](const char* key, float fallback, float low, float high) { return std::clamp(store.ReadFloat(section.c_str(), key, fallback), low, high); };
            this->display = std::clamp(store.ReadInt(section.c_str(), "display", 0), 0, 3);
            this->alignment = std::clamp(store.ReadInt(section.c_str(), "alignment", 0), 0, 2); this->fill = std::clamp(store.ReadInt(section.c_str(), "fill", 0), 0, 2);
            this->names = store.ReadBool(section.c_str(), "names", true); this->timers = store.ReadBool(section.c_str(), "timers", true); this->absolute = store.ReadBool(section.c_str(), "absolute", false);
            this->width = number("width", 0, 0, 1200); this->height = number("height", 18, 4, 64); this->size = number("icon_size", 32, 12, 96);
            this->gap = number("spacing", 4, 0, 32); this->top_gap = number("top_spacing", 0, 0, 64);
            this->offset_x = number("offset_x", 0, -1200, 1200); this->offset_y = number("offset_y", 0, -1200, 1200);
            for (size_t i = 0; i < this->abilities.size(); ++i)
            {
                auto& style = this->abilities[i]; const auto key = section + "." + pet_recast_definitions[i].key;
                style.enabled = store.ReadBool(key.c_str(), "enabled", true);
                style.ready_top = store.ReadArgb(key.c_str(), "ready_top", style.ready_top); style.ready_bottom = store.ReadArgb(key.c_str(), "ready_bottom", style.ready_bottom);
                style.cooldown_top = store.ReadArgb(key.c_str(), "cooldown_top", style.cooldown_top); style.cooldown_bottom = store.ReadArgb(key.c_str(), "cooldown_bottom", style.cooldown_bottom);
            }
        }
        auto Save(const settings_store& store, const std::string& section) const -> void
        {
            store.WriteInt(section.c_str(), "display", this->display); store.WriteInt(section.c_str(), "alignment", this->alignment); store.WriteInt(section.c_str(), "fill", this->fill);
            store.WriteBool(section.c_str(), "names", this->names); store.WriteBool(section.c_str(), "timers", this->timers); store.WriteBool(section.c_str(), "absolute", this->absolute);
            for (const auto& [key, value] : {std::pair{"width", this->width}, {"height", this->height}, {"icon_size", this->size}, {"spacing", this->gap}, {"top_spacing", this->top_gap}, {"offset_x", this->offset_x}, {"offset_y", this->offset_y}}) store.WriteFloat(section.c_str(), key, value);
            for (size_t i = 0; i < this->abilities.size(); ++i)
            {
                const auto& style = this->abilities[i]; const auto key = section + "." + pet_recast_definitions[i].key;
                store.WriteBool(key.c_str(), "enabled", style.enabled);
                store.WriteArgb(key.c_str(), "ready_top", style.ready_top); store.WriteArgb(key.c_str(), "ready_bottom", style.ready_bottom);
                store.WriteArgb(key.c_str(), "cooldown_top", style.cooldown_top); store.WriteArgb(key.c_str(), "cooldown_bottom", style.cooldown_bottom);
            }
        }
        auto Option(const size_t index, const pet_kind kind) -> module_option
        {
            static const auto choices = [] { std::array<std::vector<const char*>, pet_kind_count> result{}; for (size_t k = 0; k < pet_kind_count; ++k) for (const auto i : Indices(static_cast<pet_kind>(k))) result[k].push_back(pet_recast_definitions[i].name); return result; }();
            const auto indices = Indices(kind); this->selected = std::clamp(this->selected, 0, static_cast<int>(indices.size() - 1));
            auto& style = this->abilities[indices[this->selected]];
            static constexpr const char* displays[]{"Text rows", "Full bars", "Compact icons", "Icons only"};
            static constexpr const char* alignments[]{"Left", "Center", "Right"};
            static constexpr const char* fills[]{"Square", "Circle", "Clock"};
            switch (index)
            {
            case 0: return module_option::Choice("Recast presentation", "Choose a separate presentation for this pet type.", &this->display, displays, 4);
            case 1: return module_option::Element("Ability", "Select an ability's visibility and ready/cooldown colors.", &this->selected, choices[static_cast<size_t>(kind)].data(), indices.size()).VerbatimChoices();
            case 2: return module_option::Toggle("Show ability", "Keep this ability in the pet recast display.", &style.enabled);
            case 3: return module_option::Color("Ready top", "Upper ready gradient.", &style.ready_top).Ordered(500, true);
            case 4: return module_option::Color("Ready bottom", "Lower ready gradient.", &style.ready_bottom).Ordered(501);
            case 5: return module_option::Color("Cooldown top", "Upper cooldown gradient.", &style.cooldown_top).Ordered(502, true);
            case 6: return module_option::Color("Cooldown bottom", "Lower cooldown gradient.", &style.cooldown_bottom).Ordered(503);
            case 7: return module_option::Slider("Recast width", "Zero follows the pet content width.", &this->width, 0, 1200, 1);
            case 8: return module_option::Slider("Recast height", "Height of full recast bars.", &this->height, 4, 64, 1).VisibleWhen(this->display == 1);
            case 9: return module_option::Slider("Recast icon size", "Size of compact recast icons.", &this->size, 12, 96, 1).VisibleWhen(this->display >= 2);
            case 10: return module_option::Toggle("Ability names", "Show names in text rows and full bars.", &this->names).VisibleWhen(this->display < 2);
            case 11: return module_option::Toggle("Recast timers", "Show countdown or Ready text.", &this->timers).VisibleWhen(this->display != 3);
            case 12: return module_option::Choice("Recast alignment", "Align the recast section inside the content area.", &this->alignment, alignments, 3);
            case 13: return module_option::Slider("Ability spacing", "Space between recast rows or icons.", &this->gap, 0, 32, 1);
            case 14: return module_option::Slider("Recast top spacing", "Space after pet vitals and effects.", &this->top_gap, 0, 64, 1);
            case 15: return module_option::Toggle("Absolute recast position", "Position compact icons from the pet window instead of after the vitals.", &this->absolute).VisibleWhen(this->display >= 2);
            case 16: return module_option::Slider("Recast X offset", "Horizontal adjustment of the recast section.", &this->offset_x, -1200, 1200, 1).Ordered(200, true);
            case 17: return module_option::Slider("Recast Y offset", "Vertical adjustment of the recast section.", &this->offset_y, -1200, 1200, 1).Ordered(201);
            default: return module_option::Choice("Icon fill", "Cooldown overlay shape for compact icons.", &this->fill, fills, 3).VisibleWhen(this->display >= 2);
            }
        }
    };
}
