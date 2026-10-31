#pragma once

#include "modules/module.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace xiui::ui
{
    struct settings_topic_row
    {
        std::string id;
        const char* label{};
        const char* description{};
        size_t section{};
        bool parent{}, child{}, selected{};
        int subview{-1};
    };

    inline auto SettingsTopicBounds(rect bounds, const settings_topic_row& row) -> rect
    {
        if (row.child) { bounds.x += 12; bounds.width -= 12; }
        return bounds;
    }

    inline auto AppearanceCategory(const module_config_section& section) -> const char*
    {
        if (section.category != nullptr) return section.category;
        const auto id = section.Id();
        if (id == "layout") return "Size & Position";
        if (id == "window" || id == "appearance") return "Window";
        if (id == "bar-style") return "Bar Style";
        if (id == "slots") return "Slots";
        if (id == "colors") return "Colors";
        if (id == "entity-colors") return "Entity Colors";
        if (id == "effects") return "Effects";
        if (id == "animation") return "Animation";
        if (id == "portrait") return "Portrait";
        if (id == "jobs") return "Job Icons";
        return nullptr;
    }

    inline auto AppearanceCategoryOrder(const module_config_section& section) -> int
    {
        const auto id = section.Id();
        if (id == "layout") return 0;
        if (id == "window" || id == "appearance") return 10;
        if (id == "bar-style" || id == "slots") return 20;
        if (id == "portrait" || id == "jobs") return 30;
        if (id == "colors" || id == "entity-colors") return 40;
        if (id == "effects") return 50;
        return 60;
    }

    inline auto SettingsTopicName(const module_config_section& section) -> const char*
    { return section.Id() == "display" ? "Content" : section.name; }

    inline auto SettingsTopicPath(const std::span<const module_config_section> sections, const size_t index, const bool grouped = true) -> std::string
    {
        const auto& section = sections[index];
        if (grouped)
            if (const auto* category = AppearanceCategory(section))
            {
                const auto count = std::count_if(sections.begin(), sections.end(), [](const auto& item) { return AppearanceCategory(item) != nullptr; });
                return count > 1 ? std::string{"Appearance / "} + category : "Appearance";
            }
        return SettingsTopicName(section);
    }

    inline auto SettingsTopics(const std::span<const module_config_section> sections, const size_t selected,
        const std::string_view remembered = {}, const bool grouped = true) -> std::vector<settings_topic_row>
    {
        std::vector<size_t> appearance;
        std::vector<settings_topic_row> roots;
        bool expanded{};
        for (size_t index = 0; index < sections.size(); ++index)
        {
            const auto& section = sections[index];
            if (grouped && AppearanceCategory(section) != nullptr)
            {
                appearance.push_back(index);
                expanded |= selected == index;
            }
            else roots.push_back({std::string{section.Id()}, SettingsTopicName(section), section.description, index, false, false, selected == index});
        }
        std::stable_sort(appearance.begin(), appearance.end(), [&](const auto left, const auto right) {
            return AppearanceCategoryOrder(sections[left]) < AppearanceCategoryOrder(sections[right]);
        });
        if (!appearance.empty())
        {
            size_t destination = appearance.front();
            for (const auto index : appearance)
                if (sections[index].Id() == remembered) destination = index;
            if (expanded) destination = selected;
            roots.push_back({"appearance", "Appearance", "Size, placement, window styling, colors and visual effects.", destination, appearance.size() > 1, false, expanded});
        }
        if (grouped)
            std::stable_sort(roots.begin(), roots.end(), [](const auto& left, const auto& right) {
                const auto order = [](const auto& row) {
                    if (row.id == "general" || row.id == "actions") return 0;
                    if (row.id == "display" || row.id == "content") return 10;
                    if (row.id == "appearance") return 20;
                    if (row.id == "text") return 30;
                    return 40;
                };
                return order(left) < order(right);
            });
        std::vector<settings_topic_row> rows;
        for (auto& root : roots)
        {
            const bool children = root.parent && expanded && appearance.size() > 1;
            rows.push_back(std::move(root));
            if (children)
                for (const auto index : appearance)
                    rows.push_back({"appearance/" + std::string{sections[index].Id()}, AppearanceCategory(sections[index]),
                        sections[index].description, index, false, true, selected == index});
        }
        return rows;
    }
}
