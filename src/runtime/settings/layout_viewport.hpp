#pragma once

#include "settings_store.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace xiui
{
    struct layout_viewport
    {
        int width{}, height{};
        auto Valid(void) const -> bool { return this->width >= 320 && this->height >= 200 && this->width <= 32768 && this->height <= 32768; }
        auto operator==(const layout_viewport&) const -> bool = default;
    };

    inline auto ReadLayoutViewport(const settings_store& store) -> layout_viewport
    { return {store.ReadInt("layout.viewport", "width", 0), store.ReadInt("layout.viewport", "height", 0)}; }

    inline auto WriteLayoutViewport(const settings_store& store, const layout_viewport viewport) -> void
    {
        if (!viewport.Valid()) return;
        store.WriteInt("layout.viewport", "width", viewport.width);
        store.WriteInt("layout.viewport", "height", viewport.height);
    }

    inline auto LayoutSections(const settings_store& store) -> std::set<std::string>
    {
        std::set<std::string> result;
        store.VisitPreferences([&](const std::string_view section, const std::string_view key, std::string_view) {
            if (section.size() > 7 && ::_strnicmp(section.data(), "layout.", 7) == 0 &&
                key.size() == 1 && (key[0] == 'x' || key[0] == 'y' || key[0] == 'X' || key[0] == 'Y')) result.emplace(section);
        });
        return result;
    }

    inline auto FitLayoutAxis(const float position, const float size, const int reference, const int available) -> float
    {
        if (position < 0 || reference == available) return position;
        // Compact modules can render less than their saved layout height.
        if (position < reference && position + size > reference) return std::max(0.0f, position + available - reference);
        const auto travel = std::max(0.0f, available - size);
        if (size >= reference) return travel * .5f;
        return std::clamp(position / (reference - size), 0.0f, 1.0f) * travel;
    }

    inline auto FitDefaultLayoutAxis(const std::string_view section, const bool horizontal, const float position,
        const float size, const int reference, const int available) -> float
    {
        if (position < 0 || reference == available) return position;
        const auto id = section.substr(7);
        const bool chat = id == "chat" || id == "chat2" || id == "commands";
        const bool central = id == "player" || id == "exp" || id == "inventory" || id == "equipment" || id == "gil" ||
            id == "castbar" || id == "castcost" || id == "crossbar" || id.starts_with("hotbar");
        if (horizontal && central) return position + (available - reference) * .5f;
        if (!horizontal && (chat || central)) return std::max(0.0f, position + available - reference);
        if (horizontal && chat) return position;
        return FitLayoutAxis(position, size, reference, available);
    }

    inline auto FitLayoutViewport(const settings_store& source, const settings_store& defaults, const layout_viewport target) -> settings_store
    {
        auto result = source;
        if (!target.Valid()) return result;
        const auto reference = ReadLayoutViewport(source), baseline = ReadLayoutViewport(defaults);
        auto sections = LayoutSections(defaults);
        const auto saved = LayoutSections(source); sections.insert(saved.begin(), saved.end());
        for (const auto& section : sections)
        {
            const auto bounds = source.ReadRect(section.c_str(), defaults.ReadRect(section.c_str(), {}));
            auto preset = defaults.ReadRect(section.c_str(), {});
            const auto scaleSection = std::string{"hud.scale."} + (section == "layout.exp" ? "experience" : section.substr(7));
            const auto global = std::clamp(source.ReadFloat("appearance.hud", "scale", 1), .5f, 2.0f);
            preset.width *= global * std::clamp(source.ReadFloat(scaleSection.c_str(), "x", 1), .25f, 3.0f);
            preset.height *= global * std::clamp(source.ReadFloat(scaleSection.c_str(), "y", 1), .25f, 3.0f);
            const bool hasPreset = !defaults.ReadString(section.c_str(), "x").empty() && !defaults.ReadString(section.c_str(), "y").empty();
            const auto matches = [&](const bool horizontal) {
                const auto extent = horizontal ? reference.width : reference.height;
                const auto position = horizontal ? bounds.x : bounds.y, size = horizontal ? bounds.width : bounds.height;
                const auto expected = FitDefaultLayoutAxis(section, horizontal, horizontal ? preset.x : preset.y,
                    horizontal ? preset.width : preset.height, horizontal ? baseline.width : baseline.height, extent);
                return position == std::lround(expected) || position == std::lround(std::clamp(expected, 0.0f, std::max(0.0f, extent - size)));
            };
            const bool untouched = !source.ReadBool(section.c_str(), "position_initialized", false) &&
                hasPreset && reference.Valid() && baseline.Valid() && matches(true) && matches(false);
            for (const auto* axis : {"x", "y"})
            {
                const bool missing = source.ReadString(section.c_str(), axis).empty();
                if (missing && defaults.ReadString(section.c_str(), axis).empty()) continue;
                if ((!missing && !untouched) || !baseline.Valid()) continue;
                const bool horizontal = axis[0] == 'x';
                const auto value = FitDefaultLayoutAxis(section, horizontal, horizontal ? preset.x : preset.y,
                    horizontal ? preset.width : preset.height, horizontal ? baseline.width : baseline.height,
                    horizontal ? target.width : target.height);
                result.WriteInt(section.c_str(), axis, static_cast<int>(std::lround(value)));
            }
        }
        WriteLayoutViewport(result, target);
        return result;
    }

    inline auto RestoreDefaultLayoutPositions(const settings_store& target, const settings_store& defaults) -> void
    {
        for (const auto& section : LayoutSections(defaults))
        {
            target.WriteBool(section.c_str(), "position_initialized", false);
            for (const auto* axis : {"x", "y"})
                if (const auto value = defaults.ReadString(section.c_str(), axis); !value.empty()) target.WriteString(section.c_str(), axis, value);
        }
        WriteLayoutViewport(target, ReadLayoutViewport(defaults));
    }
}
