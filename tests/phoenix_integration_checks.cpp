#include "combat/skillchain_source.hpp"
#include "combat/combat_source.hpp"
#include "modules/phantomroll/phantom_roll_data.hpp"
#include "modules/minimap/minimap_source.hpp"
#include "modules/mobinfo/mob_database.hpp"
#include "modules/treasure/treasure_module.hpp"
#include "ui/status_strip.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
    auto Check(bool value, const char* message) -> void { if (!value) throw std::runtime_error(message); }
    auto Action(uint32_t skill, uint32_t category = 3, uint32_t message = 103) -> xiui::combat_action
    {
        xiui::combat_action value{}; value.actor = 1; value.category = category; value.parameter = skill; value.target_count = 1;
        value.targets[0].id = 100; value.targets[0].result_count = 1; value.targets[0].results[0].message = message; return value;
    }
    template<typename T, size_t N> auto Put(std::array<uint8_t, N>& packet, size_t offset, T value) -> void
    { std::memcpy(packet.data() + offset, &value, sizeof(value)); }
}
namespace xiui
{
    struct phoenix_integration_test
    {
        static auto TreasureController(treasure_source& source) -> void
        {
            treasure_module module; module.SetSource(&source); module.Update(nullptr, true);
            using action = treasure_module::action;
            module.controls_ = {
                {{0, 0, 100, 30}, action::details, 0, 1, true}, {{110, 0, 40, 30}, action::lot, 0, 1, true}, {{160, 0, 40, 30}, action::pass, 0, 1, true},
                {{0, 40, 100, 30}, action::details, 1, 2, true}, {{110, 40, 40, 30}, action::lot, 1, 2, false}, {{160, 40, 40, 30}, action::pass, 1, 2, true},
                {{0, 80, 100, 30}, action::lot_all, 0, 0, true}, {{110, 80, 90, 30}, action::expand, 0, 0, true}};
            Check(module.BeginBrowse() && module.focus_ == 0, "treasure enters on the first item without activating it");
            module.HandleControllerAction(controller_action::right, false); Check(module.focus_ == 1, "Right selects Lot in the current row");
            module.HandleControllerAction(controller_action::down, false); Check(module.focus_ == 5, "Down changes rows and skips an unavailable Lot");
            module.HandleControllerAction(controller_action::up, false); Check(module.focus_ == 2, "Up retains the Pass action when available");
            module.HandleControllerAction(controller_action::down, true); module.HandleControllerAction(controller_action::down, true);
            Check(module.focus_ == 7, "direction repeats reach the footer without activating a batch action");
            const auto expanded = module.expanded_;
            module.HandleControllerAction(controller_action::confirm, true); Check(module.expanded_ == expanded, "held Confirm cannot activate treasure controls");
            module.HandleControllerAction(controller_action::confirm, false); Check(module.expanded_ != expanded, "fresh Confirm activates the selected control once");
            module.HandleControllerAction(controller_action::cancel, false); Check(!module.InputFocused(), "Cancel releases treasure focus");
            Check(module.BeginBrowse(), "treasure can be entered again"); module.Update(nullptr, false);
            Check(!module.InputFocused(), "native ownership and hidden contexts suspend treasure browsing");
        }
        static auto Map(void) -> void
        {
            minimap_source source; source.state_.zone = 121;
            std::array<uint8_t, 24> bytes{}; Put(bytes, 4, uint32_t{200}); Put(bytes, 8, uint16_t{121});
            Put(bytes, 12, 10.0f); Put(bytes, 16, 20.0f); Put(bytes, 20, 3.0f);
            source.HandlePartyPosition(bytes, 10000);
            Check(source.party_positions_[0].id == 200 && source.party_positions_[0].y == 20 && source.party_positions_[0].height == 3, "map packets retain plane/height coordinates");
            Put(bytes, 12, 15.0f); source.HandlePartyPosition(bytes, 11000);
            Check(source.party_positions_[0].x == 15 && source.party_positions_[1].id == 0, "repeat positions update one bounded cache entry");
            Put(bytes, 4, uint32_t{201}); Put(bytes, 8, uint16_t{122}); source.HandlePartyPosition(bytes, 12000);
            Put(bytes, 8, uint16_t{121}); Put(bytes, 12, std::numeric_limits<float>::quiet_NaN()); source.HandlePartyPosition(bytes, 12000);
            source.HandlePartyPosition(std::span{bytes}.first(23), 12000);
            Check(source.party_positions_[1].id == 0, "wrong-zone, non-finite and truncated map updates are rejected");
            source.Clear(); Check(source.party_positions_[0].id == 0, "zone/logout clear distant marker snapshots");
        }
    };
}

auto RunPhoenixIntegrationChecks(void) -> bool
{
    try
    {
        using namespace xiui;
        skillchain_source chains; chains.SetRules(2); chains.Observe(Action(2), 10000);
        Check(!chains.Predict(100, 3, 6, {}, 13000).Active(), "Phoenix opener hints wait more than three seconds");
        Check(chains.Predict(100, 3, 6, {}, 13001).chain == skillchain_property::impaction,
            "Shoulder Tackle to Spinning Attack retains Phoenix primary-property priority");
        chains.Clear(); chains.Observe(Action(534, 13, 110), 20000);
        Check(chains.Count() == 0, "Phoenix removes later-era Eclipse Bite resonation");
        chains.SetRules(0); chains.Observe(Action(534, 13, 110), 20000);
        Check(chains.Count() == 1, "Retail pet properties remain available");
        chains.SetRules(1); chains.Observe(Action(3), 30000);
        Check(chains.Predict(100, 3, 37, {}, 30001).chain == skillchain_property::transfixion, "Horizon overlays remain independently supported");
        const auto* healer = PhantomRollByAbility(100, 2); const auto* gallant = PhantomRollByAbility(104, 2);
        Check(healer && std::string{healer->stat} == "hMP" && PhantomRollBasePotency(*healer, 3, 75) == 10 &&
            gallant && std::string{gallant->stat} == "Dmg reflect" && PhantomRollBasePotency(*gallant, 3, 75) == 15,
            "Phoenix rolls use ToAU powers and effect semantics");
        Check(PhantomRollByAbility(105, 1)->per_level && !PhantomRollByAbility(105, 2)->per_level, "Phoenix does not inherit Horizon Chaos scaling");
        combat_source combat; combat.SetRules(2); combat.ApplyStatus(100, 4, 58, 236, 10000);
        Check(combat.Find(100) && combat.Find(100)->statuses[0].earliest == 40000 && combat.Find(100)->statuses[0].expires == 130000,
            "Phoenix Paralyze carries a base duration range");
        ui::status_strip_settings strip; strip.timers = true; strip.crowd_control_only = false;
        Check(ui::StatusTimerText(combat.Find(100)->statuses[0], 10000, strip) == "30-120" &&
            ui::StatusTimerText(combat.Find(100)->statuses[0], 50000, strip) == "<80", "ranged estimates never pretend an exact deadline");
        combat.Prune(130000);
        Check(combat.Find(100)->statuses.size() == 1 && ui::StatusTimerText(combat.Find(100)->statuses[0], 130000, strip) == "?", "passing a Phoenix base duration does not invent wear-off");
        combat.ApplyStatus(100, 4, 0, 204, 130001); Check(combat.Find(100)->statuses.empty(), "wear-off clears a ranged estimate immediately");
        combat.ApplyStatus(100, 3, 220, 236, 20000);
        Check(combat.Find(100)->statuses[0].expires == 50000 && combat.Find(100)->statuses[0].earliest == 0, "Phoenix Poison uses its thirty-second base");
        phoenix_integration_test::Map(); Check(MobIsPlaceholder(17215867), "fresh spawn-slot metadata identifies the Leaping Lizzy placeholder");

        treasure_source pool; std::array<treasure_snapshot, 10> snapshots{};
        snapshots[0] = {100, 1, 1000}; pool.Reconcile(snapshots, true, 1, 10000);
        const auto initial = pool.Entries()[0].generation; const auto token = pool.Plan(0, initial, false, 10000);
        Check(token.has_value(), "a live undecided item can be explicitly lotted");
        int sent{}; Check(pool.Queue(*token, 10000, [&](const auto&) { ++sent; }) &&
            !pool.Queue(*token, 10001, [&](const auto&) { ++sent; }) && sent == 1, "one decision queues once until reconciliation or timeout");
        pool.Reconcile(snapshots, false, 1, 11000);
        Check(pool.Count() == 1 && !pool.Plan(0, initial, true, 11000), "loading/zoning preserves entries while suspending actions");
        snapshots[0] = {101, 1, 2000}; pool.Reconcile(snapshots, true, 1, 12000);
        Check(pool.Entries()[0].generation != initial && !pool.Queue(*token, 12000, [&](const auto&) { ++sent; }), "slot reuse invalidates captured actions");
        const auto pass = pool.Plan(0, pool.Entries()[0].generation, true, 12000);
        Check(pass && pool.Queue(*pass, 12000, [&](const auto& request) { Check(request.pass, "pass remains explicit"); ++sent; }), "pass shares identity validation");
        snapshots[0].lot = 65535; pool.Reconcile(snapshots, true, 1, 13000);
        Check(!pool.Plan(0, pool.Entries()[0].generation, true, 13000), "native pass state prevents duplicate decisions");
        std::array<uint8_t, 60> solution{}; solution[20] = 0; solution[21] = 1; pool.HandlePacket(0xD3, solution, 14000);
        pool.Reconcile(snapshots, true, 1, 14001); Check(pool.Count() == 0, "resolved entries do not return from an older native sample");

        pool.Clear(); std::array<uint8_t, 28> drop{}; Put(drop, 16, uint16_t{100}); Put(drop, 24, uint32_t{5000}); drop[21] = 1;
        pool.HandlePacket(0xD2, drop, 20000); snapshots = {}; snapshots[0] = {100, 1, 5000}; pool.Reconcile(snapshots, true, 1, 20000);
        Check(pool.Entries()[0].expires == 0, "old entries without a server clock do not receive a fresh five minutes");
        drop[21] = 0; Put(drop, 24, uint32_t{6000}); pool.HandlePacket(0xD2, drop, 21000); snapshots[0].drop = 6000;
        pool.Reconcile(snapshots, true, 1, 21000); const auto expiry = pool.Entries()[0].expires; const auto generation = pool.Entries()[0].generation;
        pool.HandlePacket(0xD2, drop, 22000); pool.Reconcile(snapshots, true, 1, 22000);
        Check(expiry == 321000 && pool.Entries()[0].expires == expiry && pool.Entries()[0].generation == generation, "duplicate drops preserve the original estimate and identity");
        drop[20] = 2; drop[21] = 1; Put(drop, 24, uint32_t{5000}); pool.HandlePacket(0xD2, drop, 23000);
        snapshots[2] = {100, 1, 5000}; pool.Reconcile(snapshots, true, 1, 23000);
        Check(pool.Entries()[2].expires == 320000, "duplicate fresh drops do not shift the server clock used by old entries");
        snapshots[1] = {102, 1, 7000}; pool.Reconcile(snapshots, true, 1, 23000);
        std::vector<treasure_action> batch;
        for (size_t i = 0; i < 2; ++i) if (const auto action = pool.Plan(i, pool.Entries()[i].generation, false, 23000)) batch.push_back(*action);
        snapshots[1] = {103, 1, 8000}; pool.Reconcile(snapshots, true, 1, 23001); sent = 0;
        for (const auto& action : batch) pool.Queue(action, 23001, [&](const auto&) { ++sent; });
        Check(sent == 1, "batch decisions revalidate every captured item identity");
        phoenix_integration_test::TreasureController(pool);
        auto store = settings_store::FromIniText("[treasure]\nexpanded=0\n[layout.treasure]\nx=123\ny=234\n");
        treasure_module module; module.ApplyTextDefaults(ui::theme::CreateDefault()); module.SetSource(&pool); module.Load(store); module.Save(store);
        Check(module.State().bounds.x == 123 && !store.ReadBool("treasure", "expanded", true), "pool presentation persists without changing its anchor");
        size_t textProfiles{}; module.VisitTextSettings([&](ui::text_style&) { ++textProfiles; }); Check(textProfiles == 4, "all pool text follows global typography");
        std::puts("Phoenix integration and Treasure Pool checks passed."); return true;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Phoenix integration: %s\n", error.what()); return false; }
}
