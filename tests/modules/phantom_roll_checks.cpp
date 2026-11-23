#include "modules/phantomroll/phantom_roll_data.hpp"
#include "modules/phantomroll/phantom_roll_tracker.hpp"

#include <cmath>
#include <iostream>

namespace
{
    auto Roll(const uint32_t ability, const uint32_t total) -> xiui::combat_action
    {
        xiui::combat_action action{};
        action.actor = 123; action.category = 6; action.parameter = ability; action.target_count = 1;
        action.targets[0].id = 123; action.targets[0].parameter = total; action.targets[0].result_count = 1;
        return action;
    }
}

auto RunPhantomRollChecks(void) -> bool
{
    const auto check = [](const bool value, const char* label) {
        if (!value) std::cerr << "Phantom Roll: " << label << '\n';
        return value;
    };
    bool okay = true;
    xiui::phantom_roll_tracker tracker;
    okay &= check(!tracker.Observe(Roll(98, 5), 999, 1000), "reject unrelated actor");
    auto wrong = Roll(98, 5); wrong.targets[0].id = 999;
    okay &= check(!tracker.Observe(wrong, 123, 1000), "require local result");
    okay &= check(!tracker.Observe(Roll(98, 0), 123, 1000) && !tracker.Observe(Roll(98, 18), 123, 1000), "reject invalid totals");
    okay &= check(tracker.Observe(Roll(98, 5), 123, 1000), "initial roll");
    okay &= check(!tracker.Observe(Roll(98, 5), 123, 1001), "duplicate action");
    tracker.Reconcile({}, 3999);
    okay &= check(tracker.Seats()[0].total == 5 && !tracker.Seats()[0].observed, "short snapshot-arrival grace");
    const std::array<xiui::phantom_roll_status, 2> native{{{310, 501000}, {308, 46000}}};
    tracker.Reconcile(native, 4000);
    okay &= check(tracker.Seats()[0].expires == 501000 && tracker.Seats()[0].observed, "native deadline replaces estimate");
    okay &= check(tracker.Observe(Roll(123, 8), 123, 5000) && tracker.Seats()[0].total == 8 && tracker.DoubleUpUntil() == 46000, "Double-Up changes latest total without rearming window");
    okay &= check(tracker.Observe(Roll(123, 12), 123, 6000) && tracker.Seats()[0].busted && tracker.LatestOpen() == nullptr, "bust closes latest roll");
    const std::array<xiui::phantom_roll_status, 1> bust{{{309, 306000}}};
    tracker.Reconcile(bust, 6100);
    okay &= check(tracker.Observe(Roll(98, 4), 123, 7000), "same named roll can occupy the other seat");
    const std::array<xiui::phantom_roll_status, 3> sameName{{{309, 306000}, {310, 507000}, {308, 52000}}};
    tracker.Reconcile(sameName, 7100);
    okay &= check(tracker.Seats()[0].busted && tracker.Seats()[1].total == 4, "live and busted copies remain distinct");
    okay &= check(tracker.Observe(Roll(123, 13), 123, 8000), "second seat bust");
    const std::array<xiui::phantom_roll_status, 2> twoBusts{{{309, 308000}, {309, 306000}}};
    tracker.Reconcile(twoBusts, 8100);
    okay &= check(tracker.Seats()[0].expires == 306000 && tracker.Seats()[1].expires == 308000, "distinct bust clocks match existing seats");
    okay &= check(!tracker.Observe(Roll(105, 4), 123, 9000), "never evict an unrelated busted seat");
    tracker.Reconcile({}, 10000);
    okay &= check(!tracker.Seats()[0].Present() && !tracker.Seats()[1].Present(), "authoritative removals clear seats");
    tracker.Reconcile(native, 11000);
    okay &= check(tracker.Seats()[0].total == 0 && tracker.Seats()[0].ability == 98 && tracker.LatestOpen() == nullptr, "buff-only recovery never invents a total or Double-Up target");
    okay &= check(!tracker.Observe(Roll(123, 8), 123, 12000), "unknown history cannot assign Double-Up to a guessed roll");
    tracker.Clear(); tracker.Observe(Roll(98, 5), 123, 1000); tracker.Reconcile({}, 4000);
    okay &= check(!tracker.Seats()[0].Present(), "unconfirmed action expires after grace");
    tracker.Clear(); tracker.Observe(Roll(98, 5), 123, 1000);
    okay &= check(!tracker.Observe(Roll(123, 8), 123, 46000), "closed Double-Up window rejects update");
    const auto* retail = xiui::PhantomRollByAbility(105, xiui::server_rules::retail);
    const auto* horizon = xiui::PhantomRollByAbility(105, xiui::server_rules::horizon);
    okay &= check(retail != nullptr && horizon != nullptr && xiui::PhantomRollBasePotency(*retail, 4, 75) == 25 &&
        xiui::PhantomRollBasePotency(*horizon, 4, 75) == 92 && xiui::PhantomRollBasePotency(*horizon, 4, 50) == 61, "retail and Horizon base tables with known level");
    okay &= check(!xiui::PhantomRollBasePotency(*horizon, 4, 0) && !xiui::PhantomRollBasePotency(*retail, 0, 75), "unavailable inputs stay unknown");
    const std::array<uint8_t, 6> withDrk{17, 8, 0, 0, 0, 0}, withoutDrk{17, 3, 0, 0, 0, 0};
    okay &= check(xiui::PhantomRollHasJobBonus(*retail, withDrk) && !xiui::PhantomRollHasJobBonus(*retail, withoutDrk) &&
        xiui::PhantomRollBasePotency(*retail, 4, 75, true) == 35 && xiui::PhantomRollPotencyText(retail, 4, 75, false, true) == "+35% ATK (job bonus +10%)" &&
        xiui::PhantomRollPotencyText(retail, 4, 75, false, false) == "Base +25% ATK", "a party DRK adds Chaos Roll's job bonus");
    okay &= check(xiui::PhantomRollPotencyText(retail, 11, 75, true, true) == "Bust penalty: unknown" && !xiui::PhantomRollHasJobBonus(*horizon, withDrk),
        "busts and Horizon rolls get no job bonus");
    const auto* phoenixGallant = xiui::PhantomRollByAbility(104, xiui::server_rules::phoenix);
    const auto* corsair = xiui::PhantomRollByAbility(114, xiui::server_rules::phoenix);
    const std::array<uint8_t, 6> withPld{17, 7, 0, 0, 0, 0};
    okay &= check(phoenixGallant != nullptr && xiui::PhantomRollHasJobBonus(*phoenixGallant, withPld) &&
        xiui::PhantomRollBasePotency(*phoenixGallant, 3, 75, true) == 25 && corsair != nullptr && !xiui::PhantomRollHasJobBonus(*corsair, withDrk),
        "Phoenix era job bonuses, and Corsair's Roll has none");
    okay &= check(xiui::PhantomRollBasePotency(*retail, 4, 75, false, 37) == 13 && xiui::PhantomRollBasePotency(*retail, 4, 75, false, 75) == 25 &&
        xiui::PhantomRollPotencyText(retail, 4, 75, false, true, 37) == "+18% ATK (job bonus +5%)", "a subjob Corsair's roll scales by its level over the recipient's");
    const auto* companion = xiui::PhantomRollByAbility(304, xiui::server_rules::retail);
    okay &= check(companion != nullptr && !xiui::PhantomRollBasePotency(*companion, 2, 99), "unsupported potency stays unknown");
    okay &= check(xiui::PhantomBustChance(5) == 0 && std::abs(xiui::PhantomBustChance(8) - 0.5f) < 0.001f && xiui::PhantomBustChance(11) == 1, "ordinary die risk boundaries");
    okay &= check(xiui::PhantomRolls(xiui::server_rules::retail).size() == 31 && xiui::PhantomRolls(xiui::server_rules::horizon).size() == 31, "both rule sets retain all roll identities");
    {
        using status = xiui::phantom_roll_status;
        using member = xiui::phantom_roll_member;
        const std::array<uint32_t, 2> ids{200, 201};
        const std::array<uint16_t, 2> holding{40, 310};
        const std::array<uint16_t, 1> without{40};
        const auto& rolls = tracker.PartyRolls();
        const auto shared = [&](const uint32_t actor, const uint32_t ability, const uint32_t total) {
            auto action = Roll(ability, total);
            action.actor = actor; action.target_count = 3;
            action.targets[0].id = actor;
            for (size_t i = 0; i < ids.size(); ++i) { action.targets[i + 1].id = ids[i]; action.targets[i + 1].parameter = total; action.targets[i + 1].result_count = 1; }
            return action;
        };
        const std::array<member, 2> both{{{200, true, holding}, {201, true, holding}}};
        const std::array<member, 2> away{{{200, false, {}}, {201, false, {}}}};
        const std::array<member, 2> none{{{200, true, without}, {201, true, without}}};
        tracker.Clear();
        okay &= check(tracker.Observe(shared(123, 98, 5), 123, 1000, ids) && rolls[0].status == 310 && !tracker.PartyVisible(rolls[0]), "a shared roll shows once, in its seat");
        const std::array<status, 1> first{{{310, 301000}}};
        tracker.Reconcile(first, 2000, both);
        okay &= check(rolls[0].expires == 301000 && !rolls[0].estimated && rolls[0].holders == 2, "the party copy takes the seat's native deadline");
        okay &= check(tracker.Observe(Roll(105, 4), 123, 60000, ids), "second roll away from the party");
        const std::array<status, 2> second{{{310, 301000}, {317, 360000}}};
        tracker.Reconcile(second, 60100, both);
        okay &= check(tracker.Observe(Roll(109, 6), 123, 120000, ids), "third roll replaces the oldest seat");
        const std::array<status, 2> third{{{317, 360000}, {321, 420000}}};
        tracker.Reconcile(third, 120100, both);
        okay &= check(tracker.PartyVisible(rolls[0]) && rolls[0].total == 5 && rolls[0].expires == 301000 && !rolls[0].on_self,
            "a roll the party still holds keeps its timer after leaving the local seats");
        tracker.Reconcile(third, 130000, away);
        okay &= check(rolls[0].Present() && rolls[0].expires == 301000, "unavailable member statuses keep a running timer");
        tracker.Reconcile(third, 302000, away);
        okay &= check(!rolls[0].Present(), "an unconfirmed party roll ends with its timer");
        tracker.Clear();
        tracker.Observe(shared(123, 98, 5), 123, 1000, ids); tracker.Reconcile(first, 2000, both);
        tracker.Observe(Roll(105, 4), 123, 60000, ids); tracker.Observe(Roll(109, 6), 123, 120000, ids);
        tracker.Reconcile(third, 120100, none);
        okay &= check(!rolls[0].Present(), "a roll no member holds is removed");
        tracker.Clear();
        tracker.Observe(shared(123, 98, 5), 123, 1000, ids); tracker.Reconcile(first, 2000, both);
        tracker.Observe(Roll(105, 4), 123, 60000, ids); tracker.Observe(Roll(109, 6), 123, 120000, ids);
        tracker.Reconcile(third, 120100, both);
        tracker.Observe(Roll(98, 3), 123, 180000, ids);
        const std::array<status, 2> again{{{321, 420000}, {310, 480000}}};
        tracker.Reconcile(again, 180100, both);
        okay &= check(tracker.PartyVisible(rolls[0]) && rolls[0].total == 5 && rolls[0].expires == 301000, "rerolling alone leaves the party's older copy on its own clock");
        tracker.Clear();
        okay &= check(tracker.Observe(shared(200, 112, 4), 123, 1000, ids) && rolls[0].roller == 200 && rolls[0].estimated && rolls[0].expires == 301000 &&
            tracker.PartyVisible(rolls[0]), "another Corsair's roll is estimated from its action");
        okay &= check(tracker.Observe(shared(200, 123, 9), 123, 9000, ids) && rolls[0].total == 9 && rolls[0].expires == 301000, "their Double-Up changes the total only");
        okay &= check(!tracker.Observe(shared(999, 105, 4), 123, 9000, ids) && !rolls[1].Present(), "rolls from outside the party are ignored");
        tracker.Reconcile({}, 20000, none);
        okay &= check(!rolls[0].Present(), "an estimated roll no member holds is removed");
        const std::array<status, 3> extra{{{317, 360000}, {321, 420000}, {324, 500000}}};
        tracker.Reconcile(extra, 21000, both);
        okay &= check(tracker.Seats()[0].status == 317 && tracker.Seats()[1].status == 321 && rolls[0].status == 324 && rolls[0].expires == 500000 && rolls[0].on_self == false &&
            rolls[1].status == 310 && rolls[1].total == 0 && rolls[1].expires == 0, "rolls beyond the two seats and member-only rolls are adopted");
        tracker.Reconcile(extra, 22000, both);
        okay &= check(rolls[0].on_self && rolls[1].holders == 2 && tracker.PartyVisible(rolls[0]) && tracker.PartyVisible(rolls[1]), "adopted rolls report who holds them");
    }
    tracker.Clear(); okay &= check(tracker.DoubleUpUntil() == 0 && !tracker.Seats()[0].Present(), "lifecycle clears roll state");
    return okay;
}
