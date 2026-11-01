#include "modules/target/target_module.hpp"
#include "world_projection.hpp"

#include <cstdio>

namespace xiui
{
    struct mob_info_test
    {
    static auto Run(void) -> bool
    {
        const auto check = [](const bool value, const char* message) { if (!value) std::fprintf(stderr, "Mob Info: %s\n", message); return value; };
        const auto* mob = MobById(16781313);
        if (!check(mob != nullptr && mob->zone == 1 && MobDataName(*mob) == "Snipper" && MobLevel(*mob) == "Lv. 15-19" &&
            MobDetection(*mob) == "Sound", "pinned server-ID record resolves with its original fields")) return false;
        if (!check(MobById(0) == nullptr && MobById(0xFFFFFFFF) == nullptr && MobById(16781312) == nullptr,
            "unmatched IDs remain unknown")) return false;
        if (!check(MobForEntity(mob->id, 1, true) == mob && MobForEntity(mob->id, 2, true) == nullptr &&
            MobForEntity(mob->id, 1, false) == nullptr && MobForEntity(mob->id, 0, true) == nullptr,
            "target lookup rejects other zones, unavailable zones and non-monsters")) return false;
        if (!check(mob->damage[0] == 0 && mob->elements[1] == -3 && mob->statuses[5] == -2,
            "packed damage adjustments and elemental/status ranks retain distinct signed values")) return false;
        mob_record sample{}; sample.immunity = 0x800; sample.minimum_level = sample.maximum_level = 75;
        if (!check(MobImmunities(sample) == "Light Sleep" && MobLevel(sample) == "Lv. 75", "light sleep immunity does not imply dark sleep immunity")) return false;
        sample.minimum_level = 0;
        if (!check(MobLevel(sample) == "Unknown level", "script-dependent level is not invented")) return false;
        auto settings = settings_store::FromIniText("[mobinfo]\nenabled=1\nranks=0\n[layout.mobinfo]\nx=123\ny=234\nwidth=400\nheight=250\n[layout.target]\nx=77\ny=88\nwidth=440\nheight=82\n");
        target_module target; target.ApplyTextDefaults(ui::theme::CreateDefault()); target.Load(settings);
        const auto saved = settings_store::FromIniText(""); target.Save(saved);
        if (!check(saved.ReadBool("target.mobinfo", "enabled", false) && !saved.ReadBool("target.mobinfo.mobelementranks", "enabled", true) &&
            saved.ReadRect("layout.target", {}).x == 77 && saved.ReadString("layout.mobinfo", "x").empty(),
            "legacy choices move into Target without importing standalone position or saving retired sections")) return false;
        target_mob_info info; info.Load(settings_store::FromIniText("[target.mobinfo]\nenabled=0\n[mobinfo]\nenabled=1\n"));
        if (!check(!info.enabled && info.Rows(mob).empty(), "explicit Target setting wins over retired standalone preferences")) return false;
        target_mob_info compact;
        compact.Load(settings_store::FromIniText("[text.target.mobinfo]\noffset_x=15\noffset_y=-56\n[target.mobinfo.mobbehavior]\ncell_spacing=12\n"));
        if (!check(compact.text.offset_x == 0 && compact.text.offset_y == 0 && compact.groups[0].cell_spacing == 2,
            "legacy header offsets and default icon spacing migrate narrowly")) return false;
        auto compactSaved = settings_store::FromIniText(""); compact.Save(compactSaved);
        compactSaved.WriteFloat("target.mobinfo.mobbehavior", "cell_spacing", 12);
        compactSaved.WriteFloat("text.target.mobinfo", "offset_y", -7); compact.Load(compactSaved);
        if (!check(compact.groups[0].cell_spacing == 12 && compact.text.offset_y == -7,
            "later header spacing and offsets survive the migration")) return false;
        info.enabled = true;
        if (!check(info.Rows(nullptr).empty(), "unknown targets add no information or invented fields")) return false;
        for (size_t i = 0; i < info.groups.size(); ++i) info.groups[i].state.enabled = i >= 3 && i <= 7;
        const auto rows = info.Rows(&sample);
        if (!check(rows.size() == 5 && rows[0].values.size() == 4 && rows[0].values[2].icon == mob_info_icon::physical &&
            rows[0].values[2].id == 2 && rows[0].values[3].id == 3 && rows[1].values.size() == 8 &&
            rows[1].values[4].icon == mob_info_icon::element && rows[1].values[4].id == 4,
            "damage cells retain physical and element ordering with their numeric multipliers")) return false;
        if (!check(rows[3].values[0].id == 4 && rows[3].values[2].id == 6 && rows[3].values[5].id == 2 &&
            rows[3].values[5].element == 6 && rows[3].values[6].id == 2 && rows[3].values[6].element == 7 &&
            rows[4].values.size() == 1 && rows[4].values[0].element == 6,
            "native status icons retain distinct light/dark sleep badges and independent immunity")) return false;
        for (const auto& row : rows)
            if (!check(row.label.find("Snapshot") == std::string::npos && row.label.find("HeadsUp") == std::string::npos,
                "mob information has no snapshot or provenance footer")) return false;
        target_mob_info behavior; behavior.enabled = true;
        mob_record senses{}; senses.flags = 32; senses.detects = 0x101;
        const auto cells = [&](const mob_record& record) {
            mob_info_row all;
            for (const auto& row : behavior.Rows(&record)) all.values.insert(all.values.end(), row.values.begin(), row.values.end());
            return std::vector<mob_info_row>{all};
        };
        auto icons = cells(senses);
        using detection = ui::mob_detection_icon;
        if (!check(icons.size() == 1 && icons[0].label.empty() && icons[0].values.size() == 4 &&
            icons[0].values[0].id == static_cast<uint16_t>(detection::passive) &&
            icons[0].values[1].id == static_cast<uint16_t>(detection::links) &&
            icons[0].values[2].id == static_cast<uint16_t>(detection::sight) &&
            icons[0].values[3].id == static_cast<uint16_t>(detection::scent),
            "passive, linking, sight and scent become icon cells without a Detects heading")) return false;
        senses.flags = 1 | 8 | 32 | 64; senses.detects = 0x167;
        icons = cells(senses);
        if (!check(icons[0].values.size() == 8 && icons[0].values[0].id == static_cast<uint16_t>(detection::aggressive_nm) &&
            icons[0].values[2].id == static_cast<uint16_t>(detection::true_sight) && icons[0].values[3].label == "True sound" &&
            icons[0].values[5].id == static_cast<uint16_t>(detection::magic) &&
            icons[0].values[6].id == static_cast<uint16_t>(detection::ability) &&
            icons[0].values[7].id == static_cast<uint16_t>(detection::low_hp),
            "NM and true-detection variants preserve each recorded sense")) return false;
        senses.flags |= 4;
        if (!check(cells(senses)[0].values[0].id == static_cast<uint16_t>(detection::passive_nm),
            "NoAggro retains the passive NM classification")) return false;
        target_module::target_view view{};
        view.id = mob->id; view.name = "Snipper"; view.hp = 75; view.health = true;
        view.kind = entity_class::unclaimed_mob; view.mob = mob;
        ui::design_system design; module_render_context context{design}; meter_animation_state animation;
        behavior.text.offset_x = behavior.text.offset_y = 0;
        const auto header = behavior.DrawHeaderIcons(&senses, {0, 100, 400, 0}, context);
        if (!check(header.width > 0 && header.x + header.width == 400 && header.y + header.height == 100 &&
            behavior.DrawHeaderIcons(nullptr, {0, 100, 400, 0}, context).width == 0,
            "available header icons anchor to the right above their caption; unknown records occupy no space")) return false;
        target.settings_.mob_info.enabled = false;
        const auto normal = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        target.settings_.mob_info.enabled = true;
        const auto expanded = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        const auto infoBefore = target.settings_.mob_info.groups[0].attached_bounds;
        view.status_count = 1; view.statuses[0] = {4, 58, 1000, 0, false};
        target.BeginRenderFrame();
        target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        const auto statusBefore = target.status_hits_.front().bounds;
        view.status_count = 24; view.cast = {"Stone", 1000, 0, 4};
        target.BeginRenderFrame();
        target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        const auto infoAfter = target.settings_.mob_info.groups[0].attached_bounds;
        const auto statusAfter = target.status_hits_.front().bounds;
        if (!check(infoBefore.x == infoAfter.x && infoBefore.y == infoAfter.y && statusBefore.x == statusAfter.x && statusBefore.y == statusAfter.y,
            "Paralysis, wrapped status rows and casting leave mob information and the first status anchor stationary")) return false;
        view.status_count = 0; view.cast = {};
        view.mob = nullptr;
        const auto unmatched = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        if (!check(expanded > normal && unmatched == normal, "information rows extend the Target footprint only for known monsters")) return false;
        auto& layout = target.settings_.mob_info;
        layout.selected_group = 1;
        auto& group = layout.groups[0];
        group.placement = mob_info_group_settings::independent;
        layout.OnOptionChanged(4, target.State().bounds);
        target.views_[0] = view; target.views_[0].mob = mob;
        target_mob_info_module detached{target, 0};
        if (!check(group.positioned && detached.HasEditableBounds() && detached.ShouldRender({}), "independent group enters shared movement only with available data")) return false;
        group.state.bounds.x = 333; group.state.bounds.y = 444;
        group.icon_size = 25; group.text.offset_y = -7;
        const auto detachedSettings = settings_store::FromIniText(""); target.Save(detachedSettings);
        target_module restored; restored.Load(detachedSettings);
        const auto& loaded = restored.settings_.mob_info.groups[0];
        if (!check(loaded.placement == 2 && loaded.state.bounds.x == 333 && loaded.state.bounds.y == 444 && loaded.icon_size == 25 && loaded.text.offset_y == -7,
            "independent placement, sizes and offsets persist")) return false;
        group.placement = mob_info_group_settings::inline_row;
        if (!check(!detached.HasEditableBounds(), "reattaching removes the extra movement surface")) return false;
        group.placement = mob_info_group_settings::independent;
        layout.OnOptionChanged(4, target.State().bounds);
        if (!check(group.state.bounds.x == 333 && group.state.bounds.y == 444, "reattach/detach retains an already customized independent position")) return false;
        target.views_[0].mob = nullptr;
        if (!check(!detached.PreviewVisible(), "unknown targets cannot retain stale independent information")) return false;
        target.settings_.mob_info.selected_group = 0;
        size_t owners{};
        for (size_t section = 0; section < target.ConfigSections().size(); ++section)
            owners += target.ConfigSectionContains(section, target_module::mob_info_options_start);
        if (!check(owners == 1 && target.ConfigSectionContains(11, target_module::mob_info_options_start) &&
            target.ConfigSectionContains(11, target_module::display_options_start + 19),
            "information and level controls belong to Target's Mob Info topic")) return false;
        for (int selected = 0; selected <= static_cast<int>(mob_info_group_count); ++selected)
        {
            layout.selected_group = selected;
            if (!check(layout.OptionVisible(0) && !layout.OptionVisible(11) &&
                layout.OptionVisible(selected == 0 ? 1 : 3), "group tabs expose their settings without duplicate or unused alignment controls")) return false;
        }
        target.SelectConfigView(3);
        if (!check(std::string_view{target.ConfigViews()[3]} == "World Target Bar" && target.ConfigSectionContains(0, target_module::world_options_start) &&
            !target.ConfigSectionContains(0, target_module::mob_info_options_start), "world bar is a separate Target subcategory")) return false;
        auto& world = target.settings_.world_bar;
        world.enabled = true; world.width = 120; world.offset_y = 10;
        hud_snapshot snapshot{}; snapshot.target_screen_x = 500; snapshot.target_screen_y = 300;
        const auto bounds = world.Bounds(snapshot);
        if (!check(bounds.x == 440 && bounds.y == 310 && world.meter.height == 6, "world HP bar is centered below the ground anchor with a compact height")) return false;
        const auto worldSaved = settings_store::FromIniText(""); target.Save(worldSaved);
        target_module worldRestored; worldRestored.Load(worldSaved);
        if (!check(worldRestored.settings_.world_bar.enabled && worldRestored.settings_.world_bar.width == 120 &&
            worldRestored.settings_.world_bar.offset_y == 10, "world bar settings persist independently")) return false;
        D3DMATRIX identity{}, projection{};
        identity._11 = identity._22 = identity._33 = identity._44 = 1;
        projection._11 = projection._22 = projection._33 = projection._34 = 1;
        D3DVIEWPORT8 viewport{0, 0, 1000, 600, 0, 1}; float screenX{}, screenY{};
        return check(ProjectWorldPoint({0, 0, 2}, identity, projection, viewport, screenX, screenY) && screenX == 500 && screenY == 300 &&
            !ProjectWorldPoint({0, 0, -2}, identity, projection, viewport, screenX, screenY) &&
            !ProjectWorldPoint({5, 0, 2}, identity, projection, viewport, screenX, screenY), "world projection rejects behind-camera and offscreen anchors");
    }
    };

    auto RunMobInfoChecks(void) -> bool { return mob_info_test::Run(); }
}
