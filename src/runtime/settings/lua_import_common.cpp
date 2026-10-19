#include "lua_import_context.hpp"

#include <array>
#include <functional>

namespace xiui::lua_import
{
    namespace
    {
        constexpr std::pair<const char*, const char*> modules[]{
            {"showPlayerBar", "player"}, {"showTargetBar", "target"}, {"showEnemyList", "enemylist"},
            {"showExpBar", "exp"}, {"showGilTracker", "gil"}, {"showInventoryTracker", "inventory"},
            {"showPartyList", "party"}, {"showCastBar", "castbar"}, {"showPetBar", "pet"},
            {"showCastCost", "castcost"}, {"showNotifications", "notifications"}, {"showReadyCheck", "readycheck"},
            {"blueMagicLearnedEnabled", "bluemagic"}, {"showPhantomRoll", "phantomroll"},
            {"magicBurstEnabled", "magicburst"}, {"treasurePoolEnabled", "treasure"}, {"showSatchelModule", "satchel"}};
        constexpr std::pair<const char*, const char*> visibility[]{
            {"playerBar", "player"}, {"targetBar", "target"}, {"enemyList", "enemylist"}, {"expBar", "experience"},
            {"gilTracker", "gil"}, {"inventoryTracker", "inventory"}, {"partyList", "party"}, {"castBar", "castbar"},
            {"petBar", "pet"}, {"castCost", "castcost"}, {"notifications", "notifications"}, {"treasurePool", "treasure"},
            {"hotbar", "hotbar"}, {"mobInfo", "mobinfo"}, {"satchel", "satchel"}, {"phantomRoll", "phantomroll"}};
        auto ResourceMode(context& data, const std::string_view path, const std::string_view section, const char* key) -> void
        { data.Choice(path, section, key, {"number", "percent", "both", "both_percent_first", "current_max", "current_max_percent"}); }
        auto Dimensions(context& data, const char* source, const char* module, const double width, const double height) -> void
        {
            const auto layout = std::string{"layout."} + module;
            data.Map(std::string{source} + "ScaleX", layout, "width", width * data.scale);
            data.Map(std::string{source} + "ScaleY", layout, "height", height * data.scale);
        }
    }
    auto ConvertCommon(context& data) -> void
    {
        auto& out = data.result.preferences;
        for (const auto& [source, destination] : modules) data.Map(source, destination, "enabled");
        data.Map("showMpCostPreview", "resources", "mp_cost_preview");
        data.Map("showMpCostPreview", "player", "mp_cost_preview"); data.Map("showMpCostPreview", "party", "mp_cost_preview");
        data.Map("hideDuringEvents", "visibility", "hide_events");
        data.Map("playerBarHideWhenInParty", "visibility", "player_in_party");
        data.Map("partyListHideOnlyAllianceOnMenuFocus", "visibility", "alliance_only_menu");
        for (const auto& [source, module] : visibility)
        {
            const auto section = std::string{"visibility."} + module;
            data.Map(std::string{source} + "HideDuringEvents", section, "hide_events");
            data.Map(std::string{source} + "HideOnMenuFocus", section, "hide_menu");
            data.Bool(std::string{source} + "HideMacroPalette", section, "keep_macro", true);
        }
        std::set<std::string> textSections, meterSections;
        out.VisitPreferences([&](std::string_view section, std::string_view, std::string_view) {
            if (section.starts_with("text.")) textSections.emplace(section);
            if (section.starts_with("meter.")) meterSections.emplace(section);
        });
        const auto family = data.String("fontFamily", "Tahoma");
        const int font = family == "Bahnschrift" ? 0 : family == "Tahoma" ? 2 : family == "Grammara" ? 3 : 1;
        if (family != "Bahnschrift" && family != "Tahoma" && family != "Grammara" && family != "Segoe UI")
            data.Issue(lua_import_severity::warning, "fontFamily", "Approximation: '" + family + "' uses Native's Segoe UI face.");
        data.Issue(lua_import_severity::warning, "fontWeight", "Approximation: Native retains its bundled font weights and fixed outline thickness.");
        for (const auto& section : textSections)
        {
            if (section.starts_with("text.chat") || section.starts_with("text.compass") || section.starts_with("text.world") || section.starts_with("text.minimap") || section.starts_with("text.recast")) continue;
            out.WriteInt(section.c_str(), "font", font);
            out.WriteBool(section.c_str(), "stroke_enabled", data.Number("fontOutlineWidth", 1) > 0);
            out.WriteBool(section.c_str(), "shadow_enabled", false); out.WriteBool(section.c_str(), "glow_enabled", false);
        }
        data.Mark("fontFamily", "closest supported Native font face"); data.Mark("fontWeight", "unsupported: Native has fixed font weights");
        data.Mark("fontOutlineWidth", "outline enabled state; thickness uses Native defaults");
        for (const auto& section : meterSections)
        {
            data.Map("barBorderThickness", section, "border_width");
            data.Map("bookendSize", section, "bookend_width");
            data.Map("healthBarFlashEnabled", section, "flash");
        }
        data.Mark("globalScale", "scaled module geometry and text");
        const auto* positions = data.Get("windowPositions");
        constexpr std::pair<const char*, const char*> windows[]{
            {"PlayerBar", "player"}, {"TargetBar", "target"}, {"PartyList", "party"}, {"PartyList2", "alliance1"}, {"PartyList3", "alliance2"},
            {"EnemyList", "enemylist"}, {"CastBar", "castbar"}, {"CastCost", "castcost"}, {"PetBar", "pet"}, {"PetBarTarget", "pettarget"},
            {"ExpBar", "exp"}, {"GilTracker", "gil"}, {"InventoryTracker", "inventory"}, {"SatchelTracker", "inventory.satchel"},
            {"SafeTracker", "inventory.safe"}, {"StorageTracker", "inventory.storage"}, {"LockerTracker", "inventory.locker"},
            {"WardrobeTracker", "inventory.wardrobe"}, {"Satchel", "satchel"}, {"TreasurePool", "treasure"},
            {"MagicBurst", "magicburst"}, {"BlueMagicLearned", "bluemagic"}, {"PhantomRoll", "phantomroll"}, {"Crossbar", "crossbar"}};
        if (positions)
            for (const auto& [source, module] : windows)
            {
                const auto prefix = std::string{"windowPositions."} + source + ".";
                data.Maps(prefix, std::string{"layout."} + module, {{"x", "x"}, {"y", "y"}});
            }
        data.Ignore("appliedPositions", "Lua layout application cache");
        for (const auto name : {"blueMagicLearnedPreview", "enemyListPreview", "hotbarPreview", "treasurePoolPreview", "partyListPreview", "petBarPreview", "petBarPreviewType", "satchelVisible"})
            data.Ignore(name, "edit preview or transient window state");

        Dimensions(data, "playerBar", "player", 500, 62);
        data.Maps("", "player", {{"playerBarTpFlashEnabled", "tp_ready_blink"}});
        data.Bool("alwaysShowMpBar", "player", "hide_zero_mp", true);
        ResourceMode(data, "playerBarHpDisplayMode", "player", "hp_display"); ResourceMode(data, "playerBarMpDisplayMode", "player", "mp_display");
        for (const auto resource : {"Hp", "Mp", "Tp"})
        {
            std::string key{resource}; key[0] = static_cast<char>(std::tolower(key[0]));
            const auto text = key == "hp" ? "text.player.values" : "text.player." + key + ".values";
            data.TextSize("playerBarFontSize", text);
            data.Map(std::string{"playerBar"} + resource + "TextOffsetX", text, "offset_x", data.scale);
            data.Map(std::string{"playerBar"} + resource + "TextOffsetY", text, "offset_y", data.scale);
            const auto alignment = "playerBar" + std::string{resource} + "TextAlignment";
            if (data.Get(alignment))
            { out.WriteInt(text.c_str(), "placement", data.String(alignment) == "left" ? 1 : 3); data.Mark(alignment, text + ".placement"); }
            data.Map("showPlayerBarBookends", "meter.player." + key, "bookends");
        }
        data.Map("playerBarRestingTicker", "timing.player", "resting");
        data.Map("showPlayerBarBookends", "meter.player.hp", "bookends");
        out.WriteBool("player", "show_identity", false); out.WriteBool("player", "show_labels", false);
        out.WriteInt("player", "resource_text_version", 1);

        Dimensions(data, "targetBar", "target", 500, 13);
        data.Maps("", "target", {{"showTargetDistance", "show_distance"}, {"showTargetHpPercent", "show_percent"},
            {"showTargetName", "show_name"}, {"showTargetBarLockOnBorder", "show_lock"}, {"showTargetBarCastBar", "show_casts"}});
        data.Map("showTargetHpPercentAllTargets", "target", "percent_players");
        data.Map("showTargetBarBookends", "meter.target.hp", "bookends");
        for (const auto& [source, destination] : {std::pair{"Name", "name"}, {"Distance", "distance"}, {"Percent", "values"}, {"Cast", "cast"}})
        {
            const auto text = std::string{"text.target."} + destination;
            data.TextSize(std::string{"targetBar"} + source + "FontSize", text);
            data.Maps(std::string{"targetBar"} + source, text, {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        }
        data.Map("targetBarCastBarScaleY", "target.cast", "height", 13 * data.scale);
        data.Map("targetBarCastBarOffsetY", "target.cast", "spacing", data.scale);
        if (data.Get("showEnemyId"))
        { out.WriteInt("target", "enemy_id_format", !data.Boolean("showEnemyId") ? 0 : data.Boolean("showEnemyIdHex") ? 2 : 1); data.Mark("showEnemyId", "target.enemy_id_format"); data.Mark("showEnemyIdHex", "target.enemy_id_format"); }
        data.Map("targetBarHpColorByType", "target.entity_fills", "enabled");
        data.Map("splitTargetOfTarget", "target.target_of_target", "enabled");
        data.Map("totBarScaleX", "target.target_of_target", "width", 500 * data.scale);
        data.Map("totBarScaleY", "target.target_of_target", "bar_height", 10 * data.scale);
        data.TextSize("totBarFontSize", "text.target.target_of_target.name");
        data.Maps("subtargetBar", "target.subtarget", {{"ShowDistance", "distance"}, {"ShowHpPercent", "percent"}, {"ShowMobLevel", "mob_level"}});
        data.Map("showSubtargetBar", "target.subtarget", "enabled");
        data.Map("subtargetBarScaleX", "target.subtarget", "width", 500 * data.scale);
        data.Map("subtargetBarScaleY", "target.subtarget", "bar_height", 13 * data.scale);
        data.TextSize("subtargetBarFontSize", "text.target.subtarget.name"); data.TextSize("subtargetBarPercentFontSize", "text.target.subtarget.percent");
        data.Map("subtargetBarShowBookends", "meter.target.subtarget", "bookends");

        data.Maps("enemyList", "enemylist", {{"RowsPerColumn", "rows_per_column"}, {"MaxColumns", "max_columns"}, {"RowSpacing", "spacing"},
            {"ColumnSpacing", "column_spacing"}, {"TargetOffsetX", "target_offset_x"}, {"TargetOffsetY", "target_offset_y"},
            {"TargetWidth", "target_width"}, {"BackgroundOpacity", "opacity"}, {"TargetBackgroundOpacity", "target_opacity"}, {"AlignBottom", "bottom"}});
        data.Maps("", "enemylist", {{"showEnemyDistance", "distance"}, {"showEnemyHPPText", "hp_percent"}, {"showEnemyListTargets", "enemy_targets"},
            {"showEnemyListCastBar", "casts"}, {"showEnemyListBorders", "borders"}, {"showEnemyListBordersUseNameColor", "claim_border"}, {"enableEnemyListClickTarget", "click_target"}});
        data.Map("enemyListScaleX", "layout.enemylist", "width", 125 * data.scale);
        data.Map("enemyListScaleY", "meter.enemylist.hp", "height", 7 * data.scale);
        for (const auto& [source, target] : {std::pair{"Name", "name"}, {"Distance", "details"}, {"Target", "target"}})
            data.TextSize(std::string{"enemyList"} + source + "FontSize", std::string{"text.enemylist."} + target);
        data.Map("showEnemyListBookends", "meter.enemylist.hp", "bookends");
        data.Map("showEnemyListDebuffs", "status.enemylist", "enabled"); data.Map("showEnemyListDebuffTimers", "status.enemylist", "timers");
        data.Map("showEnemyListDebuffTimersCCOnly", "status.enemylist", "cc_only");
        data.Map("showUncertainDebuffMarker", "status.enemylist", "uncertain");
        data.Map("showUncertainDebuffMarker", "status.target", "uncertain");
        data.Map("enemyListIconScale", "status.enemylist", "size", 18 * data.scale);
        data.Map("enemyListDebuffOffsetX", "status.enemylist", "offset_x", data.scale);
        data.Map("enemyListDebuffOffsetY", "status.enemylist", "offset_y", data.scale);
        data.Choice("enemyListDebuffsAnchor", "status.enemylist", "side", {"left", "right"});
        data.Ignore("maxEnemyListEntries", "superseded by rows per column and maximum columns");

        Dimensions(data, "expBar", "exp", 500, 13);
        data.Maps("expBar", "exp", {{"ShowText", "show_current"}, {"ShowPercent", "show_percent"}});
        data.Map("showExpBarBookends", "meter.experience", "bookends");
        for (const auto& [source, target] : {std::pair{"Job", "header"}, {"Exp", "details"}})
        {
            const auto text = std::string{"text.experience."} + target;
            data.TextSize("expBarFontSize", text);
            data.Maps(std::string{"expBar"} + source + "Text", text, {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        }
        if (data.Get("expBarLimitPointsMode"))
        { out.WriteInt("exp", "mode", data.Boolean("expBarMasteryMode") ? 3 : data.Boolean("expBarLimitPointsMode") ? 2 : 1); data.Mark("expBarMasteryMode", "exp.mode"); data.Mark("expBarLimitPointsMode", "exp.mode"); }
        data.Bool("expBarInlineMode", "exp", "info_position", true);

        data.Maps("gilTracker", "gil", {{"RightAlign", "right_align"}, {"IconRight", "icon_right"}, {"ShowIcon", "icon"}, {"ShowGilPerHour", "tracking"}});
        data.TextSize("gilTrackerFontSize", "text.gil.balance", data.Number("gilTrackerScale", 1));
        data.TextSize("gilTrackerFontSize", "text.gil.tracking", data.Number("gilTrackerScale", 1));
        data.Mark("gilTrackerScale", "Gil text and icon scale");
        data.Maps("gilTrackerText", "text.gil.balance", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        data.Maps("gilTrackerGilPerHour", "text.gil.tracking", {{"OffsetX", "offset_x"}, {"OffsetY", "offset_y"}});
        data.Map("gilTrackerDisplayMode", "gil", "tracking_mode", 1);

        const std::array trackers{"inventory", "satchel", "safe", "storage", "locker", "wardrobe"};
        for (const auto* tracker : trackers)
        {
            const auto name = std::string{tracker}; const auto section = name == "inventory" ? name : "inventory." + name;
            const auto prefix = name + "Tracker";
            data.Maps(prefix, section, {{"ColumnCount", "columns"}, {"RowCount", "rows"}, {"Opacity", "opacity"}, {"ColorThreshold1", "warning_slots"}, {"ColorThreshold2", "critical_slots"}});
            data.Maps(name, section, {{"ShowCount", "count"}, {"ShowDots", "dots"}, {"ShowLabels", "label"}, {"TextUseThresholdColor", "text_threshold_color"}, {"ShowPerContainer", "separate_containers"}});
            if (name != "inventory") { auto title = name; title[0] = static_cast<char>(std::toupper(title[0])); data.Map("show" + title + "Tracker", section, "enabled"); }
            const auto multiplier = data.Number(prefix + "Scale", 1);
            data.TextSize(prefix + "FontSize", "text." + section + ".label", multiplier);
            data.TextSize(prefix + "FontSize", "text." + section + ".count", multiplier);
            data.Map(prefix + "Scale", section, "dot_size", 7 * data.scale); data.Map(prefix + "Scale", section, "dot_gap", 2 * data.scale);
            out.WriteInt(section.c_str(), "threshold_mode", 0);
        }
    }
}
