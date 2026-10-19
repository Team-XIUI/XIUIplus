#include "lua_import_context.hpp"

#include <array>

namespace xiui::lua_import
{
    auto ConvertParty(context& data) -> void
    {
        auto& out = data.result.preferences;
        for (const auto& [source, section] : {std::pair{"partyA", "party"}, {"partyB", "alliance1"}, {"partyC", "alliance2"}})
        {
            const auto prefix = std::string{source} + ".";
            const auto text = std::string{"text."} + section + ".";
            data.Maps(prefix, section, {{"alwaysShowMpBar", "always_mp"}, {"alignBottom", "bottom"}, {"showCastBars", "casts"},
                {"showDistance", "distance"}, {"showJobIcon", "job_icons"}, {"showJob", "jobs"}, {"showMainJobLevel", "main_level"},
                {"showSubJob", "sub_job"}, {"showSubJobLevel", "sub_level"}, {"showTP", "show_tp"}, {"flashTP", "flash_tp"},
                {"showTitle", "title"}, {"showSelectionBox", "selection"}, {"expandHeight", "expand"}, {"expandHeightInAlliance", "expand_alliance"},
                {"minRows", "minimum_rows"}, {"entrySpacing", "spacing"}, {"distanceHighlight", "distance_highlight"},
                {"backgroundOpacity", "opacity"}});
            data.Window(prefix, section, "backgroundName");
            data.Map("showPartyListWhenSolo", section, "solo");
            if (std::string_view{section} != "party") data.Map("partyListAlliance", section, "enabled");
            data.Map("partyListHideDuringEvents", std::string{"visibility."} + section, "hide_events");
            data.Map(prefix + "jobIconScale", section, "icon_size", 22 * data.scale);
            data.Map(prefix + "buffScale", section, "buff_size", 18 * data.scale);
            data.Map(prefix + "statusSide", section, "status_placement");
            data.Choice(prefix + "castBarStyle", section, "cast_placement", {"separate", "name", "mp", "tp"});
            if (data.Get(prefix + "layout"))
            {
                out.WriteInt(section, "layout", data.Number(prefix + "layout") == 1 ? 1 : 0);
                data.Mark(prefix + "layout", std::string{section} + ".layout");
            }
            const auto templatePrefix = data.Number(prefix + "layout") == 1 ? "layoutCompact." : "layoutHorizontal.";
            const double scaleX = data.Number(prefix + "scaleX", 1) * data.scale, scaleY = data.Number(prefix + "scaleY", 1) * data.scale;
            double width{};
            for (const auto resource : {"hp", "mp", "tp"})
            {
                const auto meter = std::string{"meter."} + section + "." + resource;
                const auto key = std::string{resource};
                const auto sourceWidth = std::string{templatePrefix} + key + "BarWidth";
                const auto sourceX = prefix + key + "BarScaleX", sourceY = prefix + key + "BarScaleY";
                const auto value = data.Number(sourceWidth) * data.Number(sourceX, 1) * scaleX;
                out.WriteFloat(meter.c_str(), "width", static_cast<float>(value));
                out.WriteFloat(meter.c_str(), "height", static_cast<float>(14 * data.Number(sourceY, 1) * scaleY));
                data.Mark(sourceWidth, meter + ".width"); data.Mark(sourceX, meter + ".width"); data.Mark(sourceY, meter + ".height");
                data.Map(prefix + "showBookends", meter, "bookends");
                width += value;
            }
            out.WriteFloat((std::string{"layout."} + section).c_str(), "width", static_cast<float>(width + 32 * scaleX));
            data.Mark(prefix + "scaleX", "party geometry"); data.Mark(prefix + "scaleY", "party geometry");
            data.Map(std::string{templatePrefix} + "barSpacing", section, "bar_gap", scaleX);
            const bool split = data.Boolean(prefix + "splitFontSizes");
            for (const auto& [sourceText, targetText] : {std::pair{"name", "name"}, {"hp", "values"}, {"mp", "mp"}, {"tp", "tp"}, {"distance", "details"}, {"job", "jobs"}, {"zone", "zone"}})
            {
                const auto destination = text + targetText;
                const auto sourceFont = prefix + sourceText + "FontSize";
                const auto pixels = data.Number(split ? sourceFont : prefix + "fontSize", 12) * scaleY;
                out.WriteFloat(destination.c_str(), "scale", static_cast<float>(pixels / 15));
                data.Mark(sourceFont, destination + ".scale");
                for (const auto axis : {"X", "Y"})
                {
                    const auto local = prefix + sourceText + "TextOffset" + axis;
                    const auto shared = std::string{templatePrefix} + sourceText + "TextOffset" + axis;
                    const auto field = std::string{"offset_"} + (std::string_view{axis} == "X" ? "x" : "y");
                    const auto offset = (data.Number(local) + data.Number(shared)) * (std::string_view{axis} == "X" ? scaleX : scaleY);
                    out.WriteFloat(destination.c_str(), field.c_str(), static_cast<float>(offset));
                    if (data.Get(local)) data.Mark(local, destination + "." + field);
                    if (data.Get(shared)) data.Mark(shared, destination + "." + field);
                }
            }
            data.Mark(prefix + "fontSize", "party text scale"); data.Mark(prefix + "splitFontSizes", "resolved per-element font sizes");
            data.TextSize("partyListTitleFontSize", text + "heading", scaleY / data.scale);
            data.Choice(prefix + "hpDisplayMode", section, "hp_display", {"number", "percent", "both", "both_percent_first", "current_max", "current_max_percent"});
            data.Choice(prefix + "mpDisplayMode", section, "mp_display", {"number", "percent", "both", "both_percent_first", "current_max", "current_max_percent"});
            data.Map(prefix + "castBarScaleX", std::string{"meter."} + section + ".cast", "width", 150 * scaleX);
            data.Map(prefix + "castBarScaleY", std::string{"meter."} + section + ".cast", "height", 14 * scaleY);
            data.Map(prefix + "castBarOffsetX", std::string{"meter."} + section + ".cast", "offset_x", scaleX);
            data.Map(prefix + "castBarOffsetY", std::string{"meter."} + section + ".cast", "offset_y", scaleY);
            data.Map("showPartyList", section, "enabled");
            out.WriteInt(section, "tp_text_version", 1); out.WriteInt(section, "mp_text_version", 1);
            out.WriteInt(section, "status_icon_version", 1); out.WriteInt(section, "name_cast_anchor_version", 1);
        }
        // These legacy aliases are superseded by the supported per-party settings.
        for (const auto key : {"partyListLayout", "partyListDistanceHighlight", "partyListBuffScale", "partyListCastBarScaleY", "partyListCastBars", "partyListStatusTheme", "partyListTheme", "partyListFlashTP", "showPartyListBookends", "showPartyJobIcon", "showPartyListTitle", "showPartyListDistance", "showPartyListJob", "partyListCursor", "partyListBackgroundName", "partyListExpandHeight", "partyListAlignBottom", "partyListBgScale", "partyListBgColor", "partyListBorderColor", "partyListLayout1", "partyListLayout2"})
            data.Ignore(key, "superseded by per-party settings in supported profile versions");
    }
}
