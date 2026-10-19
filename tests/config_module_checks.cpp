#include "config_input_fixture.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::TreasureInput(void) -> bool
    {
        hud_runtime runtime;
        runtime.visible_ = true;
        runtime.editing_ = false;
        runtime.treasure_.Open();
        if (!check(runtime.TreasureInputAvailable(), "a visible forced pool can be closed even when empty")) return false;
        runtime.visible_ = false;
        if (!check(!runtime.TreasureInputAvailable(), "hidden XIUI suspends treasure input")) return false;
        runtime.visible_ = true;
        runtime.editing_ = true;
        if (!check(!runtime.TreasureInputAvailable(), "edit previews cannot take treasure actions")) return false;
        runtime.editing_ = false;
        runtime.native_ui_.menu_visible = true;
        if (!check(!runtime.TreasureInputAvailable(), "native menus retain treasure input ownership")) return false;
        runtime.native_ui_.menu_visible = false;
        for (auto* chat : {&runtime.chat_, &runtime.chat_second_})
        {
            chat->State().enabled = true;
            if (!check(chat->BeginControllerHistory(), "chat history can be focused for input ownership")) return false;
            chat->HandleControllerAction(controller_action::confirm, false, runtime.design_system_);
            if (!check(chat->HistoryExpanded() && !runtime.TreasureInputAvailable(), "either expanded chat window owns input ahead of treasure")) return false;
            chat->Focus(false);
        }
        if (!check(runtime.TreasureInputAvailable(), "closing expanded chat restores treasure availability")) return false;
        runtime.treasure_.HandleMouse(WM_LBUTTONDOWN, 1, 1);
        runtime.native_ui_.menu_visible = true;
        if (!check(runtime.HandleTreasureMouse(WM_LBUTTONDOWN, 1, 1, true) && runtime.treasure_.PointerDown(), "blocked treasure gestures retain only release ownership")) return false;
        if (!check(runtime.HandleTreasureMouse(WM_LBUTTONUP, 1, 1, true) && !runtime.treasure_.PointerDown(), "blocked treasure gestures drain their release")) return false;
        runtime.native_ui_.menu_visible = false;
        runtime.treasure_key_release_[VK_ESCAPE] = true;
        if (!check(runtime.HandleTreasureKey(VK_ESCAPE, 0x40000000u, false), "held Cancel stays consumed after leaving treasure focus")) return false;
        if (!check(runtime.HandleTreasureKey(VK_ESCAPE, 0xC0000000u, false) && !runtime.treasure_key_release_[VK_ESCAPE], "the owned Cancel release drains once")) return false;
        if (!check(!runtime.HandleTreasureKey(VK_ESCAPE, 0, false), "a fresh unfocused Cancel returns to gameplay")) return false;
        runtime.treasure_.State().enabled = false;
        return check(!runtime.TreasureInputAvailable(), "disabled treasure cannot capture input");
    }

    auto config_input_test::PartyNameCast(void) -> bool
    {
        party_module party;
        party.ApplyPreset(0);
        party.State().bounds.width = 360;
        party.job_icons_ = true; party.icon_size_ = 20; party.icon_follow_name_ = false;
        party.cast_placement_ = party_module::cast_name;
        party.text_[0].offset_x = 44; party.text_[0].offset_y = 6;
        party.text_[party_module::tp_text].offset_x = 37;
        const auto separateIcon = party.LayoutContainer();
        party.icon_follow_name_ = true;
        const auto sharedIcon = party.LayoutContainer();
        if (!check(separateIcon.width == sharedIcon.width && sharedIcon.canvas_width == separateIcon.canvas_width,
            "job icon anchoring does not allocate horizontal entry space")) return false;
        const ui::rect row{0, 0, sharedIcon.canvas_width, 0};
        const auto entry = party.LayoutEntry(row, true, {true, true, true, true});
        ui::design_system design;
        module_render_context context{design, nullptr, true};
        for (int font = 0; font < ui::font_count; ++font)
        {
            auto& profile = party.text_[party_module::tp_text];
            profile.style.font = font; profile.style.scale = 0.867f;
            for (const int placement : {0, 1, 2, 3})
            {
                profile.placement = placement;
                const auto number = party.TPNumberBounds({0, 0, 28, 20});
                const auto visible = context.TextBounds("3000", number, number, profile);
                if (!check(visible.width == design.MeasureText("3000", profile.style),
                    "default/left/center/right TP placement retains all four digits after text padding")) return false;
            }
        }
        party.text_[party_module::tp_text].style.font = 0;
        party.text_[party_module::tp_text].placement = 0;
        for (const int placement : {0, 1, 2, 3})
        {
            party.text_[0].placement = placement;
            const auto name = context.TextBounds("White Mage", entry.header, entry.header, party.text_[0]);
            const auto area = party.NameCastArea(entry.header, name.x);
            const auto cast = party.CastBounds(area);
            if (!check(cast.x == name.x && cast.y == entry.header.y + 6 && cast.width >= 0,
                "replacement casting starts at the actual name anchor, including offsets and alignment")) return false;
        }
        party.text_[0].placement = 0;
        const auto name = context.TextBounds("White Mage", entry.header, entry.header, party.text_[0]);
        if (!check(party.JobIconBounds(entry, name, 0).x >= row.x &&
            party.LayoutEntry(row, false, {}).zone.x >= row.x && entry.resources.bars[0].width == 152 &&
            entry.resources.bars[1].width == 92 && entry.resources.bars[2].width == 28,
            "fitted icons, out-of-zone panels and unchanged HP/MP/TP widths stay inside the compact row")) return false;
        party.bar_offsets_x_[3] = 9; party.bar_offsets_y_[3] = 3;
        const auto custom = party.CastBounds(party.NameCastArea(entry.header, name.x));
        if (!check(custom.x == name.x + 9 && custom.y == entry.header.y + 9,
            "independent cast offsets remain relative to the name anchor")) return false;
        auto legacy = settings_store::FromIniText("");
        party.ApplyPreset(1); party.bar_offsets_x_[3] = 4; party.bar_offsets_y_[3] = 8;
        party.Save(legacy); legacy.WriteInt("party", "name_cast_anchor_version", 0);
        party_module migrated; migrated.Load(legacy);
        if (!check(migrated.text_[0].offset_x == 4 && migrated.text_[0].offset_y == 8 &&
            migrated.bar_offsets_x_[3] == 0 && migrated.bar_offsets_y_[3] == 0,
            "old XIUI preset offsets migrate once without applying the name offset twice")) return false;
        migrated.Save(legacy); legacy.WriteFloat("meter.party.cast", "offset_x", 9);
        party_module customized; customized.Load(legacy);
        if (!check(customized.bar_offsets_x_[3] == 9, "later custom cast offsets survive reload")) return false;
        for (const bool follow : {false, true})
        {
            party.icon_follow_name_ = follow;
            party.icon_offset_x_ = party.icon_offset_y_ = 0;
            const auto before = party.LayoutEntry(row, true, {true, true, true, true, true});
            const auto icon = party.JobIconBounds(before, name, 0);
            party.icon_offset_x_ = 12; party.icon_offset_y_ = -6;
            const auto after = party.LayoutEntry(row, true, {true, true, true, true, true});
            const auto moved = party.JobIconBounds(after, name, 0);
            if (!check(moved.x == icon.x + 12 && moved.y == icon.y - 6 && before.height == after.height &&
                before.header.y == after.header.y && before.resources.bars[0].y == after.resources.bars[0].y,
                "fixed and name-following icon offsets move only the icon, preserving entry height and bars")) return false;
        }
        for (const int placement : {party_module::cast_separate, party_module::cast_name, party_module::cast_mp, party_module::cast_tp})
        {
            party.cast_placement_ = placement; party.text_[5].offset_y = 0;
            const auto before = party.LayoutEntry(row, true, {true, true, true, true, true});
            const auto label = context.TextBounds("Cure IV", before.header, before.header, party.text_[5]);
            for (const float offset : {-6.0f, 12.0f})
            {
                party.text_[5].offset_y = offset;
                const auto after = party.LayoutEntry(row, true, {true, true, true, true, true});
                const auto moved = context.TextBounds("Cure IV", after.header, after.header, party.text_[5]);
                if (!check(before.height == after.height && before.header.y == after.header.y &&
                    before.resources.bars[0].y == after.resources.bars[0].y && moved.y == label.y + offset,
                    "cast text offsets move its label without changing entry height or resource positions in any cast mode")) return false;
            }
        }
        auto offsets = settings_store::FromIniText("");
        party.Save(offsets);
        party_module alliance{1}; alliance.icon_offset_x_ = -9; alliance.icon_offset_y_ = 4; alliance.Save(offsets);
        party_module restoredParty, restoredAlliance{1}; restoredParty.Load(offsets); restoredAlliance.Load(offsets);
        if (!check(restoredParty.icon_offset_x_ == 12 && restoredParty.icon_offset_y_ == -6 &&
            restoredAlliance.icon_offset_x_ == -9 && restoredAlliance.icon_offset_y_ == 4 &&
            restoredParty.text_[5].offset_y == 12 &&
            party.ConfigSectionContains(party_module::jobs_section, party_module::icon_offset_options_start) &&
            party.ConfigSectionContains(party_module::jobs_section, party_module::icon_offset_options_start + 1),
            "Party and alliance icon offsets persist independently in Jobs, along with cast text offsets")) return false;
        party.ApplyPreset(0);
        if (!check(party.icon_offset_x_ == 9 && party.icon_offset_y_ == 1, "Classic restores captured icon offsets")) return false;
        ui::player_timing_settings timing;
        if (!check(timing.attack_layout == 2 && timing.recovery_layout == 2 && timing.mini_gap == 2 && timing.recovery_mini_gap == 2,
            "Player tickers default to Mini bar with two-pixel gaps")) return false;
        timing.Load(settings_store::FromIniText(""));
        if (!check(timing.attack_layout == 2 && timing.recovery_layout == 2 && timing.mini_gap == 2 && timing.recovery_mini_gap == 2,
            "missing timer settings use the same Mini bar defaults")) return false;
        std::puts("Party name/cast alignment, visual-only icon/cast offsets, independent persistence, Classic gutter and Player mini-ticker defaults passed.");
        return true;
    }

    auto config_input_test::PartyEntries(void) -> bool
    {
        ui::design_system design;
        module_render_context context{design};
        party_module jobLayout;
        for (const bool fit : {false,true}) for (const int layout : {0,1,2})
        {
            jobLayout.fit_width_=fit; jobLayout.layout_=layout;
            jobLayout.bar_widths_={140,92,32,0}; jobLayout.job_icons_=false;
            const auto plain=jobLayout.LayoutContainer();
            const auto plainEntry=jobLayout.LayoutEntry({20,0,plain.canvas_width,0},true,{true,true,true,true});
            jobLayout.job_icons_=true; jobLayout.icon_size_=64;
            jobLayout.icon_offset_x_=-90;
            for (const bool follow : {false,true})
            {
                jobLayout.icon_follow_name_=follow;
                const auto icons=jobLayout.LayoutContainer();
                const auto iconEntry=jobLayout.LayoutEntry({20,0,icons.canvas_width,0},true,{true,true,true,true});
                if (!check(icons.width==plain.width && icons.canvas_width==plain.canvas_width && icons.inset==plain.inset &&
                    iconEntry.header.x==plainEntry.header.x && iconEntry.header.width==plainEntry.header.width &&
                    iconEntry.resources.bars[0].x==plainEntry.resources.bars[0].x && iconEntry.resources.bars[0].width==plainEntry.resources.bars[0].width,
                    "job icon visibility, size, anchor and offsets cannot widen entries or push their content right")) return false;
            }
        }
        jobLayout=party_module{};
        party_member jobMember{};
        jobMember.main_job=3; jobMember.main_level=99;
        jobMember.sub_job=5; jobMember.sub_level=49; jobMember.master_level=20;
        if (!check(jobLayout.JobLines(jobMember)[0]=="WHM99/RDM49 ML20" && jobLayout.JobLines(jobMember)[1].empty(),
            "existing inline job formatting is retained by default")) return false;
        const auto inlineJobEntry=jobLayout.LayoutEntry({0,0,300,0},true,{});
        jobLayout.stack_jobs_=true;
        const auto stackedJobs=jobLayout.JobLines(jobMember);
        if (!check(stackedJobs[0]=="WHM99 ML20" && stackedJobs[1]=="RDM49" &&
            jobLayout.LayoutEntry({0,0,300,0},true,{}).height>inlineJobEntry.height,
            "stacked job rows keep master level with main job and reserve both text lines")) return false;
        jobLayout.sub_job_=false;
        if (!check(jobLayout.JobLines(jobMember)[1].empty() && jobLayout.JobTextHeight()==jobLayout.LineHeight(jobLayout.text_[1]),
            "hiding subjob removes the second line and its reserved height")) return false;
        jobLayout.sub_job_=true;
        jobLayout.job_anchor_=party_module::job_anchor_hp;
        jobLayout.bar_offsets_x_[0]=26; jobLayout.bar_offsets_y_[0]=7;
        const auto hpEntry=jobLayout.LayoutEntry({0,0,300,0},true,{});
        const auto hpJobs=jobLayout.HPJobTextBounds(hpEntry.resources.bars[0]);
        const auto track=ui::LayoutMeterTrack(hpEntry.resources.bars[0],jobLayout.meters_[0]);
        jobLayout.text_[0].offset_x+=33;
        const auto movedName=jobLayout.LayoutEntry({0,0,300,0},true,{});
        const auto stillHP=jobLayout.HPJobTextBounds(movedName.resources.bars[0]);
        if (!check(hpJobs.x==track.x && hpJobs.y==track.y && stillHP.x==hpJobs.x && stillHP.y==hpJobs.y && !jobLayout.Option(49).visible,
            "HP-anchored job text follows the actual shifted HP fill independently of name positioning")) return false;
        auto jobStore=settings_store::FromIniText(""); jobLayout.Save(jobStore);
        party_module savedJobs; savedJobs.Load(jobStore);
        savedJobs.selected_text_=1;
        if (!check(savedJobs.stack_jobs_ && savedJobs.job_anchor_==party_module::job_anchor_hp &&
            savedJobs.ConfigSectionContains(party_module::text_section,party_module::job_text_options_start) &&
            savedJobs.ConfigSectionContains(party_module::text_section,party_module::job_text_options_start+1),
            "job stack and anchor preferences persist and appear under Text > Jobs")) return false;
        for (const size_t option : std::array<size_t,8>{7,8,9,10,11,49,party_module::job_text_options_start,party_module::job_text_options_start+1})
        {
            size_t appearances{};
            for (size_t section=0;section<savedJobs.ConfigSections().size();++section)
                appearances+=savedJobs.ConfigSectionContains(section,option);
            if (!check(appearances==1 && !savedJobs.ConfigSectionContains(party_module::jobs_section,option),
                "job text display and positioning controls appear once alongside their text styles")) return false;
            savedJobs.selected_text_=0;
            if (!check(!savedJobs.ConfigSectionContains(party_module::text_section,option),
                "job controls stay out of other text elements")) return false;
            savedJobs.selected_text_=1;
        }
        if (!check(std::string_view{savedJobs.ConfigSections()[party_module::jobs_section].name}=="Job Icons" &&
            savedJobs.ConfigSectionContains(party_module::jobs_section,6) &&
            savedJobs.ConfigSectionContains(party_module::jobs_section,party_module::icon_offset_options_start) &&
            std::string_view{TextProfileOption(0,savedJobs.text_[1]).label}=="Alignment",
            "icon settings have an explicit home and text alignment is distinct from placement")) return false;
        party_module captions;
        captions.text_[0].offset_x = 44; captions.text_[0].offset_y = 8;
        const ui::rect header{20, 50, 220, 24};
        for (const int placement : {0, 1, 2, 3})
        {
            captions.text_[0].placement = placement;
            const auto name = context.TextBounds("Shuu", header, header, captions.text_[0]);
            const auto area = captions.JobTextBounds(header, name, header.x + header.width);
            const auto jobs = context.TextBounds("WHM99", area, area, captions.text_[1]);
            if (!check(captions.job_placement_ == party_module::jobs_name && jobs.x == name.x + name.width + 6 &&
                area.y == header.y + 8, "default job text follows the displayed name and its offsets in every name alignment")) return false;
        }
        const auto remote = captions.LayoutEntry({0, 0, 300, 0}, false, {});
        const auto zoneArea = ui::container_renderer::ContentBounds(remote.zone, captions.ZoneStyle());
        const auto zone = context.TextBounds("Ru'Lude Gardens", zoneArea, zoneArea, captions.text_[party_module::zone_text]);
        for (const int placement : {0, 1, 2, 3})
        {
            captions.text_[1].placement = placement; captions.text_[1].offset_x = 40; captions.text_[1].offset_y = -30;
            captions.text_[1].style.scale = 3;
            const auto after = captions.LayoutEntry({0, 0, 300, 0}, false, {});
            const auto content = ui::container_renderer::ContentBounds(after.zone, captions.ZoneStyle());
            const auto visible = context.TextBounds("Ru'Lude Gardens", content, content, captions.text_[party_module::zone_text]);
            if (!check(after.height == remote.height && after.zone.y == remote.zone.y && after.zone.height == remote.zone.height &&
                visible.x == zone.x && visible.y == zone.y, "Jobs placement, offsets and font size leave remote zone text and its entry unchanged")) return false;
        }
        auto legacyCaptions = settings_store::FromIniText("[text.party.jobs]\nfont=2\noffset_x=40\noffset_y=-30\nplacement=3\n");
        party_module migratedCaptions; migratedCaptions.Load(legacyCaptions);
        if (!check(migratedCaptions.text_[party_module::zone_text].style.font == 2 &&
            migratedCaptions.text_[party_module::zone_text].offset_x == 0 && migratedCaptions.text_[party_module::zone_text].offset_y == 0 &&
            migratedCaptions.text_[party_module::zone_text].placement == 0, "initial Zone styling retains legacy font effects without inheriting Jobs placement or offsets")) return false;
        migratedCaptions.text_[party_module::zone_text].offset_y = 4;
        migratedCaptions.text_[party_module::zone_text].placement = 2;
        migratedCaptions.Save(legacyCaptions);
        legacyCaptions.WriteInt("text.party.jobs", "font", 3);
        party_module restoredCaptions; restoredCaptions.Load(legacyCaptions);
        if (!check(restoredCaptions.text_[1].style.font == 3 && restoredCaptions.text_[1].placement == 3 &&
            restoredCaptions.text_[party_module::zone_text].style.font == 2 && restoredCaptions.text_[party_module::zone_text].offset_y == 4 &&
            restoredCaptions.text_[party_module::zone_text].placement == 2, "Jobs and Zone text persist independently after the initial split")) return false;
        party_module party;
        party.jobs_ = party.buffs_ = party.tp_ = party.job_icons_ = party.casts_ = party.distance_ = false;
        party.selection_ = false; party.hp_display_ = 5;
        party.text_[0].style.scale = 1;
        party.meters_[0].height = 12; party.meters_[1].height = party.meters_[2].height = 80;
        party.bar_offsets_y_[0] = -party.HeaderHeight(false, false) - 1;
        auto entry = party.LayoutEntry({0, 100, 300, 0}, true, {}, false);
        if (!check(entry.height == design.TextHeight(party.text_[0].style),
            "overlapping name and HP reserve their union, and hidden tall MP/TP do not enlarge the row")) return false;
        party.job_icons_ = party.icon_follow_name_ = true;
        party.text_[0].offset_y = 27; party.text_[0].offset_x = 18;
        for (const int placement : {0, 1, 2, 3})
        {
            party.text_[0].placement = placement;
            entry = party.LayoutEntry({20, 100, 300, 0}, true, {}, false);
            const auto name = context.TextBounds("Player", entry.header, entry.header, party.text_[0]);
            const auto icon = party.JobIconBounds(entry, name, 0);
            if (!check(icon.x + icon.width + 7 == name.x && icon.y + icon.height * 0.5f == name.y + name.height * 0.5f &&
                icon.y >= 100 && icon.y + icon.height <= 100 + entry.height && name.y + name.height <= 100 + entry.height,
                "following icons track positioned/aligned name bounds and fit the measured entry")) return false;
        }
        party.icon_follow_name_ = false;
        if (!check(party.JobIconBounds(entry, {999, 999, 30, 20}, 0).x == entry.icon.x,
            "turning follow off retains the fixed job-icon column")) return false;
        party_member member; member.in_zone = member.active = member.entity_valid = true;
        member.id = 123; member.index = 7; member.name = "Player"; member.main_job = 1; member.sync = member.distance_known = true; member.distance = 12.3f;
        party.distance_ = true;
        if (!check(party.DetailText(member) == "SYNC  12.3y", "SYNC is enabled by default")) return false;
        *party.Option(party_module::sync_option).toggle = false;
        if (!check(party.DetailText(member) == "12.3y" && party.ConfigSectionContains(party_module::general_section, party_module::sync_option) &&
            party.ConfigSectionContains(party_module::jobs_section, party_module::icon_follow_option), "SYNC hides independently of distance and both settings are reachable")) return false;
        party.casts_ = true; party.cast_placement_ = party_module::cast_separate;
        party.text_[0].offset_y = 0; party.bar_offsets_y_ = {};
        const auto idle = party.MemberHeight(member);
        member.cast = {"Cure", 1, ::GetTickCount64() - 1000, 0, 4};
        if (!check(party.MemberHeight(member) > idle, "separate casts reserve height only while visible")) return false;
        party.cast_placement_ = party_module::cast_mp;
        if (!check(party.ResourceSlots(member).mp && !party.ResourceSlots(member).mp_values, "an active cast can replace an otherwise hidden MP slot")) return false;
        member.cast = {};
        if (!check(!party.ResourceSlots(member).mp, "an unused cast replacement slot does not leave an empty row")) return false;
        party.icon_follow_name_ = true;
        auto store = settings_store::FromIniText(""); party.Save(store);
        party_module restored; restored.Load(store);
        party_module alliance{1}; alliance.Save(store);
        party_module restoredAlliance{1}; restoredAlliance.Load(store);
        return check(restored.icon_follow_name_ && !restored.show_sync_ && !restoredAlliance.icon_follow_name_ && restoredAlliance.show_sync_,
            "party and alliance preferences persist independently");
    }

    auto config_input_test::StatusSnapshots(void) -> bool
    {
        std::array<int16_t, 32> localIcons{};
        std::array<uint8_t, 32> loadingIcons{};
        std::array<uint16_t, 32> loadingOutput{};
        if (!check(!LocalStatusSnapshotReady(localIcons) && !PartyStatusSnapshotReady(loadingIcons, 0) &&
            DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 0,
            "zero-filled zoning snapshots cannot produce rows of KO icons")) return false;
        localIcons.fill(-1); localIcons[0] = 0;
        loadingIcons.fill(255); loadingIcons[0] = 0;
        if (!check(LocalStatusSnapshotReady(localIcons) && PartyStatusSnapshotReady(loadingIcons, 0) &&
            DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 1 && loadingOutput[0] == 0,
            "a genuine KO followed by empty status slots remains visible")) return false;
        localIcons.fill(255); loadingIcons.fill(255);
        if (!check(LocalStatusSnapshotReady(localIcons) && PartyStatusSnapshotReady(loadingIcons, 0) &&
            DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 0,
            "initialized empty snapshots remain authoritative")) return false;
        localIcons[0] = 32767;
        if (!check(!LocalStatusSnapshotReady(localIcons), "invalid local slot data stays unavailable")) return false;
        std::array<uint8_t, 32> icons{}; icons.fill(255);
        icons[0] = 33; icons[1] = 1; icons[2] = 255; icons[3] = 0; icons[4] = 255;
        std::array<uint16_t, 32> decoded{};
        const uint64_t mask = (1ULL << 2) | (1ULL << 4) | (3ULL << 6);
        if (!check(DecodePartyBuffs(icons, mask, decoded) == 4 && decoded[0] == 33 && decoded[1] == 257 && decoded[2] == 511 && decoded[3] == 768,
            "party status high bits preserve IDs above 255, including a low byte of 255")) return false;
        if (!check(DecodePartyBuffs({}, 0, decoded) == 0 && DecodePartyBuffs(icons, mask, std::span{decoded}.first(1)) == 1,
            "party status decoding respects both input and output bounds")) return false;
        return true;
    }

    auto config_input_test::Party(void) -> bool
    {
        if (!StatusSnapshots()) return false;
        if (!check(PartyResourceMaximum(0, 0) == 0 && PartyResourceMaximum(803, 80) == 1004 &&
            FormatPartyResource(0, 0, 0, false, 4) == "0/?" && FormatPartyResource(803, 80, 1004, false, 4) == "803/~1004" &&
            FormatPartyResource(62, 61, 101, true, 4) == "62/101" && FormatPartyResource(62, 61, 101, true, 5).empty(),
            "party displays distinguish missing, exact and rounded maximum resources")) return false;
        if (!check(PartyJobHasMP(3) && PartyJobHasMP(22) && !PartyJobHasMP(1) && !PartyJobHasMP(0) &&
            std::strcmp(PartyJobName(22), "RUN") == 0 && std::strcmp(PartyJobName(255), "") == 0,
            "zero MP does not hide caster bars or invent unknown job icons")) return false;
        party_cast cast{"Cure", 1, 1000, 0, 2};
        if (!check(cast.Active(4499) && !cast.Active(4500), "party casts expire even when a finish packet is absent")) return false;
        cast.interrupted = 1500;
        if (!check(cast.Active(2299) && !cast.Active(2300), "interrupted party casts clear their display promptly")) return false;

        party_source source;
        source.player_id_ = 100; source.zone_ = 100;
        auto& player = source.members_[0];
        player.active = player.in_zone = true; player.id = 100; player.slot = 0; player.name = "Player";
        player.hp = 80; player.hp_percent = 80; player.main_job = 1;
        party_module party, alliance{1}, third{2};
        for (auto* module : {&party, &alliance, &third}) module->SetSource(&source);
        hud_snapshot snapshot{};
        if (!check(party.ShouldRender(snapshot) && !alliance.ShouldRender(snapshot) && !third.ShouldRender(snapshot),
            "empty alliance lists remain hidden while the solo party row is visible")) return false;
        party.solo_ = false;
        if (!check(!party.ShouldRender(snapshot), "solo visibility follows its own preference")) return false;
        source.members_[7] = player; source.members_[7].id = 200; source.members_[7].slot = 7;
        if (!check(source.Count(1) == 1 && source.Count(2) == 0 && source.InAlliance() && party.ShouldRender(snapshot) && alliance.ShouldRender(snapshot),
            "alliance membership follows native slots without requiring a full six-member block")) return false;
        player.index = 101; player.entity_valid = true;
        source.members_[7].index = 102; source.members_[7].entity_valid = true;
        source.UpdateSelection(101, 102, true, -1);
        if (!check(player.selected && player.subtarget && source.members_[7].selected && !source.members_[7].subtarget,
            "entity subtarget highlighting distinguishes the cursor from the held target")) return false;
        source.members_[7].in_zone = source.members_[7].entity_valid = false;
        source.UpdateSelection(101, 0, false, 7);
        if (!check(source.members_[7].selected && source.members_[7].subtarget && player.selected && !player.subtarget,
            "party cursor selection can highlight out-of-zone members without replacing the held target")) return false;
        source.cutscene_ = true;
        if (!check(!party.ShouldRender(snapshot) && !alliance.ShouldRender(snapshot), "event hiding applies to each list")) return false;
        source.cutscene_ = false;
        party.preview_ = false; party.bottom_ = true;
        ui::design_system design;
        module_render_context context{design};
        party.Render(snapshot, context);
        const float bottom = party.state_.bounds.y + party.state_.bounds.height;
        source.members_[3] = player; source.members_[3].id = 300; source.members_[3].slot = 3;
        party.Render(snapshot, context);
        if (!check(bottom == party.state_.bounds.y + party.state_.bounds.height, "bottom alignment keeps a fixed anchor as membership grows")) return false;
        party.ResetDefaults(design.GetTheme());
        if (!check(party.source_ == &source && party.text_[0].style.shadow.enabled == design.GetTheme().title_text.shadow.enabled &&
            party.meters_[0].appearance == ui::meter_style::window_theme, "reset preserves the live source and shared theme effects")) return false;
        if (!check(party.job_placement_ == party_module::jobs_name && party.Option(49).choice_count == 2,
            "job/subjob defaults to the name row with a separate-row option")) return false;
        if (!check(!party.labels_ && party.Option(51).choice_count == 2 && party.Option(51).available && party.Option(51).visible,
            "party resource labels default off and TP offers a numbers-only mode")) return false;
        party.tp_display_ = 1; player.tp = 1500;
        party.Render(snapshot, context);
        party.tp_ = false;
        if (!check(!party.Option(51).visible, "hiding TP also hides its display selector")) return false;
        party.tp_ = true; party.tp_display_ = 0;
        const float inlineJobs = party.EntryHeight();
        party.job_placement_ = party_module::jobs_below;
        if (!check(party.EntryHeight() > inlineJobs, "a separate job row reserves its own text height")) return false;
        party.job_placement_ = party_module::jobs_name;
        player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4};
        for (const int placement : {party_module::cast_separate, party_module::cast_name, party_module::cast_mp, party_module::cast_tp})
        {
            party.cast_placement_ = placement;
            const float height = party.MemberHeight(player);
            party.Render(snapshot, context);
            player.cast = {};
            party.Render(snapshot, context);
            if (!check(party.MemberHeight(player) <= height, "finished casts release their visible space without enlarging the entry")) return false;
            player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4};
        }
        player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4}; player.main_job = 1; player.sub_job = 13;
        party.tp_ = false; party.cast_placement_ = party_module::cast_mp;
        if (!check(party.ResourceSlots(player).mp && !party.ResourceSlots(player).mp_values,
            "active MP replacement supplies a cast slot even for jobs without MP")) return false;
        party.cast_placement_ = party_module::cast_tp;
        if (!check(party.ResourceSlots(player).tp && !party.ResourceSlots(player).tp_values,
            "active TP replacement remains available when idle TP is hidden")) return false;
        party.meters_[3].height = 30;
        if (!check(party.ResourceBarHeight(true, true, true) >= party.CastHeight(), "active replacement rows fit their cast style")) return false;
        party.cast_placement_ = party_module::cast_name;
        if (!check(party.HeaderHeight(true) >= party.CastHeight(), "active name replacement fits its cast style")) return false;
        party.casts_ = false;
        if (!check(!party.Option(48).visible, "cast placement is hidden when casts are hidden")) return false;
        party.ResetDefaults(design.GetTheme());
        party.meters_[0].height = 1; party.meters_[1].height = 8;
        const ui::rect resourceRow{0, 0, 200, party.ResourceBarHeight()};
        if (!check(ui::LayoutMeterTrack(resourceRow, party.meters_[0]).height == 1 &&
            ui::LayoutMeterTrack(resourceRow, party.meters_[1]).height == 8,
            "party resource bars keep independent pixel heights within a shared row")) return false;
        party.ResetDefaults(design.GetTheme());
        party.layout_ = party_module::bars_mp_below_tp_left;
        const auto stacked = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(party.Option(21).choice_count == 3 && stacked.bars[1].x == stacked.bars[0].x &&
            stacked.bars[1].width == stacked.bars[0].width && stacked.bars[1].y > stacked.bars[0].y + stacked.bars[0].height &&
            stacked.bars[2].x + stacked.bars[2].width < stacked.bars[0].x &&
            stacked.bars[2].y + stacked.bars[2].height * 0.5f == 20 + stacked.height * 0.5f,
            "MP stacks directly beneath HP while TP occupies a centered column to their left")) return false;
        const auto noMP = party.LayoutResources({10, 20, 360, 0}, {false, true});
        const auto noTP = party.LayoutResources({10, 20, 360, 0}, {true, false});
        if (!check(noMP.height == party.ResourceBarHeight() && noMP.bars[1].height == 0 && noTP.bars[0].x == 10 &&
            noTP.bars[0].width == 360 && noTP.bars[2].height == 0,
            "missing MP collapses its row and hidden TP gives the stacked bars their full width")) return false;
        party.tp_display_ = 1;
        const auto numberTP = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(numberTP.bars[2].x == stacked.bars[2].x && numberTP.height == stacked.height,
            "numbers-only TP retains its left column without changing the MP/HP stack")) return false;
        party.cast_placement_ = party_module::cast_mp;
        const auto replacement = party.LayoutResources({10, 20, 360, 0}, party.ResourceSlots(player));
        if (!check(replacement.bars[1].height > 0,
            "cast replacement can reserve the lower MP row for a job without MP")) return false;
        party.Render(snapshot, context);
        party.ResetDefaults(design.GetTheme());
        party.style_mode_ = 1; party.selected_bar_ = 0;
        *party.Option(52).slider = 120; party.OnOptionChanged(52);
        party.selected_bar_ = 1;
        *party.Option(52).slider = 90; party.OnOptionChanged(52);
        const auto custom = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(custom.bars[0].width == 120 && custom.bars[1].width == 90 && custom.bars[2].width == 130 &&
            custom.bars[2].x + custom.bars[2].width == 370 && party.bar_widths_[2] == 0,
            "per-bar pixel widths retain automatic neighbors and fit the complete row")) return false;
        party.bar_widths_ = {800, 800, 0, 140};
        const auto constrained = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(constrained.bars[0].width > 0 && constrained.bars[1].width > 0 && constrained.bars[2].width >= 24 &&
            constrained.bars[2].x + constrained.bars[2].width <= 370.01f && party.BarWidth(3, 90) == 90 && party.BarWidth(3, 200) == 140,
            "oversized resource and cast widths remain within their available row or replacement slot")) return false;
        party.layout_ = party_module::bars_mp_below_tp_left; party.bar_widths_ = {200, 150, 60, 140};
        const auto customStack = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(customStack.bars[0].width == 200 && customStack.bars[1].width == 150 && customStack.bars[2].width == 60 &&
            customStack.bars[0].x == customStack.bars[1].x && customStack.bars[2].x + customStack.bars[2].width < customStack.bars[0].x,
            "stacked HP, MP and left TP retain independent widths and column clearance")) return false;
        party.layout_ = party_module::bars_vertical;
        const auto customVertical = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(customVertical.bars[0].width == 200 && customVertical.bars[1].width == 150 && customVertical.bars[2].width == 60,
            "compact vertical layout honors each resource width")) return false;
        party.state_.maximum_width = 2560;
        if (!check(party.Option(52).maximum == party.BarWidthLimit() && party.Option(52).maximum < party.state_.bounds.width,
            "per-bar width is limited to party content rather than the viewport width")) return false;
        party.layout_ = party_module::bars_mp_below_tp_left; party.selected_bar_ = 1;
        *party.Option(54).slider = -7; party.OnOptionChanged(54);
        *party.Option(55).slider = 20; party.OnOptionChanged(55);
        for (const int position : {party_module::position_left, party_module::position_center, party_module::position_right})
        {
            *party.Option(53).choice = position; party.OnOptionChanged(53);
            const auto positioned = party.LayoutResources({10, 20, 360, 0}, {true, true});
            const float factor = position == party_module::position_right ? 1.0f : position == party_module::position_center ? 0.5f : 0;
            if (!check(positioned.bars[1].x == 10 + (360 - positioned.bars[1].width) * factor - 7 &&
                positioned.bars[1].y == customStack.bars[1].y + 20 && positioned.bars[0].x == customStack.bars[0].x &&
                positioned.height >= positioned.bars[1].y + positioned.bars[1].height - 20,
                "MP anchors and offsets move only its bar while row bounds include the moved content")) return false;
        }
        party.bar_offsets_y_[1] = -4;
        const auto lifted = party.LayoutResources({10, 20, 360, 0}, {true, true});
        if (!check(lifted.height < customStack.height, "moving MP upward retains compact spacing beneath the resource rows")) return false;
        party.bar_positions_[3] = party_module::position_right; party.bar_offsets_x_[3] = -3; party.bar_offsets_y_[3] = 6;
        const auto placedCast = party.CastBounds({10, 20, 200, 16});
        if (!check(placedCast.x == 67 && placedCast.y == 26 && placedCast.width == 140,
            "cast positioning uses its own alignment, width and offsets within the selected placement")) return false;
        party.style_mode_ = 0; party.selected_bar_ = 2;
        *party.Option(52).slider = 72; party.OnOptionChanged(52);
        if (!check(std::all_of(party.bar_widths_.begin(), party.bar_widths_.end(), [](float value) { return value == 72; }),
            "shared bar settings apply a width to HP, MP, TP and Cast together")) return false;
        *party.Option(53).choice = party_module::position_center; party.OnOptionChanged(53);
        *party.Option(54).slider = 4; party.OnOptionChanged(54);
        *party.Option(55).slider = 8; party.OnOptionChanged(55);
        if (!check(std::all_of(party.bar_positions_.begin(), party.bar_positions_.end(), [](int value) { return value == party_module::position_center; }) &&
            std::all_of(party.bar_offsets_x_.begin(), party.bar_offsets_x_.end(), [](float value) { return value == 4; }) &&
            std::all_of(party.bar_offsets_y_.begin(), party.bar_offsets_y_.end(), [](float value) { return value == 8; }),
            "shared bar settings apply position and both offsets consistently")) return false;
        party.ResetDefaults(design.GetTheme());
        party.job_icons_ = false; party.tp_ = false; party.cast_placement_ = party_module::cast_name;
        party.bar_widths_ = {184, 122, 0, 0}; party.layout_ = party_module::bars_vertical;
        party.bar_positions_[0] = party.bar_positions_[1] = party_module::position_right;
        party.bar_offsets_x_[0] = -23; party.bar_offsets_x_[1] = -25; party.bar_offsets_y_[1] = -17;
        const auto beforeFit = party.LayoutContainer();
        if (!check(beforeFit.width < party.state_.bounds.width && beforeFit.inset == 23,
            "fitted containers account for existing negative bar offsets without clipping their left edges")) return false;
        party.UseCompactLayout();
        const auto fitted = party.LayoutContainer();
        if (!check(fitted.width == 184 + 2 * (party.container_.edge_size + party.container_.padding + party.SelectionPadding().x) && fitted.canvas_width == 184 &&
            party.bar_widths_[1] == 122 && party.cast_placement_ == party_module::cast_name && !party.tp_ &&
            party.bar_offsets_x_[1] == 0 && party.bar_offsets_y_[1] == 0,
            "compact layout clears overlap offsets while keeping chosen bar widths, cast placement and visibility")) return false;
        party.bar_widths_[0] = 140;
        const auto smaller = party.LayoutContainer();
        if (!check(smaller.width < fitted.width && party.LayoutRows(2).bounds.width == smaller.width,
            "reducing HP width shrinks the actual native container and its layout bounds")) return false;
        party.fit_width_ = false;
        if (!check(party.LayoutContainer().width == party.state_.bounds.width,
            "fixed-width mode retains the manually configured container width")) return false;
        party.ResetDefaults(design.GetTheme());
        if (!check(party.LayoutContainer().width == party.state_.bounds.width,
            "automatic HP widths retain the existing container size without feedback shrinkage")) return false;
        player.cast = {};
        party.cast_placement_ = party_module::cast_name; party.job_icons_ = false; party.spacing_ = 0;
        const float compact = party.EntryHeight();
        const auto compactLayout = party.LayoutRows(2);
        if (!check(compact < party.EntryHeight(false) && compactLayout.heights[0] == compact && compactLayout.heights[1] == compact &&
            compactLayout.bounds.height == 2 * (party.container_.edge_size + party.container_.padding) +
                party_module::LineHeight(party.text_[party_module::heading_text]) + 6 + 2 * compact + 4 * party.SelectionPadding().y,
            "zero entry spacing retains only visible rows and their selection padding")) return false;
        player.selected = true;
        const auto selectedLayout = party.LayoutRows(2);
        player.selected = false;
        if (!check(selectedLayout.bounds.height == compactLayout.bounds.height && selectedLayout.bounds.width == compactLayout.bounds.width,
            "moving selection cannot change the space reserved for its frame")) return false;
        source.members_[3].in_zone = false;
        const auto remoteLayout = party.LayoutRows(2);
        if (!check(remoteLayout.heights[0] == compact && remoteLayout.heights[1] == party.EntryHeight(false),
            "an off-zone panel only expands its own member's entry")) return false;
        source.members_[3].in_zone = true;
        player.buff_count = 20;
        const auto statusLayout = party.LayoutRows(2);
        if (!check(statusLayout.heights[0] == party.RowHeight(true, 20) && statusLayout.heights[0] > compact &&
            statusLayout.heights[1] == compact, "wrapped visible statuses only expand their own member's row")) return false;
        party.bottom_ = true;
        const auto before = party.LayoutRows(2);
        player.buff_count = 0;
        const auto after = party.LayoutRows(2);
        if (!check(before.bounds.y + before.bounds.height == after.bounds.y + after.bounds.height,
            "bottom alignment remains fixed as a visible status container grows or shrinks")) return false;
        party.expand_ = true;
        const auto expanded = party.LayoutRows(2);
        if (!check(expanded.rows == 6 && expanded.leading_space == 4 * (compact + 2 * party.SelectionPadding().y),
            "expanded bottom-aligned parties reserve compact empty slots before their members")) return false;
        party.preview_ = true; party.bottom_ = false; party.expand_ = false;
        const auto previewLayout = party.LayoutRows(6, true);
        const module_render_context previewContext{design, nullptr, true};
        party.Render(snapshot, previewContext);
        if (!check(party.state_.bounds.height == previewLayout.bounds.height &&
            party.VisualBounds(true).height == previewLayout.bounds.height + 16,
            "preview drawing, edit bounds and variable row geometry agree")) return false;
        party.ResetDefaults(design.GetTheme());
        const float oneStatusRow = party.RowHeight(true, 32);
        party.buff_limit_ = 32;
        if (!check(party.RowHeight(true, 32) > oneStatusRow && party.BuffColumns(0) == 1,
            "all 32 statuses can wrap within the party window without overlapping another member")) return false;
        party.buff_limit_ = 12;
        const ui::rect frame{500, 50, 420, 600}, row{513, 85, 394, party.RowHeight(true, 20)};
        const auto left = party.StatusLayout(row, frame, 20);
        const auto first = party.StatusIconBounds(left, 0), wrapped = party.StatusIconBounds(left, 8);
        if (!check(left.framed && left.reverse && left.count == 12 && left.overflow == 8 && left.columns == 8 &&
            left.bounds.x + left.bounds.width + party.status_gap_ == frame.x && left.bounds.y == row.y &&
            first.x == wrapped.x && wrapped.y == first.y + party.buff_size_ + party.status_spacing_ &&
            left.bounds.height <= row.height,
            "separate status windows default left, grow away from the member and wrap inside their reserved row")) return false;
        party.status_placement_ = party_module::status_right;
        const auto right = party.StatusLayout(row, frame, 20);
        if (!check(right.framed && !right.reverse && right.bounds.x == frame.x + frame.width + party.status_gap_ &&
            party.StatusIconBounds(right, 1).x > party.StatusIconBounds(right, 0).x && right.bounds.width == left.bounds.width,
            "right-side statuses retain the same dimensions and flow away from the member")) return false;
        party.status_frame_ = false;
        const auto bare = party.StatusLayout(row, frame, 1);
        if (!check(!bare.framed && bare.bounds.width == party.buff_size_ && bare.content.x == bare.bounds.x &&
            !party.Option(46).visible, "frameless effects retain side placement and hide window-only controls")) return false;
        party.status_placement_ = party_module::status_inline;
        const auto inside = party.StatusLayout(row, frame, 20);
        if (!check(!inside.framed && inside.bounds.x == row.x && party.RowHeight(true, 20) > party.EntryHeight() &&
            !party.Option(42).visible && !party.Option(45).visible && party.StatusLayout(row, frame, 0).bounds.width == 0,
            "inline mode reserves its own space while empty effects never draw a container")) return false;
        party.status_placement_ = party_module::status_left;
        party.status_frame_ = true;
        party.buff_limit_ = 32; party.status_columns_ = 1; party.buff_size_ = 32;
        const auto tall = party.StatusLayout(row, frame, 32);
        if (!check(party.RowHeight(true, 32) >= tall.bounds.height, "large status columns cannot overlap the next party entry")) return false;
        party.ResetDefaults(design.GetTheme());
        party.state_.bounds = {0, 0, 360, 100};
        const auto reserve = party.VisualBounds(false);
        const auto maximum = party.StatusLayout({}, party.PartyBounds(2), 32);
        if (!check(reserve.x <= maximum.bounds.x && reserve.width > party.state_.bounds.width,
            "viewport bounds reserve external statuses before effects arrive")) return false;

        const auto path = std::filesystem::temp_directory_path() / ("xiui-party-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        settings_store store{path.string()};
        party.state_.bounds = {20, 30, 380, 110}; party.hp_display_ = 4; party.buffs_ = false;
        party.status_placement_ = party_module::status_right; party.status_frame_ = false;
        party.status_columns_ = 6; party.status_gap_ = 14; party.status_container_.opacity = 0.6f;
        party.meters_[1].appearance = ui::meter_style::modern; party.text_[3].offset_y = 3;
        party.text_[0].offset_y = 12; party.text_[party_module::heading_text].offset_y = -2;
        party.selected_text_ = static_cast<int>(party_module::heading_text);
        party.cast_placement_ = party_module::cast_name; party.job_placement_ = party_module::jobs_below;
        party.tp_display_ = 1; party.labels_ = true;
        party.layout_ = party_module::bars_mp_below_tp_left;
        party.bar_widths_ = {200, 150, 60, 140};
        party.bar_positions_[1] = party_module::position_center; party.bar_positions_[3] = party_module::position_right;
        party.bar_offsets_x_ = {0, 7, 0, -3}; party.bar_offsets_y_ = {0, 12, 0, 6};
        alliance.state_.bounds = {450, 30, 320, 100}; alliance.tp_ = false; third.state_.enabled = false;
        alliance.cast_placement_ = party_module::cast_tp;
        alliance.fit_width_ = false;
        party.selection_style_ = {0xFF55AAFF, 0xFFFF9955, 0.23f, 8, 5, false};
        party.Save(store); alliance.Save(store); third.Save(store); store.Flush();
        store.WriteFloat("party", "frame_size", 12); store.WriteBool("party", "scan_lines", false);
        store.WriteFloat("party", "status_frame_size", 1); store.WriteBool("party", "status_scan_lines", false);
        party_module loaded, second{1}, last{2}; loaded.Load(store); second.Load(store); last.Load(store);
        const ui::container_style nativeStyle;
        if (!check(loaded.selection_style_.target_color == 0xFF55AAFF && loaded.selection_style_.subtarget_color == 0xFFFF9955 &&
            loaded.selection_style_.background_opacity == 0.23f && loaded.SelectionPadding().x == 8 && loaded.SelectionPadding().y == 5 &&
            !loaded.selection_style_.arrow && second.selection_style_.arrow && second.selection_style_.background_opacity == 0.12f,
            "selection tint, padding, colors and arrow persist independently for each party list")) return false;
        if (!check(loaded.text_[0].offset_y == 12 && loaded.text_[party_module::heading_text].offset_y == -2 &&
            loaded.selected_text_ == party_module::heading_text && second.text_[party_module::heading_text].offset_y == 0,
            "member names and party/alliance headings retain independent text settings")) return false;
        if (!check(loaded.container_.edge_size == nativeStyle.edge_size && loaded.container_.scan_lines == nativeStyle.scan_lines &&
            loaded.status_container_.edge_size == nativeStyle.edge_size && loaded.status_container_.scan_lines == nativeStyle.scan_lines,
            "party containers use shared native defaults instead of retired frame and pattern overrides")) return false;
        if (!check(loaded.state_.bounds.x == 20 && loaded.hp_display_ == 4 && !loaded.buffs_ && loaded.meters_[1].appearance == ui::meter_style::modern &&
            loaded.text_[3].offset_y == 3 && loaded.status_placement_ == party_module::status_right && !loaded.status_frame_ &&
            loaded.status_columns_ == 6 && loaded.status_gap_ == 14 && loaded.status_container_.opacity == 0.6f &&
            loaded.cast_placement_ == party_module::cast_name && loaded.job_placement_ == party_module::jobs_below &&
            loaded.tp_display_ == 1 && loaded.labels_ && !second.labels_ && second.tp_display_ == 0 &&
            loaded.layout_ == party_module::bars_mp_below_tp_left && second.layout_ == party_module::bars_horizontal &&
            loaded.fit_width_ && !second.fit_width_ && loaded.tp_ && !second.tp_ && loaded.fills_[2] == party.fills_[2],
            "fitted width and TP visibility persist independently from TP color and other party lists")) return false;
        if (!check(
            loaded.bar_widths_ == party.bar_widths_ && second.bar_widths_ == std::array<float, 4>{} &&
            loaded.bar_positions_ == party.bar_positions_ && loaded.bar_offsets_x_ == party.bar_offsets_x_ && loaded.bar_offsets_y_ == party.bar_offsets_y_ &&
            second.bar_positions_ == std::array<int, 4>{} && second.bar_offsets_x_ == std::array<float, 4>{} && second.bar_offsets_y_ == std::array<float, 4>{} &&
            second.cast_placement_ == party_module::cast_tp && second.job_placement_ == party_module::jobs_name &&
            second.state_.bounds.x == 450 && !second.tp_ && !last.state_.enabled,
            "party and both alliance windows persist independent layout, data and shared component settings")) return false;
        store.WriteRect("layout.config", {100, 200, 820, 430}); store.Flush();
        hud_runtime runtime;
        runtime.settings_path_ = path.string(); runtime.LoadSettings();
        if (!check(runtime.config_.bounds.height == ui::config_layout::default_height && runtime.config_.bounds.width == 1100 &&
            runtime.config_.bounds.x == 100 && runtime.config_.bounds.y == 200,
            "existing default config grows to 1100 by 600 without resetting its position")) return false;
        runtime.config_.bounds.height = 520; runtime.config_.bounds.width = 1000; runtime.FlushSettings(); runtime.LoadSettings();
        if (!check(runtime.config_.bounds.height == 520 && runtime.config_.bounds.width == 1000,
            "later custom config dimensions survive the one-time migrations")) return false;

        std::array<uint8_t, 48> bitmap{};
        const auto write = [&](size_t offset, auto value) { std::memcpy(bitmap.data() + offset, &value, sizeof(value)); };
        write(0, uint32_t{40}); write(4, int32_t{1}); write(8, int32_t{2}); write(12, uint16_t{1}); write(14, uint16_t{32});
        write(40, uint32_t{0xFF123456}); write(44, uint32_t{0x80556677});
        const auto image = resources::DecodeItemIcon(bitmap);
        if (!check(image && image->pixels[0] == 0x80556677 && image->pixels[1] == 0xFF123456 &&
            !resources::DecodeItemIcon(std::span{bitmap}.first(47)), "native BGRA status icons retain alpha, orientation and bounds checks")) return false;
        source.Clear();
        return check(!source.Available() && source.Count(0) == 0 && source.Count(1) == 0 && !party.ShouldRender(snapshot),
            "zoning or logout clears membership and stale resources together");
    }

    auto config_input_test::TargetOfTargetSettings(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.module = &runtime.target_;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        const auto topics = runtime.target_.ConfigSections();
        const auto page = runtime.ActiveConfigPage();
        if (!check(topics.size() == BarConfigSections().size() + 2 && page.SectionCount() == 3 && runtime.HasConfigSections() &&
            std::strcmp(page.SectionName(0), "Target") == 0 && std::strcmp(page.SectionName(1), "Target of Target") == 0 &&
            std::strcmp(page.SectionName(2), "Subtarget") == 0,
            "Target, Target of Target and Subtarget are sibling top categories")) return false;
        const auto second = runtime.ConfigLayout().Section(1, 3), menu = runtime.ConfigLayout().Submenu(0, topics.size());
        if (!check(second.y + second.height < menu.y && runtime.ConfigLayout().Section(0, 3).x == menu.x,
            "the category row sits above both the secondary menu and content")) return false;
        click(second);
        if (!check(runtime.target_.ConfigView() == 1 && runtime.target_.ConfigSections().size() == BarConfigSections().size() &&
            page.SectionContains(1, &runtime.target_) && !page.SectionContains(0, &runtime.target_),
            "the recipient category selects its own settings topics and highlight")) return false;
        auto& settings = runtime.target_.Settings().recipient;
        const ui::rect main{20, 100, 440, 82};
        const auto classic = settings.Area(main, 90, 20), arrow = settings.Arrow(main, 20);
        if (!check(settings.placement == target_of_target_settings::inline_right && settings.arrow && !settings.distance &&
            !settings.cast && !settings.statuses && classic.x > main.x + main.width &&
            classic.width - 14 == (main.width - 14) / 3 && classic.y + 31 + settings.BarHeight(20) / 2 == main.y + 41 &&
            arrow.y + arrow.height / 2 == main.y + 41 && classic.x + 7 - (arrow.x + arrow.width) == settings.arrow_gap,
            "classic defaults use a one-third-width inline bar and an arrow centered on the main HP bar")) return false;
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(!settings.enabled && runtime.target_.State().enabled, "recipient visibility is independent of the main target")) return false;
        const auto recipientTopics = settings.ConfigSections();
        click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::animation), recipientTopics.size()));
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(!settings.follow_meter && settings.Option(target_of_target_settings::base_options + 13).visible &&
            settings.Option(target_of_target_settings::base_options + 16).visible,
            "Animation exposes the follow-style switch and unlocks independent sheen and smoothing")) return false;
        click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::effects), recipientTopics.size()));
        click(runtime.ConfigLayout().OptionRow(0));
        if (!check(settings.follow_meter && !settings.Option(target_of_target_settings::base_options + 6).visible &&
            !settings.Option(target_of_target_settings::base_options + 11).visible,
            "Effects exposes the same follow-style switch and restores inherited shadows and glow")) return false;
        for (size_t tab = 0; tab < recipientTopics.size(); ++tab)
        {
            click(runtime.ConfigLayout().Submenu(tab, recipientTopics.size()));
            if (!check(runtime.config_.section == tab && runtime.GroupOptionCount() > 0 &&
                runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                (runtime.ConfigSoundTarget(viewport) & 0xFFFFF) == 0x300 + tab,
                "recipient topics share matching drawing, click, hover and sound geometry")) return false;
        }
        for (size_t index = 0; index < target_of_target_settings::option_count; ++index)
        {
            size_t owners{};
            for (size_t tab = 0; tab < recipientTopics.size(); ++tab)
                owners += settings.Contains(index, tab);
            if (!check(owners == (index == 21 ? size_t{3} : size_t{1}),
                "recipient controls have one topic except the shared style switch in Bar Style, Animation and Effects")) return false;
        }
        click(runtime.ConfigLayout().Section(0, 3));
        if (!check(runtime.target_.ConfigView() == 0 && runtime.config_.section == static_cast<size_t>(bar_config_section::text) &&
            runtime.ElementSelector().count == 6, "switching to Target preserves the Text topic and restores its own profiles")) return false;
        click(runtime.ConfigLayout().Section(1, 3));
        if (!check(runtime.target_.ConfigView() == 1 && runtime.config_.section == static_cast<size_t>(bar_config_section::text) && runtime.ElementSelector().count == 4,
            "switching back restores recipient Text without leaking the main target options")) return false;
        const std::array<float, 4> textSizes{1.25f, 0.75f, 1.0f, 0.5f};
        for (size_t i = 0; i < textSizes.size(); ++i)
        {
            settings.selected_text = static_cast<int>(i);
            auto size = settings.Option(target_of_target_settings::base_options + meter_style_option_count + 1);
            if (!check(size.available && size.visible && size.slider == &settings.text[i].style.scale && size.display_scale == ui::text_base_pixels,
                "each recipient text selection edits its own native pixel size")) return false;
            *size.slider = textSizes[i];
        }
        for (size_t i = 0; i < textSizes.size(); ++i)
            if (!check(settings.text[i].style.scale == textSizes[i], "editing another recipient text size leaves the others unchanged")) return false;
        size_t targetSwitches{};
        for (size_t i = general_module::enabled_modules_option; i < runtime.general_.AttributionStart(); ++i)
            targetSwitches += runtime.general_.Option(i).toggle == &runtime.target_.State().enabled;
        if (!check(targetSwitches == 1, "main Target retains one entry in Enabled Modules")) return false;
        settings.placement = target_of_target_settings::inline_left;
        const auto left = settings.Area(main, 90, 20), leftArrow = settings.Arrow(main, 20);
        if (!check(settings.ShowArrow() && left.x < main.x &&
            leftArrow.x - (left.x + left.width - 7) == settings.arrow_gap &&
            main.x + 7 - (leftArrow.x + leftArrow.width) == settings.gap &&
            leftArrow.y + leftArrow.height / 2 == main.y + 41,
            "inline left keeps the arrow between the bars and centered on main HP")) return false;
        for (const int placement : {target_of_target_settings::above, target_of_target_settings::below})
        {
            settings.placement = placement;
            settings.arrow = true;
            const auto withArrow = settings.Area(main, 90, 20), hiddenArrow = settings.Arrow(main, 20);
            if (!check(!settings.ShowArrow() && hiddenArrow.width == 0 && hiddenArrow.height == 0 &&
                !settings.Option(14).visible && !settings.Option(15).visible && !settings.Option(27).visible,
                "above and below hide the arrow and inline-only arrow controls")) return false;
            settings.arrow = false;
            const auto withoutArrow = settings.Area(main, 90, 20);
            if (!check(withArrow.x == withoutArrow.x && withArrow.y == withoutArrow.y,
                "stacked layouts reserve no arrow space regardless of the saved toggle")) return false;
        }
        settings.placement = target_of_target_settings::below;
        settings.arrow = false; settings.health = settings.percent = settings.cast = settings.statuses = false;
        settings.width = 238; settings.bar_height = 12; settings.gap = 4; settings.offset_x = 17; settings.offset_y = -70;
        settings.follow_meter = settings.follow_fill = settings.follow_text = false;
        settings.fill_top = 0xFFAABBCC; settings.arrow_color = 0x80887766;
        settings.text[0].style.glow.enabled = true; settings.text[0].style.bottom_color = 0xFF335577;
        const auto area = settings.Area(main, 50);
        if (!check(area.x == 37 && area.y == 84 && area.width == 238, "stacked sizing and signed offsets apply after spacing")) return false;
        auto saved = settings_store::FromIniText(""); runtime.target_.Save(saved);
        target_module restored; restored.Load(saved);
        const auto& loaded = restored.Settings().recipient;
        if (!check(!loaded.enabled && !loaded.health && !loaded.percent && !loaded.cast && !loaded.statuses &&
            loaded.width == 238 && loaded.bar_height == 12 && loaded.gap == 4 && loaded.offset_x == 17 && loaded.offset_y == -70 &&
            loaded.placement == target_of_target_settings::below && !loaded.arrow && !loaded.follow_meter && !loaded.follow_fill &&
            loaded.fill_top == 0xFFAABBCC && loaded.arrow_color == 0x80887766 && loaded.text[0].style.glow.enabled &&
            loaded.text[0].style.bottom_color == 0xFF335577,
            "recipient layout, arrow, independent bar colors and styled text persist through profiles")) return false;
        for (size_t i = 0; i < textSizes.size(); ++i)
            if (!check(loaded.text[i].style.scale == textSizes[i], "all four independent recipient text sizes persist")) return false;
        auto scaled = settings_store::FromIniText("[target.target_of_target]\ntext_scale=1.5\nfollow_text=1\n"
            "[text.target.name]\nscale=1.25\n[text.target.distance]\nscale=0.75\n[text.target.labels]\nscale=1\n[text.target.values]\nscale=0.5\n"
            "[text.target.recipient.name]\nscale=0.5\n");
        restored.Load(scaled);
        if (!check(!loaded.follow_text, "a legacy shared multiplier becomes independent text sizes")) return false;
        for (size_t i = 0; i < textSizes.size(); ++i)
            if (!check(loaded.text[i].style.scale == textSizes[i] * 1.5f, "legacy scaling preserves the previously displayed inherited sizes")) return false;
        restored.Save(scaled); restored.Load(scaled);
        for (size_t i = 0; i < textSizes.size(); ++i)
            if (!check(loaded.text[i].style.scale == textSizes[i] * 1.5f, "saved recipient sizes are not scaled a second time")) return false;
        auto legacy = settings_store::FromIniText("[target.target_of_target]\nwidth=0\nbar_height=0\ngap=6\npercent=1\nlabel=1\ndistance=1\ncast=1\nstatuses=1\nattack=1\n");
        restored.Load(legacy);
        if (!check(restored.Settings().recipient.placement == target_of_target_settings::inline_right && !restored.Settings().recipient.distance &&
            restored.Settings().recipient.gap == 8, "untouched earlier defaults migrate to the classic inline presentation")) return false;
        legacy.WriteFloat("target.target_of_target", "offset_x", 27);
        restored.Load(legacy);
        if (!check(restored.Settings().recipient.placement == target_of_target_settings::below && restored.Settings().recipient.offset_x == 27,
            "customized earlier placement survives the new default")) return false;
        return true;
    }

    auto config_input_test::TargetChildren(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.LoadSettings(settings_store::FromIniText("[layout.grid]\nsnap_grid=0\nsnap_elements=0\n"));
        runtime.editing_ = true;
        if (!check(!runtime.subtarget_.HasEditableBounds() && !runtime.target_recipient_.HasEditableBounds(),
            "missing child settings retain attached layouts without separate movement hit areas")) return false;
        auto& recipient = runtime.target_.Settings().recipient;
        recipient.placement = target_of_target_settings::detached;
        recipient.offset_x = 17; recipient.gap = 11;
        *runtime.subtarget_.Option(13).choice = 1;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto originalMain = runtime.target_.State().bounds;
        for (auto* child : {&runtime.target_recipient_, &runtime.subtarget_})
        {
            child->State().bounds = {420, 240, 220, 80};
            const auto movable = runtime.ControllerMovableModules();
            if (!check(child->PreviewVisible() && std::find(movable.begin(), movable.end(), child) != movable.end(),
                "detached children join the shared controller movement list")) return false;
            runtime.config_.module = child;
            const auto page = runtime.ActiveConfigPage();
            const size_t view = child == &runtime.subtarget_ ? 2 : 1;
            if (!check(page.Contains(child) && page.SectionContains(view, child),
                "dragged child retains its Target category and matching configuration tab")) return false;
            const auto original = child->State().bounds;
            auto& focus = runtime.config_.focus;
            focus.moving = true; focus.moving_id = child->Id(); focus.move_original = original;
            focus.move_desired = original; focus.move_collapsed = false; focus.viewport = viewport;
            runtime.HandleControllerMove(controller_action::right, false);
            const auto pending = runtime.CaptureSettings();
            const auto section = std::string{"layout."} + child->Id();
            if (!check(child->State().bounds.x > original.x && pending.ReadRect(section.c_str(), {}).x == original.x &&
                runtime.target_.State().bounds.x == originalMain.x && runtime.target_.State().bounds.y == originalMain.y,
                "child movement leaves Target fixed and automatic saving retains the committed child position")) return false;
            runtime.HandleControllerMove(controller_action::cancel, false);
            if (!check(!focus.moving && child->State().bounds.x == original.x && child->State().bounds.y == original.y,
                "controller Cancel restores each detached child's position")) return false;
            runtime.CenterModuleLayout(*child, 0, viewport);
            if (!check(runtime.target_.State().bounds.x == originalMain.x, "centering a child leaves Target fixed")) return false;
            child->SetDisplayAllowed(false);
            if (!check(!child->PreviewVisible() && !child->ShouldRender({}), "context-hidden children expose no preview or live surface")) return false;
            child->SetDisplayAllowed(true);
        }
        auto saved = runtime.CaptureSettings();
        hud_runtime restored; restored.LoadSettings(saved);
        if (!check(restored.target_.Settings().recipient.placement == target_of_target_settings::detached &&
            *restored.subtarget_.Option(13).choice == 1 &&
            restored.target_recipient_.State().bounds.x == runtime.target_recipient_.State().bounds.x &&
            restored.subtarget_.State().bounds.x == runtime.subtarget_.State().bounds.x,
            "both child modes and independent positions persist through ordinary profiles")) return false;
        recipient.placement = target_of_target_settings::inline_right;
        *runtime.subtarget_.Option(13).choice = 0;
        if (!check(recipient.offset_x == 17 && recipient.gap == 11 && !runtime.target_recipient_.HasEditableBounds() &&
            !runtime.subtarget_.HasEditableBounds(), "reattaching restores linked preferences and removes detached hit areas")) return false;
        runtime.editing_ = false;
        std::puts("Target child configuration, independent movement, committed saves, cancellation, visibility and profile persistence passed.");
        return true;
    }

    auto config_input_test::PartyModuleVisibility(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        size_t masters{}, children{};
        for (size_t i = general_module::enabled_modules_option; i < runtime.general_.AttributionStart(); ++i)
        {
            const auto option = runtime.general_.Option(i);
            masters += option.toggle == runtime.general_.PartyEnabledFlag();
            children += option.toggle == &runtime.alliance_first_.State().enabled || option.toggle == &runtime.alliance_second_.State().enabled;
        }
        if (!check(masters == 1 && children == 0, "Enabled Modules has one Party master and no separate alliance switches")) return false;
        runtime.party_source_.player_id_ = 1;
        for (size_t i : {size_t{0}, size_t{6}, size_t{12}}) runtime.party_source_.members_[i].active = true;
        for (unsigned mask = 0; mask < 4; ++mask)
        {
            runtime.alliance_first_.State().enabled = (mask & 1) != 0;
            runtime.alliance_second_.State().enabled = (mask & 2) != 0;
            runtime.config_.module = &runtime.party_;
            const auto page = runtime.ActiveConfigPage();
            if (!check(page.SectionCount() == 3 && runtime.HasConfigSections(), "all Party settings selectors remain available for restoring disabled lists")) return false;
            for (size_t index = 1; index < 3; ++index)
            {
                auto* module = page.SectionModule(index);
                click(runtime.ConfigLayout().Section(index, 3));
                if (!check(runtime.config_.module == module && module->PreviewVisible() == module->State().enabled &&
                    module->ShouldRender({}) == module->State().enabled, "alliance visibility controls both live rendering and the edit preview")) return false;
            }
        }
        auto disabled = settings_store::FromIniText("[modules]\nparty=0\n");
        runtime.general_.Load(disabled);
        const auto page = runtime.ActiveConfigPage();
        for (auto* module : {&runtime.party_, &runtime.alliance_first_, &runtime.alliance_second_})
            if (!check(!module->PreviewVisible() && !module->ShouldRender({}) && module->State().enabled,
                "Party master hides all lists without changing their individual enable choices")) return false;
        runtime.alliance_first_.ResetDefaults(ui::theme::CreateDefault());
        runtime.alliance_second_.CopyPreferences(runtime.party_);
        if (!check(!runtime.alliance_first_.PreviewVisible() && !runtime.alliance_second_.PreviewVisible() && !runtime.ConfigPageVisible(page),
            "list resets and preference copies retain the disabled master gate")) return false;
        const auto saved = runtime.CaptureSettings();
        hud_runtime restored;
        for (auto* module : restored.modules_.All()) module->Load(saved);
        if (!check(!restored.general_.PartyEnabled() && !restored.party_.PreviewVisible(), "Party master persists through profiles")) return false;
        runtime.general_.Load(settings_store::FromIniText(""));
        runtime.alliance_first_.State().enabled = false;
        if (!check(runtime.party_.PreviewVisible() && !runtime.alliance_first_.PreviewVisible() && runtime.alliance_second_.PreviewVisible() && runtime.ConfigPageVisible(page),
            "reenabling Party restores only the enabled lists")) return false;
        return true;
    }

    auto config_input_test::EnabledModules(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-enabled-modules-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
        std::filesystem::create_directory(root);
        struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } cleanupFiles{root};
        hud_runtime runtime;
        runtime.settings_path_ = (root / "xiuinative.ini").string();
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.editing_ = true;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto click = [&](const ui::rect& bounds) {
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        for (auto* module : runtime.modules_.All())
        {
            if (module == &runtime.general_) continue;
            const auto enabled = module->Option(0);
            if (!check(enabled.kind == module_option_kind::toggle && enabled.toggle != nullptr,
                "every configurable module exposes its existing enable switch")) return false;
            *enabled.toggle = true;
            const bool inventoryChild = std::find(runtime.inventory_pages_.begin() + 1, runtime.inventory_pages_.end(), module) != runtime.inventory_pages_.end();
            const bool partyChild = module == &runtime.alliance_first_ || module == &runtime.alliance_second_;
            const bool* flag = module == &runtime.inventory_[0] ? runtime.general_.InventoryEnabledFlag() : module == &runtime.party_ ? runtime.general_.PartyEnabledFlag() : enabled.toggle;
            size_t matches{};
            for (size_t i = general_module::enabled_modules_option; i < runtime.general_.OptionCount(); ++i)
                matches += runtime.general_.Option(i).toggle == flag;
            if (!check(matches == (inventoryChild || partyChild ? 0u : 1u), "General groups Inventory and Party while retaining their child switches in module settings")) return false;
        }
        const auto casting = runtime.ConfigPages()[1];
        if (!check(std::strcmp(casting.id, "casting") == 0 && std::strcmp(casting.name, "Casting") == 0 &&
            casting.Count() == 3 && casting.primary == &runtime.cast_bar_ && casting.secondary == &runtime.cast_cost_ && casting.tertiary == &runtime.recast_ &&
            ui::config_controls::IconId(casting.id) == ui::icon_glyph::castbar,
            "Casting groups both original modules under one alphabetized sidebar entry")) return false;
        click(runtime.ConfigLayout().ModuleTab(1));
        if (!check(runtime.config_.module == &runtime.cast_bar_ && runtime.HasConfigSections(), "Casting opens the Cast Bar settings")) return false;
        click(runtime.ConfigLayout().Section(1, 2));
        if (!check(runtime.config_.module == &runtime.cast_cost_ && runtime.HasConfigSections() && runtime.HasConfigSubmenu(),
            "the Cast Cost selector opens its existing settings in Casting")) return false;
        click(runtime.ConfigLayout().Section(0, 2));
        if (!check(runtime.config_.module == &runtime.cast_bar_, "the Cast Bar selector returns to its independent settings")) return false;
        const auto groupedPages = runtime.ConfigPages();
        const auto inventoryPage = std::find_if(groupedPages.begin(), groupedPages.end(), [](const auto& page) { return std::strcmp(page.id, "inventory") == 0; });
        if (!check(inventoryPage != groupedPages.end() && inventoryPage->Count() == 8 && inventoryPage->SectionCount() == 3 &&
            inventoryPage->SectionModule(1) == &runtime.equipment_ && inventoryPage->SectionModule(2) == &runtime.gil_,
            "Inventory groups storage, Equipment and Gil under three settings selectors")) return false;
        for (const auto& page : groupedPages)
            if (!check(std::strcmp(page.id, "equipment") != 0 && std::strcmp(page.id, "gil") != 0,
                "Equipment and Gil have no duplicate sidebar entries")) return false;
        click(runtime.ConfigLayout().ModuleTab(static_cast<size_t>(inventoryPage - groupedPages.begin())));
        for (size_t index = 0; index < runtime.inventory_.size(); ++index)
        {
            click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), index, runtime.inventory_.size()));
            if (!check(runtime.config_.module == &runtime.inventory_[index] && runtime.IsInventorySettings() &&
                inventoryPage->SectionContains(0, runtime.config_.module) && runtime.ConfigSoundTarget(viewport) != 0 &&
                runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                "each storage selector retains its settings, Inventory section, hover and sound target")) return false;
        }
        click(runtime.ConfigLayout().Section(1, 3));
        if (!check(runtime.config_.module == &runtime.equipment_ && !runtime.IsInventorySettings() && runtime.GroupOptionCount() > 0,
            "Equipment opens its original settings inside Inventory")) return false;
        click(runtime.ConfigLayout().Section(2, 3));
        if (!check(runtime.config_.module == &runtime.gil_ && !runtime.IsInventorySettings() && runtime.GroupOptionCount() > 0,
            "Gil opens its original settings inside Inventory")) return false;
        click(runtime.ConfigLayout().ModuleTab(0));
        click(runtime.ConfigLayout().Submenu(general_module::enabled_modules_section, 2));
        if (!check(runtime.config_.module == &runtime.general_ && ControlCount(runtime) == runtime.modules_.Count() - runtime.inventory_.size() - 2,
            "Enabled Modules exposes one Inventory group and the other module switches")) return false;
        const auto toggle = [&](hud_module& module) {
            size_t row{};
            const bool* flag = &module == &runtime.inventory_[0] ? runtime.general_.InventoryEnabledFlag() : &module == &runtime.party_ ? runtime.general_.PartyEnabledFlag() : module.Option(0).toggle;
            while (row < runtime.GroupOptionCount() && runtime.general_.Option(runtime.GroupOptionIndex(row)).toggle != flag) ++row;
            if (row == runtime.GroupOptionCount()) return false;
            const int maxScroll = std::max(0, (static_cast<int>((runtime.GroupOptionCount() + 1) / 2) - runtime.ConfigLayout().VisibleRows()) * 2);
            runtime.config_.scroll = std::min(static_cast<int>(row / 2 * 2), maxScroll);
            click(runtime.ConfigLayout().OptionRow(static_cast<int>(row) - runtime.config_.scroll));
            return true;
        };
        const auto castBounds = runtime.cast_bar_.State().bounds;
        if (!toggle(runtime.cast_bar_) || !check(!runtime.cast_bar_.State().enabled && runtime.VisibleConfigPages().size() == runtime.ConfigPages().size(),
            "Casting remains available while Cast Cost is enabled")) return false;
        if (!toggle(runtime.cast_cost_) || !check(runtime.VisibleConfigPages().size() == runtime.ConfigPages().size() - 1,
            "disabling both casting modules removes their shared sidebar page")) return false;
        const auto saved = runtime.CaptureSettings();
        hud_runtime restored;
        for (auto* module : restored.modules_.All()) module->Load(saved);
        if (!check(!restored.cast_bar_.State().enabled && !restored.cast_cost_.State().enabled && restored.VisibleConfigPages().size() == runtime.VisibleConfigPages().size(),
            "existing saved enable keys restore sidebar visibility")) return false;
        if (!toggle(runtime.cast_cost_) || !toggle(runtime.cast_bar_) || !check(runtime.cast_bar_.State().enabled && runtime.cast_bar_.State().bounds.x == castBounds.x &&
            runtime.cast_bar_.State().bounds.y == castBounds.y, "enabling a module restores the sidebar without moving it")) return false;
        std::array<bool, 6> inventoryVisibility{};
        for (size_t i = 0; i < runtime.inventory_.size(); ++i)
            runtime.inventory_[i].State().enabled = inventoryVisibility[i] = i % 2 == 1;
        if (!toggle(runtime.inventory_[0])) return false;
        if (!check(runtime.ConfigPageVisible(*inventoryPage), "Equipment and Gil keep Inventory reachable when storage is disabled")) return false;
        if (!toggle(runtime.equipment_) || !toggle(runtime.gil_) || !check(!runtime.ConfigPageVisible(*inventoryPage),
            "the category hides when storage, Equipment and Gil are all disabled")) return false;
        if (!toggle(runtime.equipment_) || !check(runtime.ConfigPageVisible(*inventoryPage),
            "Equipment alone keeps the shared category visible")) return false;
        if (!toggle(runtime.gil_)) return false;
        for (size_t i = 0; i < runtime.inventory_.size(); ++i)
            if (!check(!runtime.inventory_[i].PreviewVisible() && !runtime.inventory_[i].ShouldRender({}) &&
                runtime.inventory_[i].State().enabled == inventoryVisibility[i], "Inventory master hides HUD and previews without rewriting child visibility")) return false;
        const auto grouped = runtime.CaptureSettings();
        for (auto* module : restored.modules_.All()) module->Load(grouped);
        restored.inventory_[1].ResetDefaults(ui::theme::CreateDefault());
        if (!check(!restored.general_.InventoryEnabled() && !restored.inventory_[1].PreviewVisible() &&
            !grouped.ReadBool("modules", "inventory", true), "master switch persists through profiles and child resets retain the group gate")) return false;
        if (!toggle(runtime.inventory_[0])) return false;
        for (size_t i = 0; i < runtime.inventory_.size(); ++i)
            if (!check(runtime.inventory_[i].PreviewVisible() == inventoryVisibility[i] && runtime.inventory_[i].State().enabled == inventoryVisibility[i],
                "reenabling Inventory restores previews only for enabled trackers")) return false;
        size_t inventoryOption = general_module::enabled_modules_option;
        while (runtime.general_.Option(inventoryOption).toggle != runtime.general_.InventoryEnabledFlag()) ++inventoryOption;
        const auto groupOption = runtime.general_.Option(inventoryOption);
        if (!check(groupOption.submodules != nullptr && std::strstr(groupOption.submodules, "Satchel") &&
            std::strstr(groupOption.submodules, "Wardrobes"), "Inventory shows a nested submodule summary with one master control")) return false;
        const auto visible = runtime.VisibleConfigPages();
        for (size_t index = 0; index < visible.size(); ++index)
        {
            click(runtime.ConfigLayout().ModuleTab(index));
            if (!check(runtime.config_.module == visible[index].primary && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                runtime.ConfigSoundTarget(viewport) == 0x100 + index, "sidebar drawing, clicks, hover and sound use the same compacted indices")) return false;
        }
        for (const auto& page : runtime.ConfigPages())
        {
            if (page.primary == &runtime.general_) continue;
            for (size_t member = 0; member < page.Count(); ++member) *page.Module(member)->Option(0).toggle = false;
            if (page.primary == &runtime.party_)
            {
                if (!check(runtime.ConfigPageVisible(page), "Party settings stay reachable with all lists disabled")) return false;
                continue;
            }
            if (page.primary == &runtime.inventory_[0])
            {
                if (!check(runtime.ConfigPageVisible(page), "Inventory settings remain accessible when all individual trackers are hidden")) return false;
                continue;
            }
            if (!check(!runtime.ConfigPageVisible(page), "pages disappear only when all of their members are disabled")) return false;
            if (page.Count() > 1)
            {
                *page.Module(page.Count() - 1)->Option(0).toggle = true;
                if (!check(runtime.ConfigPageVisible(page), "any enabled Chat, Party, Inventory or Extras member keeps its page reachable")) return false;
                *page.Module(page.Count() - 1)->Option(0).toggle = false;
            }
        }
        runtime.config_.module = &runtime.general_;
        runtime.config_.section = general_module::enabled_modules_section;
        if (!toggle(runtime.inventory_[0])) return false;
        if (!toggle(runtime.party_)) return false;
        runtime.config_.module = &runtime.target_;
        runtime.NormalizeConfigSelection();
        if (!check(runtime.config_.module == &runtime.general_ && runtime.config_.section == general_module::enabled_modules_section &&
            runtime.VisibleConfigPages().size() == 1, "a hidden active page falls back to General and every module can still be restored")) return false;
        click(runtime.ConfigLayout().ModuleTab(1));
        if (!check(runtime.config_.module == &runtime.general_ && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow &&
            runtime.ConfigSoundTarget(viewport) == 0, "removed sidebar rows leave no click, cursor or sound targets")) return false;
        if (!toggle(runtime.target_) || !toggle(runtime.game_hud_)) return false;
        const auto recovered = runtime.VisibleConfigPages();
        if (!check(recovered.size() == 3 && std::strcmp(recovered[1].id, "target") == 0 && std::strcmp(recovered.back().id, "extras") == 0,
            "reenabling modules preserves General first, alphabetical pages and Extras last")) return false;
        click(runtime.ConfigLayout().Submenu(general_module::appearance_section, 2));
        return check(ControlCount(runtime) == 2, "global appearance retains its separate topic");
    }

    auto config_input_test::NotificationGroupsInput(void) -> bool
    {
        hud_runtime runtime;
        runtime.notifications_.Load(settings_store::FromIniText("[notifications]\ngroup_count=6\nenabled=0\n"));
        if (!check(!runtime.notification_groups_[4].PreviewVisible(), "disabled notifications hide every group preview")) return false;
        runtime.notifications_.State().enabled = true;
        runtime.editing_ = true;
        runtime.config_.module = &runtime.notifications_;
        runtime.config_.section = 2;
        runtime.notifications_.SelectGroup(3);
        if (!check(runtime.MovementSelectionModule() == &runtime.notification_groups_[2] && runtime.notification_groups_[2].PreviewVisible(),
            "each group uses the shared movement adapter")) return false;
        runtime.FocusModuleConfig(runtime.notification_groups_[4]);
        if (!check(runtime.config_.module == &runtime.notifications_ && runtime.notifications_.SelectedGroup() == 5,
            "clicking an independent group opens its owner settings")) return false;
        runtime.notifications_.Group(5).text.style.glow.enabled = true;
        runtime.notifications_.Group(5).state.bounds.x = 321;
        auto saved = runtime.CaptureSettings();
        notification_module restored; restored.Load(saved);
        if (!check(restored.Group(5).text.style.glow.enabled && restored.Group(5).state.bounds.x == 321,
            "independent group text effects and positions participate in profiles")) return false;
        *runtime.notifications_.Option(27).choice = 0; runtime.notifications_.OnOptionChanged(27);
        if (!check(!runtime.notification_groups_[4].PreviewVisible() && runtime.notifications_.SelectedGroup() < 2,
            "removed groups leave edit mode and selection falls back")) return false;
        runtime.editing_ = false;
        const auto now = ::GetTickCount64();
        runtime.notifications_.Center().Post({notification_kind::party_invite, 11, 0, "Party Invitation: Alice"}, now - 4000);
        runtime.notifications_.Center().Post({notification_kind::trade_invite, 22, 0, "Trade Request: Bob"}, now - 4000);
        const module_render_context context{runtime.design_system_};
        runtime.notifications_.Group(0).screen_anchor = false;
        runtime.notifications_.RenderGroup(0, context);
        const auto id = runtime.notifications_.FirstDetail();
        const auto hit = runtime.notifications_.Detail(id);
        const auto hovered = hit ? runtime.notifications_.DetailAt(hit->bounds.x + 1, hit->bounds.y + 1) : std::nullopt;
        if (!check(hit && hovered && hovered->id == id,
            "minified notices expose the same sender details to pointer inspection")) return false;
        runtime.notifications_.Group(0).style.pin_invites = false;
        runtime.notifications_.RenderGroup(0, context);
        if (!check(runtime.notifications_.Detail(id)->bounds.height > hit->bounds.height, "minification reduces the invitation footprint while retaining identity")) return false;
        runtime.notifications_.Group(0).style.pin_invites = true;
        runtime.notifications_.RenderGroup(0, context);
        if (!check(runtime.BeginControllerFocus(hud_runtime::controller_surface::notifications), "notification details have an explicit controller entry")) return false;
        runtime.HandleController({controller_action::down, controller_backend::none, 0, true, false, false, now});
        if (!check(runtime.controller_notification_id_ != id && runtime.notifications_.Center().Entries().size() == 2,
            "controller browsing follows stable notice identity without responding to invites")) return false;
        runtime.HandleController({controller_action::confirm, controller_backend::none, 0, true, false, false, now});
        if (!check(runtime.notifications_.Center().Entries().size() == 2, "reading a notice cannot accept or decline it")) return false;
        runtime.HandleController({controller_action::cancel, controller_backend::none, 0, true, false, false, now});
        if (!check(!runtime.ControllerCaptured(), "Cancel releases notification focus")) return false;
        const auto names = runtime.general_.NameplatesOption();
        if (!check(runtime.general_.ConfigSectionContains(general_module::visibility_section, names) &&
            !runtime.general_.ConfigSectionContains(general_module::appearance_section, names), "nameplate control belongs to Visibility")) return false;
        *runtime.general_.Option(names).toggle = true; runtime.general_.OnOptionChanged(names);
        if (!check(runtime.general_.TakeNameplatesRequest() == true && !runtime.general_.TakeNameplatesRequest(), "hiding names emits one native request")) return false;
        saved = runtime.CaptureSettings(); general_module general; general.Load(saved);
        if (!check(*general.Option(general.NameplatesOption()).toggle && general.TakeNameplatesRequest() == true, "nameplate preference survives save and load")) return false;
        *general.Option(general.NameplatesOption()).toggle = false; general.OnOptionChanged(general.NameplatesOption());
        return check(general.TakeNameplatesRequest() == false, "turning off nameplate hiding requests native restoration");
    }

    auto config_input_test::MinimapEdgeButtons(void) -> bool
    {
        hud_runtime runtime;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.module = &runtime.minimap_;
        runtime.config_.section = 0;
        constexpr size_t index = minimap_module::coordinate_edges_option;
        const auto option = runtime.minimap_.Option(index);
        const auto slots = runtime.GroupOptionSlots();
        if (!check(option.kind == module_option_kind::toggle_buttons && option.choice_count == 4 &&
            std::count(slots.begin(), slots.end(), index) == 1, "coordinate edges occupy one setting with four independent buttons")) return false;
        const auto slot = std::find(slots.begin(), slots.end(), index) - slots.begin();
        const auto row = runtime.ConfigLayout().OptionRow(static_cast<int>(slot));
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        for (size_t part = 0; part < 4; ++part)
        {
            const auto button = ui::config_layout::ToggleButton(row, part, 4);
            runtime.mouse_x_ = static_cast<int>(button.x + button.width / 2);
            runtime.mouse_y_ = static_cast<int>(button.y + button.height / 2);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            for (size_t edge = 0; edge < 4; ++edge)
                if (!check(option.button_toggles[edge] == (edge > part), "pointer activation changes only its coordinate edge")) return false;
        }
        runtime.config_.focus.region = config_controller_region::options;
        const auto items = runtime.ControllerConfigItems(config_controller_region::options);
        const auto first = std::find_if(items.begin(), items.end(), [](const auto& item) { return item.index == index; });
        if (!check(first != items.end() && std::count_if(items.begin(), items.end(), [](const auto& item) { return item.index == index; }) == 4,
            "every edge button has a separate controller focus target")) return false;
        auto& focus = runtime.config_.focus;
        focus.id = first->id; focus.Resolve(items);
        for (size_t part = 0; part < 4; ++part)
        {
            const auto& item = items[focus.item];
            if (!check(item.index == index && item.part == part, "directional focus follows the four buttons in order")) return false;
            runtime.ActivateControllerConfigItem(item, controller_action::confirm, true);
            if (!check(!option.button_toggles[part], "held confirmation cannot repeat a toggle")) return false;
            runtime.ActivateControllerConfigItem(item, controller_action::confirm, false);
            if (!check(option.button_toggles[part], "controller activation shares the pointer toggle")) return false;
            if (part < 3) focus.MoveGrid(items, 1, 0);
        }
        return true;
    }

    auto config_input_test::AllUiVisibility(void) -> bool
    {
        hud_runtime runtime;
        runtime.game_hud_.Load(settings_store::FromIniText("[game_hud]\nenabled=1\nshow_player=0\nshow_party=1\n"));
        runtime.world_info_.State().enabled = false;
        const auto nativeMask = runtime.NativeHudHiddenMask();
        const auto playerBounds = runtime.player_.State().bounds;
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.section = general_module::visibility_section;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        if (!check(runtime.GroupOptionIndex(0) == runtime.general_.HideAllUiOption(), "global hiding is reachable in General Visibility")) return false;
        const auto button = ui::config_layout::ChoiceButton(runtime.ConfigLayout().OptionRow(0));
        runtime.mouse_x_ = static_cast<int>(button.x + button.width * 0.5f);
        runtime.mouse_y_ = static_cast<int>(button.y + button.height * 0.5f);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.IsVisible() && runtime.ConfirmingAction() && runtime.profile_keyboard_selection_ == 0 &&
            runtime.confirmation_message_.find("press /, type xiui show") != std::string::npos &&
            runtime.confirmation_message_.find("Typing will be invisible") != std::string::npos,
            "hide-all opens a Cancel-first warning with complete recovery instructions")) return false;
        runtime.HandleProfileKey(VK_RETURN, 0);
        runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0x80000000u), false);
        if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused() && runtime.NativeHudHiddenMask() == nativeMask,
            "default Enter cancels hiding and leaves visibility unchanged")) return false;
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        runtime.HandleProfileKey(VK_ESCAPE, 0);
        runtime.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
        if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused(), "Escape dismisses the hide-all warning")) return false;
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        const ui::profile_dialog_layout dialog{runtime.config_.bounds, viewport};
        runtime.mouse_x_ = static_cast<int>(dialog.Create().x + 5);
        runtime.mouse_y_ = static_cast<int>(dialog.Create().y + 5);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        const uint32_t allNative = (uint32_t{1} << native_hud_options.size()) - 1;
        if (!check(!runtime.IsVisible() && !runtime.IsEditing() && runtime.NativeHudHiddenMask() == allNative &&
            !runtime.ContextVisible(runtime.player_) && runtime.ControllerCapture() == controller_capture_mode::none && !runtime.ApplyConfirmedAction(),
            "explicit Hide all hides both HUDs exactly once and releases XIUI focus")) return false;
        if (!check(runtime.HandleMouseMessage(WM_LBUTTONUP, 0, 0, false) && !runtime.visibility_pointer_release_,
            "hiding from a pointer press consumes its release while the UI is hidden")) return false;
        runtime.SetVisible(true);
        if (!check(runtime.NativeHudHiddenMask() == nativeMask && !runtime.world_info_.State().enabled &&
            runtime.player_.State().bounds.x == playerBounds.x && runtime.player_.State().bounds.y == playerBounds.y,
            "restoring preserves native choices, disabled modules and positions")) return false;

        *runtime.game_hud_.Option(0).toggle = false;
        runtime.editing_ = true;
        runtime.config_.focus.region = config_controller_region::options;
        const auto items = runtime.ControllerConfigItems(config_controller_region::options);
        const auto item = std::find_if(items.begin(), items.end(), [&](const auto& value) { return value.index == runtime.general_.HideAllUiOption(); });
        if (!check(item != items.end(), "controller can reach the shared hide-all action")) return false;
        runtime.ActivateControllerConfigItem(*item, controller_action::confirm, true);
        if (!check(runtime.IsVisible(), "held Confirm cannot hide the UI repeatedly")) return false;
        runtime.ActivateControllerConfigItem(*item, controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::confirm, true);
        if (!check(runtime.IsVisible() && runtime.ConfirmingAction(), "controller activation opens the warning without hiding on held Confirm")) return false;
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused(), "controller confirmation defaults to Cancel")) return false;
        runtime.ActivateControllerConfigItem(*item, controller_action::confirm, false);
        runtime.HandleControllerConfig(controller_action::right, false);
        runtime.HandleControllerConfig(controller_action::confirm, false);
        if (!check(!runtime.IsVisible() && runtime.NativeHudHiddenMask() == allNative, "controller hides both HUDs even with native controls paused")) return false;
        runtime.HandleCommand("toggleui");
        return check(runtime.IsVisible() && runtime.NativeHudHiddenMask() == 0 && !*runtime.game_hud_.Option(0).toggle &&
            !*runtime.game_hud_.Option(1).toggle && *runtime.game_hud_.Option(2).toggle,
            "toggle command restores the native master switch and individual preferences");
    }

    auto config_input_test::PetTabs(void) -> bool
    {
        hud_runtime runtime;
        runtime.pet_.Load(settings_store::FromIniText("[pet]\nenabled=1\n[pet.target]\nenabled=1\n"));
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.module = &runtime.pet_;
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto selectedOnly = [&](const size_t selected) {
            const auto page = runtime.ActiveConfigPage();
            if (!check(runtime.config_.module == &runtime.pet_ && runtime.pet_.ConfigView() == selected,
                "Pet tab activation opens the matching settings view")) return false;
            for (size_t index = 0; index < page.SectionCount(); ++index)
                if (!check(page.SectionContains(index, runtime.config_.module) == (index == selected),
                    "exactly one Pet tab is selected when pet types share a module")) return false;
            return true;
        };
        const auto count = runtime.ActiveConfigPage().SectionCount();
        for (size_t selected = 0; selected < count; ++selected)
        {
            const auto bounds = runtime.ConfigLayout().Section(selected, count);
            runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!selectedOnly(selected)) return false;
        }
        runtime.config_.focus.region = config_controller_region::sections;
        for (size_t selected = count; selected-- > 0;)
        {
            const auto items = runtime.ControllerConfigItems(config_controller_region::sections);
            if (!check(items.size() == count, "controller exposes all Pet settings tabs")) return false;
            runtime.ActivateControllerConfigItem(items[selected], controller_action::confirm, false);
            if (!selectedOnly(selected)) return false;
        }
        const auto page = runtime.ActiveConfigPage();
        for (size_t index = 0; index < count; ++index)
            if (!check(page.SectionContains(index, &runtime.pet_target_) == (index == count - 1),
                "the independent Pet Target module retains its own tab selection")) return false;
        return true;
    }
}
