#include "modules/status/status_effects_module.hpp"
#include "ui/status_classification.hpp"

#include <cstdio>

namespace xiui
{
    struct status_module_test
    {
        static auto Check(const bool value, const char* message) -> bool
        { if (!value) std::printf("FAIL: %s\n", message); return value; }

        static auto Run(void) -> bool
        {
            const auto now = ::GetTickCount64();
            party_source source;
            auto& player = source.members_[0];
            player.active = player.buffs_known = true; player.id = 50;
            player.buff_count = 6;
            player.buffs = {33, 2, 173, 999, 10, 33};
            player.buff_expires = {now + 60000, now + 20000, 0, 0, now + 5000, now + 30000};
            status_effects_module buffs, debuffs{true};
            buffs.SetSource(&source); debuffs.SetSource(&source);
            const auto theme = ui::theme::CreateDefault();
            buffs.ApplyTextDefaults(theme); debuffs.ApplyTextDefaults(theme);
            const auto positive = buffs.Effects(), negative = debuffs.Effects();
            if (!Check(positive.count == 4 && negative.count == 2 && positive.entries[0].id == 33 && positive.entries[1].id == 173 &&
                positive.entries[2].id == 999 && positive.entries[3].id == 33 && negative.entries[0].id == 2 && negative.entries[1].id == 10,
                "native buffs and debuffs split in source order; unknown effects and Dread Spikes remain visible")) return false;
            if (!Check(positive.entries[0].expires == player.buff_expires[0] && negative.entries[1].expires == player.buff_expires[4] &&
                positive.entries[1].expires == 0 && positive.entries[0].observed == 0,
                "local status groups preserve actual native expiration and hidden-timer values")) return false;
            buffs.settings_.columns = 2;
            if (!Check(buffs.BeginControllerBrowse() && buffs.MoveControllerBrowseHorizontal(1) && buffs.controller_effect_ == 173 &&
                buffs.MoveControllerBrowse(1) && buffs.controller_effect_ == 33 && buffs.controller_occurrence_ == 1 && buffs.ControllerBounds().width == buffs.settings_.size,
                "local status controller browsing follows measured rows and duplicate effect identities")) return false;
            std::swap(player.buffs[2], player.buffs[3]); std::swap(player.buff_expires[2], player.buff_expires[3]);
            if (!Check(buffs.ControllerFocused() && buffs.controller_effect_ == 33 && buffs.controller_occurrence_ == 1,
                "local effect focus survives other status reordering")) return false;
            player.buff_count = 5;
            if (!Check(!buffs.ControllerFocused(), "removed duplicate status cannot retain stale inspection")) return false;
            buffs.EndControllerBrowse();
            buffs.state_.bounds = {80, 110, 320, 120}; buffs.settings_.size = 40; buffs.settings_.opacity = 0.7f;
            debuffs.state_.bounds = {220, 310, 200, 60}; debuffs.settings_.size = 18; debuffs.state_.enabled = false;
            buffs.text_[1].style.shadow.enabled = buffs.text_[1].style.stroke.enabled = buffs.text_[1].style.glow.enabled = true;
            auto saved = settings_store::FromIniText(""); buffs.Save(saved); debuffs.Save(saved);
            status_effects_module loadedBuffs, loadedDebuffs{true};
            loadedBuffs.ApplyTextDefaults(theme); loadedDebuffs.ApplyTextDefaults(theme);
            loadedBuffs.Load(saved); loadedDebuffs.Load(saved);
            if (!Check(loadedBuffs.state_.bounds.x == 80 && loadedDebuffs.state_.bounds.y == 310 && loadedBuffs.settings_.size == 40 &&
                loadedDebuffs.settings_.size == 18 && loadedBuffs.settings_.columns == 2 && !loadedDebuffs.state_.enabled &&
                loadedBuffs.text_[1].style.shadow.enabled && loadedBuffs.text_[1].style.stroke.enabled && loadedBuffs.text_[1].style.glow.enabled,
                "buff and debuff positions, scale, wrapping, enable state and text effects persist separately")) return false;
            debuffs.state_.enabled = true;
            if (!Check(buffs.ShouldRender({}) && debuffs.ShouldRender({}), "native effect groups render when effects are available")) return false;
            player.buffs_known = false;
            if (!Check(!buffs.ShouldRender({}) && !debuffs.ShouldRender({}) && !buffs.BeginControllerBrowse(),
                "unknown native effect state hides groups and prevents inspection")) return false;
            buffs.preview_ = debuffs.preview_ = true;
            const auto buffPreview = buffs.Effects(true), debuffPreview = debuffs.Effects(true);
            if (!Check(buffPreview.count == 6 && debuffPreview.count == 6 && buffPreview.entries[0].expires > now &&
                !ui::IsDebuffStatus(buffPreview.entries[0].id) && ui::IsDebuffStatus(debuffPreview.entries[0].id) &&
                buffs.Effects().count == 0 && debuffs.Effects().count == 0 && !buffs.ShouldRender({}),
                "sample buffs/debuffs and countdowns exist only in edit preview, never in gameplay data")) return false;
            debuffs.SetBuffsModule(&buffs);
            buffs.settings_.size = 36; buffs.settings_.timer_placement = 2; buffs.text_[1].offset_y = -7;
            debuffs.settings_.size = 18; debuffs.text_[1].offset_y = 3;
            const auto ownBounds = debuffs.state_.bounds;
            debuffs.follow_buffs_ = true;
            if (!Check(debuffs.Layout(debuffPreview).size == 36 && debuffs.StyleModule().text_[1].offset_y == -7 &&
                debuffs.StyleModule().settings_.timer_placement == 2 && debuffs.Option(1).visible && !debuffs.Option(1).available && debuffs.preview_ &&
                debuffs.state_.bounds.x == ownBounds.x && debuffs.state_.bounds.y == ownBounds.y && debuffs.state_.bounds.width == ownBounds.width,
                "Debuffs inherits Buffs presentation while retaining its position, width, visibility and preview")) return false;
            const auto container = debuffs.Option(status_effects_module::appearance_options_start + 1);
            const auto selector = debuffs.Option(status_effects_module::text_options_start - 1);
            if (!Check(container.visible && !container.available && container.toggle == &buffs.background_ &&
                debuffs.ConfigSectionContains(2, status_effects_module::appearance_options_start + 1) &&
                selector.visible && selector.available && selector.choice == &debuffs.selected_text_,
                "linked Appearance keeps inherited values visible and Text retains its independent profile selector")) return false;
            for (int profile = 0; profile < 3; ++profile)
            {
                *selector.choice = profile;
                const auto alignment = debuffs.Option(status_effects_module::text_options_start);
                if (!Check(alignment.visible && !alignment.available && alignment.choice == &buffs.text_[profile].placement &&
                    debuffs.ConfigSectionContains(3, status_effects_module::text_options_start),
                    "every linked text profile remains visible and read-only")) return false;
            }
            buffs.settings_.size = 40;
            if (!Check(debuffs.Layout(debuffPreview).size == 40 && debuffs.settings_.size == 18 && debuffs.text_[1].offset_y == 3,
                "following tracks live Buffs changes without overwriting Debuffs preferences")) return false;
            auto linked = settings_store::FromIniText(""); buffs.Save(linked); debuffs.Save(linked);
            status_effects_module linkedBuffs, linkedDebuffs{true};
            linkedBuffs.ApplyTextDefaults(theme); linkedDebuffs.ApplyTextDefaults(theme);
            linkedBuffs.Load(linked); linkedDebuffs.Load(linked); linkedDebuffs.SetBuffsModule(&linkedBuffs);
            if (!Check(linkedDebuffs.follow_buffs_ && linkedDebuffs.preview_ && linkedBuffs.preview_ && linkedDebuffs.settings_.size == 18 &&
                linkedDebuffs.Layout(debuffPreview).size == 40, "preview/follow options persist without copying inherited values into saved Debuffs settings")) return false;
            debuffs.follow_buffs_ = false;
            if (!Check(debuffs.Layout(debuffPreview).size == 18 && debuffs.StyleModule().text_[1].offset_y == 3 && debuffs.Option(1).visible && debuffs.Option(1).available &&
                debuffs.Option(status_effects_module::appearance_options_start + 1).toggle == &debuffs.background_ &&
                debuffs.Option(status_effects_module::text_options_start).choice == &debuffs.text_[2].placement,
                "turning follow off restores the independent Debuffs presentation")) return false;
            {
                ui::design_system design;
                party_source timedSource;
                auto& local = timedSource.members_[0]; local.active = local.buffs_known = true; local.id = 50;
                local.buff_count = 4; local.buffs = {33, 41, 2, 10};
                local.buff_expires = {now + 3600000, now + 21420000, now + 3600000, now + 21420000};
                for (const bool isDebuff : {false, true})
                {
                    status_effects_module timed{isDebuff}; timed.SetSource(&timedSource); timed.ApplyTextDefaults(theme);
                    timed.design_ = &design; timed.State().bounds.width = 160;
                    timed.text_[1].style.scale = 12 / ui::text_base_pixels;
                    const auto effects = timed.Effects(); const auto compactLayout = timed.Layout(effects, now);
                    timed.text_[1].style.scale = 38 / ui::text_base_pixels;
                    const auto large = timed.Layout(effects, now);
                    const auto caption = ui::StatusTimerProfile(timed.text_[1], timed.settings_);
                    if (!Check(large.count == 2 && large.cell_width > compactLayout.cell_width && large.timer_height > compactLayout.timer_height,
                        "Buffs and Debuffs honor configured timer text size for hour-long effects")) return false;
                    for (size_t i = 0; i < large.count; ++i)
                    {
                        const auto icon = large.Icon(i);
                        const auto text = ui::StatusTimerText(effects.entries[i], now, timed.settings_);
                        const auto label = ui::StatusTimerBounds(icon, timed.settings_, timed.text_[1], large.cell_width, large.timer_height);
                        if (!Check(design.MeasureText(text.c_str(), caption.style) <= label.width && design.TextHeight(caption.style) <= label.height,
                            "native multi-hour labels fit their measured cells without shrinking their text profile")) return false;
                        const auto hit = ui::StatusStripHit(effects.Effects(), timed.StripBounds(), timed.settings_, icon.x + icon.width / 2, icon.y + icon.height / 2, &large);
                        if (!Check(hit && hit->status.id == effects.entries[i].id, "hover uses the same font-aware status layout as rendering")) return false;
                    }
                    if (!Check(timed.BeginControllerBrowse() && timed.ControllerBounds().x == large.Icon(0).x && timed.ControllerBounds().y == large.Icon(0).y,
                        "controller inspection follows the measured status icon positions")) return false;
                }
            }
            return true;
        }
    };

    auto RunStatusModuleChecks(void) -> bool { return status_module_test::Run(); }
}
