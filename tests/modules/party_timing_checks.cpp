#include "modules/party/party_module.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace xiui
{
    struct party_timing_test
    {
        static auto Check(const bool value, const char* message) -> bool
        { if (!value) std::fprintf(stderr, "Party timing check failed: %s\n", message); return value; }
        static auto Near(const float a, const float b) -> bool { return std::abs(a - b) < 0.02f; }

        static auto Observations(void) -> bool
        {
            std::array<party_member, 3> members{};
            for (size_t index = 0; index < members.size(); ++index)
            {
                auto& member = members[index];
                member.id = static_cast<uint32_t>(100 + index); member.index = static_cast<uint32_t>(200 + index); member.zone = 1;
                member.active = member.in_zone = member.entity_valid = member.status_known = true;
                member.hp = 400; member.hp_max = 1000; member.hp_percent = 40;
                member.mp = 40; member.mp_max = 400; member.mp_percent = 10; member.tp = 200; member.main_job = 3;
            }
            members[1].status = 1; members[2].status = 33;
            combat_timing_source local;
            local.attack_.Begin(100, 4);
            local.recovery_.Update({400, 1000, 40, 400, 200, true}, 100);
            party_timing_source source;
            uint64_t now = 100000;
            const auto poll = [&] { source.Update(members, 100, &local, now); };
            const auto advance = [&](const uint64_t until) { while (now < until) { now += 100; poll(); } };
            const auto action = [&](const uint32_t actor, const uint16_t category, const uint32_t target = 0) {
                combat_action event{}; event.actor = actor; event.category = category; event.parameter = 1;
                if (target != 0) { event.target_count = 1; event.targets[0].id = target; }
                source.Observe(event, now);
            };
            poll();
            if (!Check(Near(source.Swing(100, 100).seconds, local.Player(100).seconds) &&
                Near(source.Resting(100, 100).seconds, local.Recovery().Predict(100).seconds), "player row reuses the existing clocks")) return false;
            if (!Check(!source.Swing(101, 100).active && !source.Resting(102, 100).active, "joining mid-action waits for observations")) return false;
            advance(101000); action(101, 1);
            advance(103000);
            members[2].hp += 10; members[2].mp += 12; members[2].tp -= 10;
            // A rounded maximum can change without a real equipment change.
            members[2].hp_max = 1025;
            now += 100; poll();
            if (!Check(source.Resting(102, now / 1000.0).active && source.Resting(102, now / 1000.0).known,
                "observed remote recovery synchronizes despite rounded maximum drift")) return false;
            advance(105000); action(101, 1);
            advance(107000); action(999, 4, 102);
            members[2].hp += 50; members[2].mp += 100; now += 100; poll();
            if (!Check(Near(source.Resting(102, 107.1).seconds, 5), "an incoming heal cannot restart the resting ticker")) return false;
            advance(109000); action(101, 1);
            if (!Check(!source.Swing(101, 109).active, "three rounds do not invent a learned swing interval")) return false;
            advance(113000); action(101, 1);
            if (!Check(source.Swing(101, 113).active && source.Swing(101, 113).learned && Near(source.Swing(101, 113).interval, 4),
                "four observed rounds establish a remote swing interval")) return false;
            std::swap(members[0], members[1]); now += 100; poll();
            if (!Check(source.Swing(101, now / 1000.0).active, "remote clocks follow identity across row reordering")) return false;
            advance(114000); action(101, 8);
            advance(115000);
            if (!Check(source.Swing(101, 115).state == "Casting" && Near(source.Swing(101, 115).seconds, 3), "casting holds a learned remote swing")) return false;
            action(101, 4);
            if (!Check(Near(source.Swing(101, 115).seconds, 5), "cast completion includes the action tail")) return false;
            members[0].status = 0; now += 100; poll();
            if (!Check(!source.Swing(101, now / 1000.0).active, "disengaging clears remote swings")) return false;
            members[2].hp = 1000; members[2].mp = 400; members[2].hp_percent = members[2].mp_percent = 100;
            now += 100; poll();
            if (!Check(source.Full(102), "remote completion follows actual resource percentages")) return false;
            members[2].in_zone = false; now += 100; poll();
            if (!Check(!source.Resting(102, now / 1000.0).active, "zone departure removes a remote resting ticker")) return false;
            members[2].in_zone = true; members[2].status_known = false; now += 100; poll();
            if (!Check(!source.Resting(102, now / 1000.0).active, "unavailable entity status cannot create a timer")) return false;
            members[2].status_known = true; members[2].id = 202; now += 100; poll();
            if (!Check(!source.Resting(102, now / 1000.0).active && !source.Resting(202, now / 1000.0).active,
                "a replacement member cannot inherit the previous clock")) return false;
            source.Clear();
            return Check(!source.Swing(100, 116).active && !source.Resting(100, 116).active, "clearing releases local and remote timing");
        }

        static auto LayoutAndSettings(void) -> bool
        {
            party_module party;
            ui::design_system design;
            party_member member; member.preview = member.in_zone = true; member.slot = 0; member.status = 33;
            member.main_job = 3; member.mp = 40; member.mp_max = 400;
            for (int layout = 0; layout < 3; ++layout)
            {
                party.layout_ = layout;
                for (size_t kind = 0; kind < 2; ++kind)
                    for (int display = 1; display <= 2; ++display)
                    {
                        member.status = kind == 0 ? 1 : 33;
                        party.tickers_.tickers[kind].display = display;
                        const auto slots = party.ResourceSlots(member);
                        const auto entry = party.LayoutEntry({0, 0, 360, 0}, true, slots);
                        auto plainSlots = slots; plainSlots.swing = plainSlots.resting = false;
                        const auto plain = party.LayoutEntry({0, 0, 360, 0}, true, plainSlots);
                        const auto& hp = entry.resources.bars[0];
                        const auto& at = entry.tickers[kind];
                        if (display == 2)
                        {
                            const auto track = ui::LayoutMeterTrack(hp, party.meters_[0]);
                            if (!Check(Near(at.x, track.x) && Near(hp.width, plain.resources.bars[0].width) &&
                                Near(entry.resources.bars[1].y, plain.resources.bars[1].y) &&
                                Near(entry.resources.bars[2].y, plain.resources.bars[2].y),
                                "text-only tickers retain HP width and do not move MP or TP vertically")) return false;
                            if (layout == 0)
                            {
                                if (!Check(at.y >= track.y + track.height, "horizontal layouts keep text-only tickers below HP")) return false;
                            }
                            else
                            {
                                const auto& mp = entry.resources.bars[1];
                                if (!Check(Near(at.y + at.height * 0.5f, mp.y + mp.height * 0.5f) &&
                                    at.x + at.width < mp.x && Near(entry.height, plain.height),
                                    "both text-only tickers share the existing MP row on its left without adding entry height")) return false;
                            }
                        }
                        else
                        {
                            if (!Check(at.x > hp.x + hp.width && Near(at.y + at.height * 0.5f, hp.y + hp.height * 0.5f) &&
                                Near(hp.width, plain.resources.bars[0].width) &&
                                Near(entry.resources.bars[1].width, plain.resources.bars[1].width) &&
                                Near(entry.resources.bars[2].width, plain.resources.bars[2].width),
                                "bar countdowns sit outside HP without shortening any resource bar")) return false;
                            if (!Check(Near(entry.height, plain.height) && Near(entry.resources.bars[1].y, plain.resources.bars[1].y) &&
                                Near(entry.resources.bars[2].y, plain.resources.bars[2].y),
                                "inline countdowns add no row or shift MP and TP")) return false;
                        }
                        auto profile = party.tickers_.tickers[kind].text; profile.style.tabular_numbers = true;
                        if (!Check(design.MeasureText("~99.9s", profile.style) <= at.width,
                            "countdown gutter fits the selected font and stable numeric cells")) return false;
                        party.tickers_.tickers[kind].display = 0;
                    }
                member.status = 33;
                party.tickers_.tickers[1].display = 0;
                const auto inside = party.LayoutEntry({0, 0, 360, 0}, true, party.ResourceSlots(member));
                auto plainSlots = party.ResourceSlots(member); plainSlots.resting = false;
                const auto plain = party.LayoutEntry({0, 0, 360, 0}, true, plainSlots);
                const auto hp = ui::LayoutMeterTrack(inside.resources.bars[0], party.meters_[0]);
                if (!Check(Near(inside.height, plain.height) && inside.tickers[1].y >= hp.y &&
                    inside.tickers[1].y + inside.tickers[1].height <= hp.y + hp.height,
                    "bar-only resting strips stay inside HP without increasing the row height")) return false;
                member.status = 1;
                const auto swing = party.LayoutEntry({0, 0, 360, 0}, true, party.ResourceSlots(member));
                if (!Check(swing.tickers[0].width > 0 && Near(swing.tickers[0].x, ui::LayoutMeterTrack(swing.resources.bars[0], party.meters_[0]).x),
                    "swing strips use HP even while TP is visible")) return false;
                member.status = 33;
            }
            party.job_icons_ = false; party.tp_ = false; party.layout_ = party_module::bars_mp_below_tp_left;
            party.bar_widths_[0] = 171; party.bar_widths_[1] = 122;
            party.bar_positions_[1] = party_module::position_right; party.bar_offsets_y_[1] = -5;
            party.meters_[0].height = 12; party.meters_[1].height = 8;
            for (size_t kind = 0; kind < 2; ++kind)
                for (int display = 1; display <= 2; ++display)
                {
                    member.status = kind == 0 ? 1 : 33;
                    party.tickers_.tickers[kind].display = 0;
                    const auto before = party.LayoutEntry({0, 0, 171, 0}, true, party.ResourceSlots(member));
                    party.tickers_.tickers[kind].display = display;
                    const auto after = party.LayoutEntry({0, 0, 171, 0}, true, party.ResourceSlots(member));
                    const auto& mpBefore = before.resources.bars[1];
                    const auto& mpAfter = after.resources.bars[1];
                    if (!Check(Near(mpBefore.x, mpAfter.x) && Near(mpBefore.y, mpAfter.y) && Near(mpBefore.width, mpAfter.width) &&
                        Near(before.height, after.height) && Near(before.resources.bars[0].width, after.resources.bars[0].width),
                        "both ticker displays preserve a short right-aligned MP bar and full HP width")) return false;
                    if (!Check(display == 1 ? after.tickers[kind].x > after.resources.bars[0].x + after.resources.bars[0].width :
                        after.tickers[kind].x + after.tickers[kind].width < mpAfter.x,
                        "bar text sits outside HP while text-only sits left of MP")) return false;
                    party.tickers_.tickers[kind].display = 0;
                }
            party.tickers_.tickers[0].display = party.tickers_.tickers[1].display = 1;
            const auto preview = party.LayoutRows(6, true);
            const auto visual = party.VisualBounds(true);
            if (!Check(preview.right_overflow > 0 && Near(preview.bounds.width, party.LayoutContainer().width) &&
                visual.x + visual.width >= preview.bounds.x + preview.bounds.width + preview.right_overflow,
                "movement bounds include external countdowns without enlarging their container")) return false;
            member.status = 33;
            const ui::rect fill{10, 20, 40, 6};
            const auto clipped = ui::LayoutMeterStrip(fill, 12, 24);
            if (!Check(clipped.y >= fill.y && clipped.y + clipped.height <= fill.y + fill.height,
                "large height and inset settings cannot put a strip outside HP")) return false;
            party.tickers_.tickers[1].enabled = false;
            if (!Check(!party.ResourceSlots(member).resting, "disabled tickers reserve no entry space")) return false;
            party.tickers_.tickers[0].display = 2; party.tickers_.tickers[0].other_members = false;
            party.tickers_.tickers[0].color = 0x8F123456;
            party.tickers_.tickers[0].text.style.letter_spacing = 1.5f;
            auto settings = settings_store::FromIniText(""); party.Save(settings);
            party_module restored; restored.Load(settings.Preferences(""));
            const auto& ticker = restored.tickers_.tickers[0];
            if (!Check(ticker.display == 2 && !ticker.other_members && ticker.color == 0x8F123456 &&
                ticker.text.style.letter_spacing == 1.5f && !restored.tickers_.tickers[1].enabled,
                "independent ticker styles, text effects and color alpha survive profiles")) return false;
            party_module alliance{1}; alliance.Load(settings);
            if (!Check(alliance.tickers_.tickers[0].other_members && alliance.tickers_.tickers[1].enabled,
                "party ticker preferences do not overwrite independent alliance settings")) return false;
            for (size_t index = 0; index < party_timing_settings::option_count; ++index)
                if (!Check(party.ConfigSectionContains(party_module::tickers_section, party_module::ticker_options_start + index) &&
                    !party.ConfigSectionContains(party_module::text_section, party_module::ticker_options_start + index),
                    "ticker controls stay in their own settings section")) return false;
            return true;
        }
    };

    auto RunPartyTimingChecks(void) -> bool
    { return party_timing_test::Observations() && party_timing_test::LayoutAndSettings(); }
}
