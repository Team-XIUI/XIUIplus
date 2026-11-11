#include "combat/skillchain_source.hpp"
#include "combat/combat_source.hpp"
#include "combat/server_rules.hpp"
#include "modules/phantomroll/phantom_roll_data.hpp"
#include "modules/minimap/minimap_source.hpp"
#include "modules/mobinfo/mob_database.hpp"
#include "modules/treasure/treasure_module.hpp"
#include "ui/status_strip.hpp"
#include <cstdio>
#include <cmath>
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
        static auto TextVisibility(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            module_text_profile profile{theme.caption_text};
            const auto toggle = TextProfileOption(text_profile_option_count - 1, profile);
            Check(profile.visible && toggle.toggle == &profile.visible, "text roles start visible and expose a shared toggle");
            *toggle.toggle = false; profile.offset_x = 9; profile.style.glow.enabled = true;
            auto store = settings_store::FromIniText(""); SaveTextProfile(store, "text.test", profile);
            const auto loaded = LoadTextProfile(store, "text.test", module_text_profile{theme.caption_text});
            const auto tinted = TintTextProfile(loaded, 0xFF80FFFF);
            Check(!loaded.visible && !tinted.visible && loaded.offset_x == 9 && loaded.style.glow.enabled,
                "hidden text retains its style and visibility through saving and tinting");
            Check(LoadTextProfile(store, "text.legacy", {}).visible, "old profiles keep visible text");
            ui::design_system design; const module_render_context context{design, nullptr, true};
            Check(context.TextBounds("Hidden", {0, 0, 100, 30}, {0, 0, 100, 30}, loaded).width == 0,
                "hidden text has no hit bounds");

            treasure_module pool; pool.ApplyTextDefaults(theme); pool.State().bounds = {10, 20, 440, 200};
            pool.Render({}, context);
            const float fullHeight = pool.State().bounds.height;
            for (size_t row = 0; row < 3; ++row)
            {
                const auto& name = pool.controls_[row * 3].bounds;
                const auto& lot = pool.controls_[row * 3 + 1].bounds;
                const auto& pass = pool.controls_[row * 3 + 2].bounds;
                Check(lot.width == pass.width && lot.y == pass.y && lot.x + lot.width < pass.x &&
                    std::abs(name.y + name.height / 2 - lot.y - lot.height / 2) < 0.01f,
                    "Lot and Pass have equal sizes and share the item row's center");
            }
            const auto content = ui::container_renderer::ContentBounds(pool.State().bounds, pool.container_);
            for (size_t i = 9; i < pool.controls_.size(); ++i)
            {
                const auto header = pool.controls_[i].bounds;
                Check(header.x >= content.x && header.x + header.width <= content.x + content.width + 0.01f &&
                    header.y + header.height <= pool.controls_[0].bounds.y, "all four actions fit in the title bar above the items");
                if (i >= 11) Check(header.width == header.height, "Collapse and Details use square icon buttons");
            }
            pool.text_[0].visible = false; pool.Render({}, context);
            Check(pool.State().bounds.height <= fullHeight && pool.controls_.size() == 13, "hidden title retains the header actions");
            const float noTitle = pool.State().bounds.height;
            pool.text_[2].visible = false; pool.Render({}, context);
            Check(pool.State().bounds.height == noTitle && pool.text_[4].visible, "timers remain independently visible without details");
            pool.text_[4].visible = false; pool.Render({}, context);
            Check(pool.State().bounds.height < noTitle, "hidden secondary content releases its row");
            pool.text_[3].style.scale = 3; pool.Render({}, context);
            for (size_t i = 9; i < pool.controls_.size(); ++i)
                Check(pool.controls_[i].bounds.x >= content.x && pool.controls_[i].bounds.x + pool.controls_[i].bounds.width <= content.x + content.width + 0.01f &&
                    pool.controls_[i].bounds.y + pool.controls_[i].bounds.height <= pool.controls_[0].bounds.y, "large header controls wrap inside the window");
            pool.Save(store); treasure_module restored; restored.ApplyTextDefaults(theme); restored.Load(store);
            Check(!restored.text_[0].visible && !restored.text_[2].visible && !restored.text_[4].visible &&
                restored.State().bounds.x == 10 && restored.State().bounds.y == 20, "pool toggles and anchor survive reload");
            auto legacy = settings_store::FromIniText("[text.treasure.item]\nscale=0.8\n[text.treasure.details]\nscale=0.8\n");
            restored.Load(legacy); restored.Save(legacy); const float subtextScale = restored.text_[2].style.scale;
            restored.Load(legacy);
            Check(std::abs(subtextScale - 0.7f) < 0.001f && restored.text_[2].style.scale == subtextScale,
                "old uniform subtext becomes smaller once without repeated migration");
        }
        static auto TreasureController(treasure_source& source) -> void
        {
            treasure_module module; module.SetSource(&source); module.Update(nullptr, true);
            using action = treasure_module::action;
            module.controls_ = {
                {{0, 40, 100, 30}, action::details, 0, 1, true}, {{110, 43, 40, 24}, action::lot, 0, 1, true}, {{160, 43, 40, 24}, action::pass, 0, 1, true},
                {{0, 80, 100, 30}, action::details, 1, 2, true}, {{110, 83, 40, 24}, action::lot, 1, 2, false}, {{160, 83, 40, 24}, action::pass, 1, 2, true},
                {{0, 0, 100, 30}, action::lot_all, 0, 0, true}, {{110, 0, 90, 30}, action::expand, 0, 0, true}};
            Check(module.BeginBrowse() && module.focus_ == 0, "treasure enters on the first item without activating it");
            module.HandleControllerAction(controller_action::right, false); Check(module.focus_ == 1, "Right selects Lot in the current row");
            module.HandleControllerAction(controller_action::down, false); Check(module.focus_ == 5, "Down changes rows and skips an unavailable Lot");
            module.HandleControllerAction(controller_action::up, false); Check(module.focus_ == 2, "Up retains the Pass action when available");
            module.HandleControllerAction(controller_action::up, true);
            Check(module.focus_ == 7, "Up reaches title-bar controls without activating a batch action");
            const auto expanded = module.expanded_;
            module.HandleControllerAction(controller_action::confirm, true); Check(module.expanded_ == expanded, "held Confirm cannot activate treasure controls");
            module.HandleControllerAction(controller_action::confirm, false); Check(module.expanded_ != expanded, "fresh Confirm activates the selected control once");
            module.HandleControllerAction(controller_action::cancel, false); Check(!module.InputFocused(), "Cancel releases treasure focus");
            Check(module.BeginBrowse(), "treasure can be entered again"); module.Update(nullptr, false);
            Check(!module.InputFocused(), "native ownership and hidden contexts suspend treasure browsing");
        }
        static auto TreasureReconciliation(void) -> void
        {
            treasure_source source;
            treasure_module module; module.SetSource(&source);
            std::array<treasure_snapshot, treasure_source::capacity> slots{};
            for (size_t slot = 0; slot < slots.size(); ++slot) slots[slot] = {100, 1, 1000 + static_cast<uint32_t>(slot)};
            slots[0].members[0] = {1, 0};
            source.Reconcile(slots, true, 1, 10000);
            const auto generation = source.Entries()[0].generation;
            const auto captured = source.Plan(0, generation, false, 10000);
            Check(captured && module.ShouldRender({}), "a full pool appears automatically");
            const auto original = slots;
            slots = {};
            source.Reconcile(slots, true, 1, 10001);
            Check(source.Count() == 10 && module.ShouldRender({}) && !source.Plan(0, generation, false, 10001) &&
                !source.MemberLot(0, generation, 1), "a missing native sample preserves the window while suspending stale decisions");
            source.Reconcile(slots, true, 1, 10001);
            Check(source.Count() == 10, "forced reads cannot advance the missing-item grace period");
            slots = original;
            source.Reconcile(slots, true, 1, 10100);
            Check(source.Entries()[0].generation == generation && source.Plan(0, generation, false, 10100),
                "native refresh recovery preserves item identity and restores decisions");

            std::array<uint8_t, 60> result{}; result[20] = 0; result[21] = 1;
            source.HandlePacket(0xD3, result, 10200);
            source.Reconcile(slots, true, 1, 10201);
            Check(source.Count() == 10 && source.Entries()[0].generation == generation,
                "an identity-free outcome cannot suppress an occupied native slot");
            slots[0] = {};
            source.Reconcile(slots, true, 1, 10300);
            source.Reconcile(slots, true, 1, 10600);
            Check(source.Count() == 9 && module.ShouldRender({}), "confirmed removal affects only its own slot");

            std::array<uint8_t, 28> drop{}; Put(drop, 16, uint16_t{100}); Put(drop, 24, uint32_t{2000});
            source.HandlePacket(0xD2, drop, 10700);
            slots[0] = {100, 1, 2000}; source.Reconcile(slots, true, 1, 10700);
            Check(source.Entries()[0].generation != generation && !source.Queue(*captured, 10700, [](const auto&) {}),
                "same-item slot reuse rejects a captured action from the previous drop");
            const auto replaced = source.Entries()[0].generation;
            const auto expiry = source.Entries()[0].expires;
            Put(drop, 24, uint32_t{3000}); source.HandlePacket(0xD2, drop, 10800);
            source.Reconcile(slots, true, 1, 10800);
            Check(source.Entries()[0].generation == replaced && !source.Plan(0, replaced, false, 10800),
                "a new drop awaiting native memory suspends actions without applying its timer to the previous item");
            slots[0].drop = 3000; source.Reconcile(slots, true, 1, 10900);
            Check(source.Entries()[0].generation != replaced && source.Entries()[0].expires != expiry &&
                source.Plan(0, source.Entries()[0].generation, false, 10900), "native confirmation applies the new generation and timer together");
            slots[0].drop = 4000; source.Reconcile(slots, true, 1, 11000);
            Check(source.Plan(0, source.Entries()[0].generation, false, 11000).has_value(),
                "native replacement remains actionable when its drop packet was missed");

            for (uint8_t outcome = 1; outcome <= 3; ++outcome)
            {
                result[21] = outcome; source.HandlePacket(0xD3, result, 11100 + outcome);
                source.Reconcile(slots, true, 1, 11200 + outcome);
                Check(source.Count() == 10, "win, error and lost outcomes never create a permanent slot tombstone");
            }
            slots = {}; source.Reconcile(slots, true, 1, 12000); source.Reconcile(slots, true, 1, 12300);
            Check(source.Count() == 0 && !module.ShouldRender({}), "a confirmed empty pool closes automatically");

            source.Clear(); Put(drop, 24, uint32_t{0}); source.HandlePacket(0xD2, drop, 20000);
            slots[0] = {100, 1, 0}; source.Reconcile(slots, true, 1, 20000);
            const auto zeroTime = source.Entries()[0].generation;
            source.HandlePacket(0xD2, drop, 20100); source.Reconcile(slots, true, 1, 20100);
            Check(source.Entries()[0].generation == zeroTime && source.Entries()[0].expires == 320000,
                "duplicate zero-time drops do not restart identity or countdown");
            source.Reconcile(slots, true, 1, 320001);
            Check(source.Count() == 1 && module.ShouldRender({}), "estimated timeout never removes an occupied native item");
            source.Reconcile(slots, false, 0, 320002);
            slots[0].lot = 65535; source.Reconcile(slots, true, 2, 320003);
            Check(source.Entries()[0].generation != zeroTime, "temporary missing player identity cannot carry items into another character");
            source.BeginZone();
            Check(!source.Ready() && source.Count() == 0 && !module.ShouldRender({}), "zoning clears old slots and hides the pool immediately");
            Put(drop, 24, uint32_t{5000}); source.HandlePacket(0xD2, drop, 330000);
            slots[0].lot = 0; source.Reconcile(slots, true, 2, 330000);
            Check(source.Plan(0, source.Entries()[0].generation, false, 330000).has_value() && source.Entries()[0].expires == 630000,
                "clients with an unavailable native drop time still accept matching-item metadata and decisions");
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
        skillchain_source chains; chains.SetRules(server_rules::phoenix); chains.Observe(Action(2), 10000);
        Check(!chains.Predict(100, 3, 6, {}, 13000).Active(), "Phoenix opener hints wait more than three seconds");
        Check(chains.Predict(100, 3, 6, {}, 13001).chain == skillchain_property::impaction,
            "Shoulder Tackle to Spinning Attack retains Phoenix primary-property priority");
        chains.Clear(); chains.Observe(Action(534, 13, 110), 20000);
        Check(chains.Count() == 0, "Phoenix removes later-era Eclipse Bite resonation");
        chains.SetRules(server_rules::retail); chains.Observe(Action(534, 13, 110), 20000);
        Check(chains.Count() == 1, "Retail pet properties remain available");
        chains.SetRules(server_rules::horizon); chains.Observe(Action(3), 30000);
        Check(chains.Predict(100, 3, 37, {}, 30001).chain == skillchain_property::transfixion, "Horizon overlays remain independently supported");
        const auto* healer = PhantomRollByAbility(100, server_rules::phoenix); const auto* gallant = PhantomRollByAbility(104, server_rules::phoenix);
        Check(healer && std::string{healer->stat} == "hMP" && PhantomRollBasePotency(*healer, 3, 75) == 10 &&
            gallant && std::string{gallant->stat} == "Dmg reflect" && PhantomRollBasePotency(*gallant, 3, 75) == 15,
            "Phoenix rolls use ToAU powers and effect semantics");
        Check(PhantomRollByAbility(105, server_rules::horizon)->per_level && !PhantomRollByAbility(105, server_rules::phoenix)->per_level, "Phoenix does not inherit Horizon Chaos scaling");
        combat_source combat; combat.SetRules(server_rules::phoenix); combat.ApplyStatus(100, 4, 58, 236, 10000);
        Check(combat.Find(100) && combat.Find(100)->statuses[0].earliest == 40000 && combat.Find(100)->statuses[0].expires == 130000,
            "Phoenix Paralyze carries a base duration range");
        ui::status_strip_settings strip; strip.timers = true; strip.crowd_control_only = false;
        Check(ui::StatusTimerText(combat.Find(100)->statuses[0], 10000, strip) == "30-02:00" &&
            ui::StatusTimerText(combat.Find(100)->statuses[0], 50000, strip) == "<01:20", "ranged estimates never pretend an exact deadline");
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
        pool.Reconcile(snapshots, true, 1, 14001); Check(pool.Count() == 1, "native slot contents remain authoritative after an outcome");

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
        pool.Clear();
        snapshots = {};
        snapshots[0] = {100, 1, 1000};
        snapshots[1] = {101, 1, 2000, 450};
        snapshots[2] = {102, 1, 3000, 65535};
        snapshots[3] = {103, 1, 4000};
        snapshots[4] = {104, 1, 5000};
        pool.Reconcile(snapshots, true, 1, 23000);
        const auto pending = pool.Plan(3, pool.Entries()[3].generation, false, 23000);
        Check(pending && pool.Queue(*pending, 23000, [](const auto&) {}), "batch fixture contains an awaiting response");
        const auto lots = pool.CaptureBatch(false, 23000);
        const auto passes = pool.CaptureBatch(true, 23000);
        Check(lots.size() == 2 && lots[0].slot == 0 && lots[1].slot == 4,
            "Lot All shares eligibility and skips prior lots, passes and pending decisions");
        Check(passes.size() == 3 && passes[1].slot == 1,
            "Pass All can change an existing lot while skipping passed and pending items");
        snapshots[4] = {105, 1, 6000};
        snapshots[5] = {106, 1, 7000};
        pool.Reconcile(snapshots, true, 1, 23001);
        sent = 0;
        Check(pool.QueueBatch(lots, 23001, [&](const auto& request) { Check(request.slot == 0, "batch excludes reused slots and later arrivals"); ++sent; }) == 1 && sent == 1,
            "captured batches revalidate identity without expanding to later arrivals");
        Check(pool.QueueBatch(passes, 23001, [&](const auto& request) { Check(request.slot == 1 && request.pass, "Pass All retains the eligible prior lot"); }) == 1 &&
            pool.QueueBatch(passes, 23001, [](const auto&) {}) == 0, "batch submission silently skips stale and newly pending decisions");
        phoenix_integration_test::TreasureController(pool);
        phoenix_integration_test::TreasureReconciliation();
        auto store = settings_store::FromIniText("[treasure]\nexpanded=0\n[layout.treasure]\nx=123\ny=234\n");
        treasure_module module; module.ApplyTextDefaults(ui::theme::CreateDefault()); module.SetSource(&pool); module.Load(store); module.Save(store);
        Check(module.State().bounds.x == 123 && !store.ReadBool("treasure", "expanded", true), "pool presentation persists without changing its anchor");
        size_t textProfiles{}; module.VisitTextSettings([&](ui::text_style&) { ++textProfiles; }); Check(textProfiles == 5, "all pool text follows global typography");
        std::puts("Phoenix integration and Treasure Pool checks passed."); return true;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Phoenix integration: %s\n", error.what()); return false; }
}

auto RunTextVisibilityChecks(void) -> bool
{
    try { xiui::phoenix_integration_test::TextVisibility(); return true; }
    catch (const std::exception& error) { std::fprintf(stderr, "Text visibility and Treasure Pool: %s\n", error.what()); return false; }
}
