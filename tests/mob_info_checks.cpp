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
        if (!check(mob->damage[0] == 0 && mob->elements[1] == -3 && mob->statuses[5] == -2 && MobJobs(*mob) == "PLD",
            "packed damage adjustments, elemental/status ranks and jobs retain distinct values")) return false;
        const auto* dalham = MobById(16793928);
        const auto* peerifool = MobById(16793711);
        if (!check(dalham != nullptr && dalham->absorbs == 0x20 && dalham->damage_taken[1] == -1250 && dalham->damage[0] == 2500 &&
            MobJobs(*dalham) == "WAR" && peerifool != nullptr && peerifool->traits == 16 && MobAddEffectName(*peerifool) == std::string_view{"Sleep"} &&
            MobAddEffectName(*mob) == nullptr, "absorbs, damage taken, traits and additional effects pack by server ID")) return false;
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
        using group_kind = mob_info_group;
        const auto at = [](const group_kind group) { return static_cast<size_t>(group); };
        const size_t behaviorGroup = at(group_kind::behavior);
        target_mob_info compact;
        compact.Load(settings_store::FromIniText("[text.target.mobinfo]\noffset_x=15\noffset_y=-56\n[target.mobinfo.mobbehavior]\ncell_spacing=12\n"));
        if (!check(compact.text.offset_x == 0 && compact.text.offset_y == 0 && compact.groups[behaviorGroup].cell_spacing == 2,
            "legacy header offsets and default icon spacing migrate narrowly")) return false;
        auto compactSaved = settings_store::FromIniText(""); compact.Save(compactSaved);
        compactSaved.WriteFloat("target.mobinfo.mobbehavior", "cell_spacing", 12);
        compactSaved.WriteFloat("text.target.mobinfo", "offset_y", -7); compact.Load(compactSaved);
        if (!check(compact.groups[behaviorGroup].cell_spacing == 12 && compact.text.offset_y == -7,
            "later header spacing and offsets survive the migration")) return false;
        target_mob_info migrated;
        migrated.Load(settings_store::FromIniText("[target.mobinfo.mobphysical]\nenabled=1\n[target.mobinfo.mobelements]\nenabled=0\n"));
        target_mob_info chosen;
        chosen.Load(settings_store::FromIniText("[target.mobinfo.mobphysical]\nenabled=1\n[target.mobinfo.mobresist]\nenabled=0\n"));
        if (!check(migrated.groups[at(group_kind::weaknesses)].state.enabled && migrated.groups[at(group_kind::resistances)].state.enabled &&
            chosen.groups[at(group_kind::weaknesses)].state.enabled && !chosen.groups[at(group_kind::resistances)].state.enabled &&
            !target_mob_info{}.groups[at(group_kind::weaknesses)].state.enabled && target_mob_info{}.groups[at(group_kind::job)].state.enabled,
            "retired damage groups enable weaknesses and resistances until either is chosen")) return false;
        if (!check(!migrated.groups[at(group_kind::weaknesses)].headings && !target_mob_info{}.groups[at(group_kind::resistances)].headings &&
            migrated.groups[at(group_kind::immunities)].headings, "damage rows start as icons and amounts without headings")) return false;
        info.enabled = true;
        if (!check(info.Rows(nullptr).empty(), "unknown targets add no information or invented fields")) return false;
        const auto showOnly = [&](target_mob_info& target, const std::initializer_list<group_kind> shown) {
            for (auto& group : target.groups) group.state.enabled = false;
            for (const auto group : shown) target.groups[at(group)].state.enabled = true;
        };
        showOnly(info, {group_kind::weaknesses, group_kind::resistances, group_kind::element_ranks, group_kind::status_ranks, group_kind::immunities});
        if (!check(info.Rows(&sample).size() == 3 && info.Rows(&sample)[0].group == group_kind::element_ranks,
            "neutral damage adds no weakness or resistance rows")) return false;
        sample.damage[1] = 2500; sample.damage[2] = 2500; sample.damage[3] = -1250; sample.damage[5] = -2500;
        sample.damage[9] = 5000; sample.absorbs = 1; sample.damage_taken[1] = -5000;
        auto rows = info.Rows(&sample);
        if (!check(rows.size() == 5 && rows[0].label == "Weak:" && rows[0].values.size() == 3 &&
            rows[0].values[0].icon == mob_info_icon::element && rows[0].values[0].id == 5 && rows[0].values[0].value == "+50%" &&
            rows[0].values[1].icon == mob_info_icon::physical && rows[0].values[1].id == 1 && rows[0].values[1].value.empty() &&
            rows[0].values[2].id == 2 && rows[0].values[2].value == "+25%",
            "weaknesses list only increased damage, strongest first, sharing equal percentages")) return false;
        if (!check(rows[1].label == "Resists:" && rows[1].values.size() == 4 &&
            rows[1].values[0].icon == mob_info_icon::element && rows[1].values[0].id == 0 && rows[1].values[0].value == "Absorbs" &&
            rows[1].values[1].id == 1 && rows[1].values[1].value == "-25%" && rows[1].values[2].id == 3 && rows[1].values[2].value == "-12.5%" &&
            rows[1].values[3].icon == mob_info_icon::none && rows[1].values[3].label == "Magic" && rows[1].values[3].value == "-50%",
            "resistances lead with absorbs and end with damage kinds")) return false;
        auto& weakSettings = info.groups[at(group_kind::weaknesses)];
        weakSettings.group_percentages = false;
        if (!check(info.Rows(&sample)[0].values[1].value == "+25%", "ungrouped weaknesses show each percentage")) return false;
        weakSettings.percentages = false;
        info.groups[at(group_kind::resistances)].percentages = false;
        rows = info.Rows(&sample);
        if (!check(rows[0].values[0].value.empty() && rows[1].values[0].value == "Absorbs" && rows[1].values[1].value.empty() &&
            rows[1].values[3].value == "-50%", "hidden percentages keep absorbs and labeled damage kinds readable")) return false;
        if (!check(rows[3].values[0].id == 4 && rows[3].values[2].id == 6 && rows[3].values[5].id == 2 &&
            rows[3].values[5].element == 6 && rows[3].values[6].id == 2 && rows[3].values[6].element == 7 &&
            rows[4].values.size() == 1 && rows[4].values[0].element == 6,
            "native status icons retain distinct light/dark sleep badges and independent immunity")) return false;
        for (const auto& row : rows)
            if (!check(row.label.find("Snapshot") == std::string::npos && row.label.find("HeadsUp") == std::string::npos,
                "mob information has no snapshot or provenance footer")) return false;
        showOnly(info, {group_kind::job, group_kind::traits});
        sample.main_job = 7; sample.sub_job = 3; sample.traits = 1 | 16; sample.add_effect = 23;
        rows = info.Rows(&sample);
        if (!check(rows.size() == 2 && rows[0].group == group_kind::job && rows[0].values.size() == 1 && rows[0].values[0].label == "PLD/WHM" &&
            rows[1].label == "Traits:" && rows[1].values.size() == 3 && rows[1].values[0].label == "HP Regen" && rows[1].values[1].label == "Counter" &&
            rows[1].values[2].label == "Additional effect: Plague", "jobs, traits and named additional effects become their own cells")) return false;
        sample.traits = 0; sample.add_effect = 1; sample.main_job = 0;
        rows = info.Rows(&sample);
        if (!check(rows.size() == 1 && rows[0].values.size() == 1 && rows[0].values[0].label == "Additional effect",
            "unnamed additional effects stay generic and missing jobs add no cell")) return false;
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
        const auto infoBefore = target.settings_.mob_info.groups[behaviorGroup].attached_bounds;
        view.status_count = 1; view.statuses[0] = {4, 58, 1000, 0, false};
        target.BeginRenderFrame();
        target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        const auto statusBefore = target.status_hits_.front().bounds;
        view.status_count = 24; view.cast = {"Stone", 1000, 0, 4};
        target.BeginRenderFrame();
        target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        const auto infoAfter = target.settings_.mob_info.groups[behaviorGroup].attached_bounds;
        const auto statusAfter = target.status_hits_.front().bounds;
        if (!check(infoBefore.x == infoAfter.x && infoBefore.y == infoAfter.y && statusBefore.x == statusAfter.x && statusBefore.y == statusAfter.y,
            "Paralysis, wrapped status rows and casting leave mob information and the first status anchor stationary")) return false;
        view.status_count = 0; view.cast = {};
        view.mob = nullptr;
        const auto unmatched = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
        if (!check(expanded > normal && unmatched == normal, "information rows extend the Target footprint only for known monsters")) return false;
        auto& layout = target.settings_.mob_info;
        layout.selected_group = static_cast<int>(behaviorGroup + 1);
        auto& group = layout.groups[behaviorGroup];
        group.placement = mob_info_group_settings::independent;
        layout.OnOptionChanged(4, target.State().bounds);
        target.views_[0] = view; target.views_[0].mob = mob;
        target_mob_info_module detached{target, behaviorGroup};
        if (!check(group.positioned && detached.HasEditableBounds() && detached.ShouldRender({}), "independent group enters shared movement only with available data")) return false;
        group.state.bounds.x = 333; group.state.bounds.y = 444;
        group.icon_size = 25; group.text.offset_y = -7;
        const auto detachedSettings = settings_store::FromIniText(""); target.Save(detachedSettings);
        target_module restored; restored.Load(detachedSettings);
        const auto& loaded = restored.settings_.mob_info.groups[behaviorGroup];
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
            const bool damage = selected == static_cast<int>(at(group_kind::weaknesses) + 1) || selected == static_cast<int>(at(group_kind::resistances) + 1);
            if (!check(layout.OptionVisible(3 + target_mob_info::percent_options) == damage &&
                layout.OptionVisible(3 + target_mob_info::percent_options + 1) == damage, "percentage controls appear only for damage groups")) return false;
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
