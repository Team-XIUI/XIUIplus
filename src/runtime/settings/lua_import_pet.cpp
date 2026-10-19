#include "lua_import_context.hpp"

#include <cctype>
#include <tuple>

namespace xiui::lua_import
{
    auto ConvertPet(context& data) -> void
    {
        auto& out = data.result.preferences;
        for (const auto& [source, native] : {std::pair{"Avatar", "avatar"}, {"Charm", "charm"}, {"Jug", "jug"}, {"Wyvern", "wyvern"}, {"Automaton", "automaton"}})
        {
            const auto prefix = std::string{"petBar"} + source + ".";
            const auto section = std::string{"pet."} + native;
            out.WriteBool(section.c_str(), "split_values", true);
            data.Maps(prefix, section, {{"showDistance", "distance"}, {"showHP", "hp"}, {"showMP", "mp"}, {"showTP", "tp"},
                {"showTimers", "recasts"}, {"backgroundOpacity", "opacity"}, {"recastTopSpacing", "spacing"}});
            data.Window(prefix, section);
            data.Map("petBarShowImage", section, "portrait");
            data.Map("petBarShowStatusIcons", "status." + section, "enabled");
            data.Map("petBarStatusIconSize", "status." + section, "size", data.scale);
            data.Map("petBarImageScale", section, "portrait_size", 126 * data.scale);
            data.Map("petBarImageOpacity", "portrait." + section, "opacity");
            data.Map("petBarImageOffsetX", "portrait." + section, "offset_x", data.scale);
            data.Map("petBarImageOffsetY", "portrait." + section, "offset_y", data.scale);
            const double scaleX = data.Number(prefix + "scaleX", 1) * data.scale, scaleY = data.Number(prefix + "scaleY", 1) * data.scale;
            data.Mark(prefix + "scaleX", section + " geometry"); data.Mark(prefix + "scaleY", section + " geometry");
            for (const auto& [resource, title] : {std::pair{"hp", "Hp"}, {"mp", "Mp"}, {"tp", "Tp"}})
            {
                std::string resourceKey{resource};
                for (auto& letter : resourceKey) letter = static_cast<char>(std::toupper(letter));
                const auto meter = "meter." + section + "." + resourceKey;
                const auto sourceKey = std::string{resource};
                data.Map(prefix + sourceKey + "ScaleX", section, (resourceKey + "_width").c_str(), 150 * scaleX);
                data.Map(prefix + sourceKey + "ScaleY", meter, "height", 14 * scaleY);
                data.Map(prefix + "showBookends", meter, "bookends");
                data.TextSize(prefix + sourceKey + "FontSize", "text." + section + "." + resource, scaleY / data.scale);
                const auto color = std::string{"colorCustomization.petBar"} + source + "." + resource;
                data.Gradient(color + "Gradient", section, (resourceKey + "_top").c_str(), (resourceKey + "_bottom").c_str());
                data.TextColor(color + "TextColor", "text." + section + "." + resource);
            }
            data.TextSize(prefix + "nameFontSize", "text." + section + ".name", scaleY / data.scale);
            data.TextSize(prefix + "distanceFontSize", "text." + section + ".details", scaleY / data.scale);
            data.Maps(prefix + "distance", "text." + section + ".details", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
            data.TextSize(prefix + "recastFullNameFontSize", "text." + section + ".recasts", scaleY / data.scale);
            data.TextSize(prefix + "recastFullTimerFontSize", "text." + section + ".recast_timer", scaleY / data.scale);
            data.Map(prefix + "alignBottom", section, "align_bottom");
            const auto recasts = section + ".recasts";
            data.Choice(prefix + "recastDisplayStyle", recasts, "display", {"text", "full", "compact", "icons"});
            data.Choice(prefix + "timerFillStyle", recasts, "fill", {"square", "circle", "clock"});
            data.Choice(prefix + "recastFullAlignment", recasts, "alignment", {"left", "center", "right"});
            data.Maps(prefix, recasts, {{"recastFullShowName", "names"}, {"recastFullShowTimer", "timers"}, {"recastFullSpacing", "spacing"},
                {"recastTopSpacing", "top_spacing"}, {"iconsAbsolute", "absolute"}, {"iconsOffsetX", "offset_x"}, {"iconsOffsetY", "offset_y"}});
            data.Map(prefix + "recastScaleX", recasts, "width", 150 * scaleX);
            data.Map(prefix + "recastScaleY", recasts, "height", 14 * scaleY);
            data.Map(prefix + "iconsScale", recasts, "icon_size", 32 * data.scale);
            if (data.Get(prefix + "showImage")) data.Map(prefix + "showImage", section, "portrait");
            data.Map(prefix + "imageScale", section, "portrait_size", 126 * data.scale);
            data.Maps(prefix, "portrait." + section, {{"imageOpacity", "opacity"}, {"imageOffsetX", "offset_x"}, {"imageOffsetY", "offset_y"}, {"imageClipToBackground", "clip"}});
            const auto colors = std::string{"colorCustomization.petBar"} + source + ".";
            data.TextColor(colors + "nameTextColor", "text." + section + ".name");
            data.TextColor(colors + "distanceTextColor", "text." + section + ".details");
            for (const auto& [name, key, job] : {std::tuple{"BPRage", "rage", "Smn"}, {"BPWard", "ward", "Smn"}, {"Apogee", "apogee", "Smn"},
                {"ManaCede", "mana_cede", "Smn"}, {"Ready", "ready", "Bst"}, {"Reward", "reward", "Bst"}, {"CallBeast", "call_beast", "Bst"},
                {"BestialLoyalty", "bestial_loyalty", "Bst"}, {"CallWyvern", "call_wyvern", "Drg"}, {"SpiritLink", "spirit_link", "Drg"},
                {"DeepBreathing", "deep_breathing", "Drg"}, {"SteadyWing", "steady_wing", "Drg"}, {"Activate", "activate", "Pup"},
                {"Repair", "repair", "Pup"}, {"DeusExAutomata", "deus_ex_automata", "Pup"}, {"Deploy", "deploy", "Pup"},
                {"Deactivate", "deactivate", "Pup"}, {"Retrieve", "retrieve", "Pup"}, {"2h", "two_hour", ""}})
            {
                const auto ability = recasts + "." + key;
                data.Gradient(colors + "timer" + name + "ReadyGradient", ability, "ready_top", "ready_bottom");
                data.Gradient(colors + "timer" + name + "RecastGradient", ability, "cooldown_top", "cooldown_bottom");
                data.Map(std::string{"petBar"} + job + "Show" + (std::string{key} == "two_hour" ? "2HourAbility" : name), ability, "enabled");
            }
        }
        if (const auto* portraits = data.Get("petBarAvatarSettings"))
            for (const auto& [key, image] : portraits->fields)
            {
                const auto prefix = "petBarAvatarSettings." + key + ".";
                const auto section = "pet.portrait." + (key == "airspirit" ? "windspirit" : key);
                data.Maps(prefix, section, {{"scale", "scale"}, {"opacity", "opacity"}, {"offsetX", "offset_x"}, {"offsetY", "offset_y"}, {"clipToBackground", "clip"}});
            }
        data.Maps("", "pet.target", {{"petBarShowTarget", "enabled"}, {"petTargetSnapToPetBar", "snapped"}, {"petTargetSnapOffsetX", "offset_x"},
            {"petTargetSnapOffsetY", "offset_y"}, {"petTargetBackgroundOpacity", "background_opacity"}});
        data.Map("petTargetBarScaleX", "pet.target", "width", 150 * data.scale);
        data.Map("petTargetBarScaleY", "meter.pet.target", "height", 8 * data.scale);
        data.Gradient("colorCustomization.petTarget.hpGradient", "pet.target");
        data.TextColor("colorCustomization.petTarget.nameTextColor", "text.pet.target.name");
        data.TextColor("colorCustomization.petTarget.distanceTextColor", "text.pet.target.details");
        data.TextSize("petBarTargetFontSize", "text.pet.target.name");
        data.Maps("petTargetName", "text.pet.target.name", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        data.Maps("petTargetDistance", "text.pet.target.details", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        out.WriteInt("pet", "portrait_preset_version", 1);
    }
}
