#include "lua_import_context.hpp"
#include "modules/castbar/cast_adjustments.hpp"

namespace xiui::lua_import
{
    auto ConvertExtras(context& data) -> void
    {
        auto& out = data.result.preferences;
        data.Map("castBarFastCastEnabled", "castbar.fastcast", "enabled");
        for (size_t job = 0; job < std::size(cast_job_names); ++job)
            data.Map("castBarFastCast." + std::to_string(job + 1), "castbar.fastcast", cast_job_names[job], 100);
        for (const auto& [source, destination] : {std::pair{"RDMSJ", "rdm_sub"}, {"WHMCureSpeed", "cure"}, {"BRDSingSpeed", "singing"}, {"SMNAvatarSpeed", "summoning"}})
            data.Map(std::string{"castBarFastCast"} + source, "castbar.fastcast", destination, 100);
        for (const auto& [source, destination] : {std::pair{"Lullaby", "lullaby"}, {"Requiem", "requiem"}, {"Elegy", "elegy"},
            {"Threnody", "threnody"}, {"Nocturne", "nocturne"}, {"Virelai", "virelai"}})
            data.Map(std::string{"songPlus"} + source, "combat.songs", destination);
        data.Map("castBarScaleX", "layout.castbar", "width", 500 * data.scale);
        data.Map("castBarScaleY", "meter.castbar", "height", 14 * data.scale);
        data.Map("showCastBarBookends", "meter.castbar", "bookends");
        data.TextSize("castBarFontSize", "text.castbar.name"); data.TextSize("castBarFontSize", "text.castbar.percent");
        data.Gradient("colorCustomization.castBar.barGradient", "castbar");
        data.TextColor("colorCustomization.castBar.spellTextColor", "text.castbar.name");
        data.TextColor("colorCustomization.castBar.percentTextColor", "text.castbar.percent");
        data.Maps("castCost.", "castcost", {{"showName", "show_name"}, {"showMpCost", "show_cost"}, {"showRecast", "show_timing"},
            {"showCooldown", "show_cooldown"}, {"padding", "padding"}, {"backgroundOpacity", "opacity"}});
        data.Window("castCost.", "castcost", "backgroundTheme", "container");
        data.Map("castCost.minWidth", "layout.castcost", "width", data.scale);
        data.Map("castCost.windowPosX", "layout.castcost", "x"); data.Map("castCost.windowPosY", "layout.castcost", "y");
        data.Map("castCost.barScaleY", "meter.castcost", "height", 14 * data.scale);
        for (const auto& [source, native] : {std::pair{"name", "name"}, {"cost", "cost"}, {"time", "timing"}, {"recast", "recast_value"}})
            data.TextSize(std::string{"castCost."} + source + "FontSize", std::string{"text.castcost."} + native);
        data.Gradient("colorCustomization.castCost.cooldownBarGradient", "castcost");
        for (const auto& [source, native] : {std::pair{"nameTextColor", "name"}, {"mpCostTextColor", "cost"}, {"timeTextColor", "timing"}, {"cooldownTextColor", "recast_value"}})
            data.TextColor(std::string{"colorCustomization.castCost."} + source, std::string{"text.castcost."} + native);
        data.Color("colorCustomization.castCost.readyTextColor", "castcost", "ready_color");
        data.Color("colorCustomization.castCost.mpNotEnoughColor", "castcost", "unavailable_color");

        data.Maps("", "satchel", {{"satchelColumns", "columns"}, {"satchelRows", "rows"}, {"satchelSlotSize", "icon_size"},
            {"satchelShowEmptySlots", "empty_slots"}, {"satchelOverrideCommand", "override_satchel"}, {"satchelAutoSortBags", "autosort"}});
        data.TextSize("satchelTooltipFontSize", "text.satchel.items");
        data.Map("satchelTooltipFontFamily", "text.satchel.items", "font_family");

        data.Maps("", "treasure", {{"treasurePoolExpanded", "expanded"}, {"treasurePoolBackgroundOpacity", "opacity"},
            {"treasurePoolAlignBottom", "bottom"}, {"treasurePoolReverseGrowDirection", "reverse"}});
        for (const auto name : {"title", "item", "details", "controls"}) data.TextSize("treasurePoolFontSize", std::string{"text.treasure."} + name);

        data.Maps("readyCheck", "readycheck", {{"SoundOnChecker", "sound_checker"}, {"SoundOnPrompt", "sound_prompt"}, {"SoundVolume", "volume"}});
        data.Map("readyCheckSoundFile", "readycheck", "sound_file");
        if (data.Get("readyCheckSoundFile")) out.WriteInt("readycheck", "sound_choice", 1);
        data.Maps("blueMagicLearned", "bluemagic", {{"Duration", "duration"}, {"Scale", "scale"}});
        data.Maps("magicBurst", "magicburst", {{"ShowTimer", "timer"}});
        data.Map("magicBurstScale", "layout.magicburst", "width", 160 * data.scale);
        data.Map("magicBurstScale", "layout.magicburst", "height", 80 * data.scale);
        data.Map("phantomRollScale", "phantomroll", "scale", data.scale);
        if (data.Get("phantomRollHorizonMode"))
        { out.WriteInt("phantomroll", "rules", data.Boolean("phantomRollHorizonMode") ? 1 : 0); data.Mark("phantomRollHorizonMode", "phantomroll.rules"); }

        data.Map("showMobInfo", "mobinfo", "enabled"); data.Map("showMobInfo", "target.mobinfo", "enabled");
        data.Maps("mobInfo", "target.mobinfo", {{"ShowDetection", "behavior"}, {"ShowResistances", "ranks"}, {"ShowWeaknesses", "damage"},
            {"ShowImmunities", "immunities"}, {"ShowServerId", "server_id"}, {"ShowNoData", "unknown"}});
        data.Map("mobInfoShowLevel", "target", "mob_level");
        const auto* types = data.Get("notificationTypeGroup");
        const std::pair<const char*, const char*> notifications[]{{"partyInvite", "party"}, {"tradeInvite", "trade"}, {"treasurePool", "treasure"},
            {"itemObtained", "item"}, {"keyItemObtained", "key_item"}, {"gilObtained", "gil"}};
        data.Map("notificationGroupCount", "notifications", "group_count");
        for (const auto& [source, native] : notifications)
            if (types && types->Find(source))
            {
                const auto path = std::string{"notificationTypeGroup."} + source;
                const std::string_view kind{native};
                const auto index = kind == "party" ? "7" : kind == "trade" ? "8" : kind == "treasure" ? "6" : kind == "item" ? "0" : kind == "key_item" ? "5" : "4";
                out.WriteInt("notifications.routing", index, static_cast<int>(data.Number(path, 1))); data.Mark(path, std::string{"notifications.routing."} + native);
            }
        data.Maps("notificationsShow", "notifications", {{"PartyInvite", "party_invites"}, {"TradeInvite", "trade_invites"}, {"Treasure", "treasure"},
            {"Items", "items"}, {"KeyItems", "key_items"}, {"Gil", "gil"}});
        for (size_t group = 1; group <= 6; ++group)
        {
            const auto prefix = "notificationGroup" + std::to_string(group) + ".";
            const auto section = group == 1 ? "notifications" : "notifications.group" + std::to_string(group);
            data.Maps(prefix, section, {{"displayDuration", "duration"}, {"inviteMinifyTimeout", "minify_after"}, {"spacing", "row_spacing"},
                {"maxVisible", "visible_count"}, {"padding", "padding"}});
            data.Choice(prefix + "direction", section, "stack_direction", {"up", "down"});
            const auto position = "windowPositions.Notifications_Group" + std::to_string(group) + ".";
            data.Maps(position, "layout." + section, {{"x", "x"}, {"y", "y"}});
            out.WriteBool(section.c_str(), "screen_anchor", false);
            data.Window(prefix, section);
            data.TextSize(prefix + "titleFontSize", "text." + section);
            const auto colors = "colorCustomization.notifications.";
            for (const auto& [source, native] : {std::pair{"itemObtainedColor", "item_color"}, {"partyInviteColor", "party_color"},
                {"tradeInviteColor", "trade_color"}, {"treasurePoolColor", "treasure_color"}, {"keyItemColor", "key_item_color"}, {"gilColor", "gil_color"}})
                data.Color(std::string{colors} + source, section, native);
            data.TextColor("colorCustomization.notifications.textColor", "text." + section);
        }
        for (const auto name : {"notificationsSplitPartyInvite", "notificationsSplitTradeInvite", "notificationsSplitTreasurePool", "notificationsSplitItemObtained", "notificationsSplitKeyItemObtained", "notificationsSplitGilObtained"})
            data.Ignore(name, "superseded by notification type-to-group routing");
        data.Gradient("colorCustomization.enemyList.hpGradient", "enemylist", "hp_top", "hp_bottom");
        data.Gradient("colorCustomization.targetBar.hpGradient", "meter.target.hp");
        data.Gradient("colorCustomization.targetBar.castBarGradient", "target.cast");
        data.Gradient("colorCustomization.totBar.hpGradient", "target.target_of_target");
        data.Gradient("colorCustomization.subtargetBar.hpGradient", "target.subtarget");
        data.Gradient("colorCustomization.playerBar.mpGradient", "meter.player.mp");
        data.Gradient("colorCustomization.playerBar.tpGradient", "meter.player.tp");
        data.Gradient("colorCustomization.playerBar.tpOverlayGradient", "player", "tp_overlay_top", "tp_overlay_bottom");
        data.Color("colorCustomization.playerBar.tpFlashColor", "player", "tp_flash_color");
        data.TextColor("colorCustomization.playerBar.hpTextColor", "text.player.values");
        data.TextColor("colorCustomization.playerBar.mpTextColor", "text.player.mp.values");
        data.TextColor("colorCustomization.playerBar.tpEmptyTextColor", "text.player.tp.values");
        data.Color("colorCustomization.playerBar.tpFullTextColor", "player", "tp_ready_color");
        data.TextColor("colorCustomization.targetBar.distanceTextColor", "text.target.distance");
        data.TextColor("colorCustomization.targetBar.castTextColor", "text.target.cast");
        data.TextColor("colorCustomization.subtargetBar.distanceTextColor", "text.target.subtarget.distance");
        data.Gradient("colorCustomization.expBar.barGradient", "meter.experience");
        for (const auto& [source, native] : {std::pair{"playerPartyTextColor", "group_player"}, {"playerOtherTextColor", "player"}, {"npcTextColor", "npc"},
            {"mobUnclaimedTextColor", "unclaimed_mob"}, {"mobPartyClaimedTextColor", "group_mob"}, {"mobOtherClaimedTextColor", "other_mob"}})
            data.Color(std::string{"colorCustomization.shared."} + source, "entity.colors", native);
        out.WriteBool("entity.colors", "enabled", true);
    }
}
