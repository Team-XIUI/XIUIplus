#include "config/health_feedback.hpp"
#include "modules/notifications/notification_module.hpp"
#include "notifications/event_packets.hpp"
#include "ui/health_colors.hpp"

#include <cstdio>
#include <set>

namespace xiui
{
    auto RunNotificationFeedbackChecks(void) -> bool
    {
        const auto check = [](bool value, const char* label) {
            if (!value) std::fprintf(stderr, "Notification / feedback check failed: %s\n", label);
            return value;
        };
        health_feedback_settings feedback;
        const auto brightDamage = settings_store::FromIniText("[health.feedback]\ndamage_color=CFFF7668\n");
        health_feedback_settings migrated; migrated.Load(brightDamage);
        if (!check(migrated.damage_color == 0xDFF7FBFF && migrated.warning_colors,
            "legacy default damage becomes white and HP warning colors are enabled")) return false;
        migrated.Load(settings_store::FromIniText("[health.feedback]\nstyle_version=1\ndamage_color=CFFF7668\n"));
        if (!check(migrated.damage_color == 0xCFFF7668, "later explicit damage colors survive the one-time default correction")) return false;
        constexpr D3DCOLOR original = 0x80FF797D;
        const auto yellow = ui::HPWarningFill(original, .5f), orange = ui::HPWarningFill(original, .25f), red = ui::HPWarningFill(original, .24f);
        const auto channel = [](D3DCOLOR color, unsigned shift) { return (color >> shift) & 255; };
        if (!check(ui::HPWarningFill(original, .75f) == original && ui::HPWarningFill(original, 1) == original &&
            channel(yellow, 16) == channel(yellow, 8) && channel(yellow, 8) > channel(yellow, 0) &&
            channel(orange, 16) > channel(orange, 8) && channel(orange, 8) > channel(orange, 0) &&
            channel(red, 16) > channel(red, 8) && channel(red, 8) == channel(red, 0) &&
            (yellow >> 24) == (original >> 24) && (orange >> 24) == (original >> 24) && (red >> 24) == (original >> 24) &&
            ui::HPWarningFill(0xFFFFFFFF, .5f) != 0xFFFFFFFF && ui::HPWarningFill(0x80543030, .5f) != yellow,
            "HP warning fills use native threshold hues, tint white fills and retain gradient contrast/alpha")) return false;
        ui::health_feedback_state state;
        if (!check(state.Update(1, 1000, feedback).opacity == 0, "first health snapshot establishes a baseline")) return false;
        auto frame = state.Update(.6f, 1100, feedback);
        if (!check(frame.start == .6f && frame.end == 1 && !frame.healing && frame.flash > 0, "damage retains the lost portion")) return false;
        frame = state.Update(.4f, 1200, feedback);
        if (!check(frame.start == .4f && frame.end == 1, "repeated damage keeps the earlier trail")) return false;
        frame = state.Update(.7f, 1300, feedback);
        if (!check(frame.healing && frame.start == .4f && frame.end == .7f, "healing reverses the effect without stale damage")) return false;
        frame = state.Update(.7f, 2500, feedback);
        if (!check(frame.opacity == 0 && frame.flash == 0, "segments and flashes expire")) return false;
        feedback.enabled = false;
        if (!check(state.Update(.2f, 2600, feedback).opacity == 0, "disabled feedback follows resources without an effect")) return false;
        meter_animation_state meter;
        meter.Bind(41); meter.health.Update(1, 1000, {}); meter.health.Update(.4f, 1100, {});
        meter.Bind(42);
        if (!check(meter.health.Update(.1f, 1200, {}).opacity == 0 && !meter.initialized, "new actors reset both animation histories")) return false;
        feedback.enabled = true; feedback.delay = .45f; feedback.damage_color = 0x80998877; feedback.warning_colors = false;
        auto saved = settings_store::FromIniText(""); feedback.Save(saved);
        health_feedback_settings restored; restored.Load(saved);
        if (!check(restored.enabled && restored.delay == .45f && restored.damage_color == 0x80998877 && !restored.warning_colors,
            "health feedback preferences retain timing, alpha and the warning-color switch")) return false;

        notification_module legacy;
        legacy.Load(settings_store::FromIniText("[notifications]\nitems=0\nrow_spacing=13\nduration=4.5\n[layout.notifications]\nx=42\ny=65\nwidth=456\nheight=110\n[text.notifications]\nfont=2\nstroke_enabled=1\n"));
        if (!check(legacy.State().bounds.x == 42 && legacy.State().bounds.width == 456 && !*legacy.Option(2).toggle &&
            *legacy.Option(6).slider == 4.5f && *legacy.Option(5).slider == 13 && legacy.GroupFor(notification_kind::skill) == 0,
            "legacy layout, filters, lifetime and XP / skill routing stay in group one")) return false;
        saved = settings_store::FromIniText(""); legacy.Save(saved);
        notification_module copied; copied.Load(saved);
        if (!check(copied.State().bounds.x == 42 && !*copied.Option(2).toggle && copied.GroupCount() == 2, "group settings round-trip alongside legacy keys")) return false;

        notification_module groups;
        *groups.Option(27).choice = 4; groups.OnOptionChanged(27);
        std::array<int, 9> routing{};
        for (size_t kind = 0; kind < routing.size(); ++kind)
        {
            *groups.Option(32).choice = static_cast<int>(kind);
            *groups.Option(33).choice = routing[kind] = static_cast<int>(kind % 6);
            groups.Center().Post({static_cast<notification_kind>(kind), static_cast<uint32_t>(kind + 1), 1, std::to_string(kind)}, 1000);
        }
        std::set<uint64_t> seen;
        for (int group = 0; group < 6; ++group)
        {
            const auto rows = notification_renderer::Rows(groups.Center(), groups.Group(group).style, 1100, routing, group);
            for (size_t row = 0; row < rows.count; ++row)
                if (!check(seen.insert(rows.entries[row]->id).second, "routing never duplicates an active event")) return false;
        }
        if (!check(seen.size() == 9, "all notification kinds, including XP, skill and messages, have a destination")) return false;
        const auto identity = groups.Center().Entries()[1].id;
        *groups.Option(32).choice = 1; *groups.Option(33).choice = 5; groups.OnOptionChanged(33);
        if (!check(groups.Center().Find(identity)->created_at == 1000 && groups.GroupFor(notification_kind::experience) == 5,
            "moving a kind retains its active event identity and age")) return false;
        *groups.Option(27).choice = 0; groups.OnOptionChanged(27);
        if (!check(groups.GroupFor(notification_kind::experience) == 0, "inactive groups fall back to group one")) return false;
        *groups.Option(27).choice = 4; groups.OnOptionChanged(27);
        if (!check(groups.GroupFor(notification_kind::experience) == 5, "restoring a group retains the preferred assignment")) return false;
        groups.Center().Clear(); groups.Group(0).style.duration = 1; groups.Group(5).style.duration = 8;
        groups.Center().Post({notification_kind::item, 1, 1, "item"}, 1000);
        groups.Center().Post({notification_kind::experience, 0, 100, "XP"}, 1000);
        groups.Expire(2500);
        if (!check(groups.Center().Entries().size() == 1 && groups.Center().Entries()[0].event.kind == notification_kind::experience,
            "the single queue expires each kind using its group's lifetime")) return false;

        notification_center center;
        center.Post({notification_kind::party_invite, 10, 0, "Party Invitation: Alice"}, 1000);
        center.Post({notification_kind::trade_invite, 20, 0, "Trade Request: Bob"}, 1000);
        for (uint32_t i = 0; i < 40; ++i) center.Post({notification_kind::message, i, 0, "reward"}, 1000);
        if (!check(center.Entries().size() == notification_center::capacity && center.Entries()[0].event.kind == notification_kind::party_invite &&
            center.Entries()[1].event.kind == notification_kind::trade_invite, "queue overflow preserves both bounded invitation slots")) return false;
        center.ResolveTrade(99);
        if (!check(center.Entries()[1].event.kind == notification_kind::trade_invite, "an unrelated trader cannot resolve the current notice")) return false;
        center.ResolveTrade(20); center.Update(32001, 3000);
        if (!check(center.Entries().size() == 1 && center.Entries()[0].event.kind == notification_kind::party_invite, "trade resolution leaves the party invitation intact")) return false;
        center.Update(91000, 3000);
        if (!check(center.Entries().empty(), "party invitations expire after their native validity window")) return false;
        std::array<uint8_t, 12> packet{}; uint32_t sender = 20, response = 1;
        std::memcpy(packet.data() + 4, &sender, 4); std::memcpy(packet.data() + 8, &response, 4);
        const auto decoded = DecodeTradeResponse(0x22, packet);
        return check(decoded && decoded->sender == sender && decoded->kind == response && !DecodeTradeResponse(0x22, std::span{packet}.first(11)),
            "native trade resolution validates the complete packet");
    }
}
