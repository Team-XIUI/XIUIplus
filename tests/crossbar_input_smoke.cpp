#include "crossbar_normalizer.hpp"
#include "controller_mailbox.hpp"

#include <cstdio>

namespace
{
    using namespace xiui;

    auto require(const bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "Crossbar input check failed: %s\n", message);
        return condition;
    }

    auto settings(void) -> controller_crossbar_settings
    {
        controller_crossbar_settings value;
        value.enabled = value.replace_native = true;
        value.xinput.left_trigger = 9;
        value.xinput.right_trigger = 12;
        value.xinput.palette_modifier = 11;
        value.xinput.slots = {6, 4, 7, 5, 2, 0, 3, 1};
        value.direct_input.left_trigger = 6;
        value.direct_input.right_trigger = 7;
        value.direct_input.palette_modifier = 5;
        value.direct_input.slots = {41, 40, 41, 40, 3, 2, 1, 0};
        return value;
    }

    struct fixture
    {
        crossbar_normalizer input;
        std::vector<controller_event> events;
        uint64_t now{100};

        explicit fixture(const controller_backend backend = controller_backend::xinput,
            const controller_crossbar_settings& config = settings(), const std::vector<int32_t>& nativeMapping = {})
        {
            this->input.Configure(backend, nativeMapping, config);
            this->state(this->input.Values());
        }

        auto dispatch(const controller_event& event) -> bool { this->events.push_back(event); return true; }
        auto raw(const uint32_t field, const int32_t value, const bool blocked = false, const bool eligible = true) -> bool
        {
            return this->input.Raw(field, value, ++this->now, true, blocked, eligible,
                [this](const controller_event& event) { return this->dispatch(event); });
        }
        auto state(const std::array<int32_t, 176>& value, const bool eligible = true) -> void
        {
            this->input.State(value, ++this->now, false, eligible,
                [this](const controller_event& event) { return this->dispatch(event); });
        }
        auto presses(const controller_action action) const -> size_t
        {
            return static_cast<size_t>(std::count_if(this->events.begin(), this->events.end(),
                [action](const controller_event& event) { return event.action == action && event.pressed; }));
        }
        auto suspend(void) -> void
        {
            this->input.Suspend(++this->now, [this](const controller_event& event) { return this->dispatch(event); });
        }
    };

    auto CheckMappings(void) -> bool
    {
        crossbar_normalizer input;
        controller_crossbar_settings automatic;
        automatic.enabled = automatic.replace_native = true;
        input.Configure(controller_backend::xinput, {}, automatic);
        if (!require(!input.Ready(), "automatic mapping waits for effective native bindings")) return false;
        std::vector<int32_t> native;
        if (!ParseControllerValues("8,9,13,12,10,0,1,3,2,15,-1,-1,14,-33,-33,32,32,-36,-36,35,35,6,7,5,4,11,-1,99", native)) return false;
        input.Configure(controller_backend::xinput, native, automatic);
        if (!require(input.Ready() && input.Mapping().left_trigger == 9 && input.Mapping().right_trigger == 12 &&
            input.Mapping().slots == settings().xinput.slots, "native macro, menu and face bindings resolve separately from event IDs")) return false;
        auto custom = settings();
        custom.xinput.slots[0] = 32;
        input.Configure(controller_backend::xinput, {}, custom);
        if (!require(!input.Ready(), "movement/camera axes cannot become crossbar buttons")) return false;
        custom = settings(); custom.xinput.slots[0] = 9;
        input.Configure(controller_backend::xinput, {}, custom);
        if (!require(!input.Ready(), "trigger and slot collision disables capture")) return false;
        custom = settings(); custom.xinput.slots[0] = custom.xinput.slots[1];
        input.Configure(controller_backend::xinput, {}, custom);
        if (!require(!input.Ready(), "duplicate digital slots disable capture")) return false;
        custom = settings(); custom.direct_input.left_trigger = 32;
        input.Configure(controller_backend::direct_input, {}, custom);
        if (!require(!input.Ready(), "DirectInput movement axis cannot become a trigger")) return false;
        custom = settings(); custom.direct_input.slots[2] = -41;
        input.Configure(controller_backend::direct_input, {}, custom);
        if (!require(!input.Ready(), "reversed POV mappings cannot fire two slots from one direction")) return false;
        return require(ControllerEntryBit(controller_action::crossbar_bank) == 0 &&
            ControllerEntryBit(controller_action::palette_next) == 0, "crossbar events cannot overlap routing revision bits");
    }

    auto CheckBanksAndEdges(void) -> bool
    {
        fixture value;
        if (!require(value.raw(16, 80) && value.input.Bank() == crossbar_bank::left, "left trigger opens left bank and is consumed immediately")) return false;
        if (!require(value.raw(15, 1) && value.presses(controller_action::crossbar_slot) == 1 &&
            value.events.back().bank == crossbar_bank::left && value.events.back().slot == 5 && value.events.back().injected,
            "top face button activates Lua slot five once with provenance")) return false;
        value.raw(15, 1);
        if (!require(value.presses(controller_action::crossbar_slot) == 1, "held activation never repeats")) return false;
        value.raw(17, 80);
        if (!require(value.input.Bank() == crossbar_bank::left_right && value.events.size() >= 4 &&
            value.events[value.events.size() - 2].action == controller_action::crossbar_slot &&
            !value.events[value.events.size() - 2].pressed && value.events[value.events.size() - 2].bank == crossbar_bank::left,
            "second trigger changes bank after releasing the previous slot visual")) return false;
        value.raw(15, 1);
        if (!require(value.presses(controller_action::crossbar_slot) == 1, "held button cannot activate a new bank")) return false;
        value.raw(15, 0); value.raw(15, 1);
        if (!require(value.presses(controller_action::crossbar_slot) == 2 && value.events.back().bank == crossbar_bank::left_right,
            "fresh button press activates expanded bank")) return false;
        value.raw(16, 0);
        if (!require(value.input.Bank() == crossbar_bank::right, "releasing first trigger returns to remaining single bank")) return false;
        value.raw(15, 0); value.raw(17, 0);
        if (!require(value.input.Bank() == crossbar_bank::none, "all trigger release clears bank")) return false;
        if (!require(!value.raw(18, 28000) && !value.raw(20, 28000) && !value.input.Suppress(18, 28000, true),
            "movement and camera stay native")) return false;
        value.raw(17, 80); value.raw(16, 80);
        return require(value.input.Bank() == crossbar_bank::right_left && value.events.back().triggers == 3,
            "opposite trigger order uses a distinct bank and reports both triggers");
    }

    auto CheckThresholdsAndDoubleTap(void) -> bool
    {
        fixture value;
        value.raw(16, 29);
        if (!require(value.input.Bank() == crossbar_bank::none, "trigger waits for press threshold")) return false;
        value.raw(16, 30); value.raw(16, 16);
        if (!require(value.input.Bank() == crossbar_bank::left && value.events.size() == 1, "hysteresis prevents trigger jitter")) return false;
        value.raw(16, 14);
        if (!require(value.input.Bank() == crossbar_bank::none, "trigger releases below release threshold")) return false;
        auto config = settings(); config.double_tap = true;
        fixture twice(controller_backend::xinput, config);
        twice.raw(16, 80); twice.now += 60; twice.raw(16, 0); twice.now += 100; twice.raw(16, 80);
        if (!require(twice.input.Bank() == crossbar_bank::left_double, "double tap held on second press opens double bank")) return false;
        twice.now += 60; twice.raw(16, 0); twice.now += 400; twice.raw(16, 80);
        if (!require(twice.input.Bank() == crossbar_bank::left, "expired double tap starts ordinary bank")) return false;
        fixture bounce(controller_backend::xinput, config);
        bounce.raw(17, 80); bounce.raw(17, 0); bounce.raw(17, 80);
        if (!require(bounce.input.Bank() == crossbar_bank::right, "short analog bounce cannot create double tap")) return false;
        config.expanded = false;
        fixture single(controller_backend::xinput, config);
        single.raw(17, 80); single.raw(16, 80);
        return require(single.input.Bank() == crossbar_bank::right, "disabling expanded banks retains the first trigger bank");
    }

    auto CheckStateDeduplication(void) -> bool
    {
        fixture value;
        value.raw(16, 80);
        auto filtered = value.input.Values(); filtered[16] = 0;
        value.state(filtered);
        if (!require(value.input.Bank() == crossbar_bank::left, "filtered state cannot release an event-driven trigger")) return false;
        value.raw(12, 1);
        filtered[12] = 0; value.state(filtered);
        if (!require(value.presses(controller_action::crossbar_slot) == 1 && value.events.back().pressed,
            "event plus filtered state cannot duplicate or release an activation")) return false;
        value.raw(12, 0);
        if (!require(!value.events.back().pressed && value.events.back().slot == 7, "canonical release drains original slot once")) return false;
        fixture stateOnly;
        auto packet = stateOnly.input.Values(); packet[17] = 80;
        stateOnly.state(packet); packet[14] = 1; stateOnly.state(packet); stateOnly.state(packet);
        if (!require(stateOnly.presses(controller_action::crossbar_slot) == 1 && stateOnly.events.back().slot == 8,
            "state-only activation uses Lua slot order once")) return false;
        stateOnly.raw(14, 1);
        packet[14] = 0; stateOnly.state(packet);
        if (!require(stateOnly.events.back().pressed, "event takeover ignores modified state neutral")) return false;
        stateOnly.raw(14, 0);
        if (!require(!stateOnly.events.back().pressed, "event takeover still observes real release")) return false;
        crossbar_normalizer fresh;
        fresh.Configure(controller_backend::xinput, {}, settings());
        std::vector<controller_event> events;
        const auto dispatch = [&](const controller_event& event) { events.push_back(event); return true; };
        auto held = fresh.Values(); held[16] = 80; held[12] = 1;
        fresh.State(held, 100, false, true, dispatch);
        if (!require(events.empty(), "reload with held trigger and face button cannot activate")) return false;
        held[16] = held[12] = 0; fresh.State(held, 110, false, true, dispatch);
        held[16] = 80; held[12] = 1; fresh.State(held, 120, false, true, dispatch);
        return require(events.size() == 2 && events.front().action == controller_action::crossbar_bank &&
            events.back().action == controller_action::crossbar_slot && events.back().pressed && events.back().slot == 7,
            "fresh simultaneous trigger and slot activate after reload neutral was observed");
    }

    auto CheckFreshPacketTransitions(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        {
            fixture value{backend};
            const uint32_t left = backend == controller_backend::xinput ? 16 : 54;
            const uint32_t right = backend == controller_backend::xinput ? 17 : 55;
            const uint32_t bottom = backend == controller_backend::xinput ? 12 : 49;
            const uint32_t top = backend == controller_backend::xinput ? 15 : 51;
            auto packet = value.input.Values(); packet[left] = 128; packet[bottom] = 1;
            value.state(packet); value.state(packet);
            if (!require(value.presses(controller_action::crossbar_slot) == 1 && value.events.back().bank == crossbar_bank::left &&
                value.events.back().slot == 7, "one packet can enter a bank and activate a fresh slot exactly once on both backends")) return false;
            packet[right] = 128; packet[top] = 1;
            value.state(packet); value.state(packet);
            if (!require(value.presses(controller_action::crossbar_slot) == 2 && value.events.back().bank == crossbar_bank::left_right &&
                value.events.back().slot == 5, "expanded transition accepts a fresh slot while inhibiting the previously held slot")) return false;
            packet[right] = 0; value.state(packet); value.state(packet);
            if (!require(value.input.Bank() == crossbar_bank::left && value.presses(controller_action::crossbar_slot) == 2,
                "returning to a single bank cannot replay either held slot")) return false;
            packet[bottom] = packet[top] = 0; value.state(packet);
            packet[bottom] = 1; value.state(packet);
            if (!require(value.presses(controller_action::crossbar_slot) == 3 && value.events.back().bank == crossbar_bank::left,
                "held-slot guard ends after release and a fresh press")) return false;
        }
        fixture palette;
        auto packet = palette.input.Values(); packet[16] = 80; palette.state(packet);
        packet[9] = packet[0] = packet[12] = 1; palette.state(packet); palette.state(packet);
        return require(palette.presses(controller_action::palette_previous) == 1 && palette.presses(controller_action::crossbar_slot) == 0,
            "palette changes still inhibit simultaneous slot presses until neutral");
    }

    auto CheckSuspensionAndNativeOwnership(void) -> bool
    {
        fixture value;
        value.raw(16, 80); value.raw(12, 1); value.suspend();
        if (!require(value.events.back().bank == crossbar_bank::none && !value.events.back().pressed,
            "suspension clears bank after releasing slot visual")) return false;
        const auto before = value.presses(controller_action::crossbar_slot);
        value.raw(16, 80); value.raw(12, 1);
        if (!require(value.input.Bank() == crossbar_bank::none && value.presses(controller_action::crossbar_slot) == before,
            "held controls cannot reactivate after focus loss or disconnect")) return false;
        if (!require(value.raw(16, 0, false, false) && value.raw(12, 0, false, false) &&
            !value.input.Suppress(12, 0, false), "owned releases drain through native ownership and restore movement")) return false;
        value.raw(16, 0); value.raw(16, 80); value.raw(12, 1, true);
        if (!require(value.presses(controller_action::crossbar_slot) == before && value.input.Bank() == crossbar_bank::none,
            "another owner's blocked press cannot activate or retain a bank")) return false;
        value.raw(12, 0); value.raw(16, 0); value.raw(16, 80);
        value.raw(12, 1, false, false);
        if (!require(value.presses(controller_action::crossbar_slot) == before, "native text, menu and subtarget precedence suppresses actions")) return false;
        auto config = settings(); config.replace_native = false;
        fixture native(controller_backend::xinput, config);
        if (!require(!native.raw(16, 80) && !native.raw(12, 1) && native.events.empty(), "native macro controls stay untouched until replacement is enabled")) return false;
        fixture changed;
        changed.raw(16, 80); changed.raw(12, 1);
        changed.input.Configure(controller_backend::xinput, {}, config);
        if (!require(changed.input.Suppress(16, 80, false) && changed.input.Suppress(12, 1, false), "disabling replacement retains ownership of already-held controls")) return false;
        changed.raw(16, 0, false, false); changed.raw(12, 0, false, false);
        if (!require(!changed.input.Suppress(16, 0, false) && !changed.input.Suppress(12, 0, false), "settings-change drain ends at neutral")) return false;
        fixture profile;
        profile.raw(16, 80); profile.raw(12, 1);
        const auto activated = profile.presses(controller_action::crossbar_slot);
        // A routing revision invalidates input even when local device settings are unchanged.
        profile.input.Configure(controller_backend::xinput, {}, settings());
        profile.raw(17, 80); profile.raw(15, 1);
        if (!require(profile.input.Bank() == crossbar_bank::none && profile.presses(controller_action::crossbar_slot) == activated,
            "profile revision plus second trigger cannot bypass fresh neutral")) return false;
        profile.raw(16, 0); profile.raw(17, 0); profile.raw(12, 0); profile.raw(15, 0);
        profile.raw(17, 80); profile.raw(15, 1);
        if (!require(profile.input.Bank() == crossbar_bank::right && profile.presses(controller_action::crossbar_slot) == activated + 1,
            "profile revision preserves held releases and resumes after full neutral")) return false;
        fixture device(controller_backend::direct_input);
        device.raw(54, 128); device.raw(49, 128);
        const auto deviceActions = device.presses(controller_action::crossbar_slot);
        device.input.ResetDevice();
        auto packet = device.input.Values(); packet[54] = 128; packet[51] = 128;
        device.state(packet);
        if (!require(device.input.Bank() == crossbar_bank::none && device.presses(controller_action::crossbar_slot) == deviceActions,
            "replacement device cannot inherit the previous controller bank or held buttons")) return false;
        packet[54] = packet[51] = 0; device.state(packet);
        packet[54] = 128; device.state(packet); packet[51] = 128; device.state(packet);
        return require(device.input.Bank() == crossbar_bank::left && device.presses(controller_action::crossbar_slot) == deviceActions + 1,
            "replacement device resumes only after a fresh neutral packet");
    }

    auto CheckPaletteAndDirectInput(void) -> bool
    {
        fixture value;
        value.raw(16, 80); value.raw(12, 1); value.raw(9, 1); value.raw(0, 1);
        if (!require(value.presses(controller_action::palette_previous) == 1 && value.presses(controller_action::crossbar_slot) == 1,
            "held shoulder plus Up changes palette without firing the D-pad slot")) return false;
        value.raw(0, 1); value.raw(12, 1);
        if (!require(value.presses(controller_action::palette_previous) == 1 && value.presses(controller_action::crossbar_slot) == 1,
            "palette change does not repeat or replay another held slot")) return false;
        value.raw(0, 0); value.raw(1, 1);
        if (!require(value.presses(controller_action::palette_next) == 1, "held shoulder plus Down selects next palette")) return false;
        fixture direct(controller_backend::direct_input);
        if (!require(direct.input.Ready() && direct.raw(54, 128), "DirectInput trigger uses its own field namespace")) return false;
        direct.raw(32, 0);
        if (!require(direct.events.back().action == controller_action::crossbar_slot && direct.events.back().slot == 1,
            "DirectInput cardinal POV selects up slot")) return false;
        direct.raw(32, 4500);
        if (!require(!direct.events.back().pressed && direct.presses(controller_action::crossbar_slot) == 1,
            "diagonal POV cannot fire two slots")) return false;
        direct.raw(32, 9000);
        if (!require(direct.events.back().pressed && direct.events.back().slot == 2, "DirectInput POV rotates to right slot")) return false;
        direct.raw(32, -1); direct.raw(51, 128);
        if (!require(direct.events.back().slot == 5 && direct.events.back().bank == crossbar_bank::left, "DirectInput physical top face uses slot five")) return false;
        return require(!direct.raw(0, 100) && !direct.raw(8, 100), "DirectInput movement and camera axes remain native");
    }

    auto CheckNativeMacroSuppression(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        {
            controller_crossbar_settings config; config.suppress_native_macros = true;
            const std::vector<int32_t> native = backend == controller_backend::xinput ?
                std::vector<int32_t>{0, 9, 0, 12} : std::vector<int32_t>{0, 6, 0, 7};
            const uint32_t left = backend == controller_backend::xinput ? 16 : 54;
            const uint32_t right = backend == controller_backend::xinput ? 17 : 55;
            const uint32_t face = backend == controller_backend::xinput ? 12 : 49;
            fixture value{backend, config, native};
            if (!require(!value.input.Ready() && value.raw(left, 128) && value.raw(right, 128) &&
                value.input.Suppress(left, 128, true) && value.input.Suppress(right, 128, true) && value.events.empty(),
                "native macro triggers are suppressed with crossbar disabled and only a partial native map")) return false;
            if (!require(!value.raw(face, 1) && !value.input.Suppress(face, 1, true) &&
                !value.raw(backend == controller_backend::xinput ? 18 : 0, 100) &&
                !value.raw(backend == controller_backend::xinput ? 20 : 8, 100),
                "native macro suppression leaves face buttons, movement and camera available")) return false;
            if (!require(value.raw(left, 0) && value.raw(right, 0) && value.events.empty(),
                "native trigger release remains consumed without dispatching crossbar actions")) return false;

            auto custom = settings(); custom.enabled = custom.replace_native = false; custom.suppress_native_macros = true;
            if (backend == controller_backend::xinput) { custom.xinput.left_trigger = 8; custom.xinput.right_trigger = 11; }
            else { custom.direct_input.left_trigger = 10; custom.direct_input.right_trigger = 11; }
            fixture overridden{backend, custom, native};
            const uint32_t customLeft = backend == controller_backend::xinput ? 8 : 58;
            if (!require(overridden.raw(left, 128) && !overridden.raw(customLeft, 1) && overridden.events.empty(),
                "native suppression follows the effective FFXI trigger instead of custom crossbar overrides")) return false;

            auto one = native; one[3] = -1;
            fixture partial{backend, config, one};
            if (!require(partial.raw(left, 128) && !partial.raw(right, 128) && partial.events.empty(),
                "one independently valid native macro trigger does not require the other trigger")) return false;
            fixture unknown{backend, config};
            if (!require(!unknown.raw(left, 128) && !unknown.raw(right, 128) && unknown.events.empty(),
                "missing native bindings never cause guessed button capture")) return false;
            fixture axes{backend, config, {0, 32, 0, 35}};
            if (!require(!axes.raw(backend == controller_backend::xinput ? 18 : 0, 100) &&
                !axes.raw(backend == controller_backend::xinput ? 20 : 8, 100),
                "unsupported axis trigger bindings never swallow a movement or camera axis")) return false;
        }
        return true;
    }

    auto CheckNativeMacroStateAndBuffer(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        {
            controller_crossbar_settings config; config.suppress_native_macros = true;
            const std::vector<int32_t> native = backend == controller_backend::xinput ?
                std::vector<int32_t>{0, 9, 0, 12} : std::vector<int32_t>{0, 6, 0, 7};
            const uint32_t left = backend == controller_backend::xinput ? 16 : 54;
            const uint32_t right = backend == controller_backend::xinput ? 17 : 55;
            const uint32_t face = backend == controller_backend::xinput ? 12 : 49;
            const uint32_t move = backend == controller_backend::xinput ? 18 : 0;
            fixture polled{backend, config, native};
            auto packet = polled.input.Values(); packet[left] = packet[right] = 128; packet[face] = 1; packet[move] = 100;
            polled.state(packet);
            for (uint32_t field = 0; field < packet.size(); ++field)
                if (polled.input.Suppress(field, packet[field], true)) packet[field] = 0;
            if (!require(packet[left] == 0 && packet[right] == 0 && packet[face] == 1 && packet[move] == 100 && polled.events.empty(),
                "state-only input clears both native triggers while retaining unrelated state and dispatching nothing")) return false;

            fixture buffered{backend, config, native};
            std::array<std::pair<uint32_t, int32_t>, 4> data{{{left, 128}, {face, 1}, {right, 128}, {left, 0}}};
            std::array<bool, 4> consumed{};
            for (size_t index = 0; index < data.size(); ++index)
                consumed[index] = buffered.input.State(data[index].first, data[index].second, ++buffered.now, false, true,
                    [&](const controller_event& event) { return buffered.dispatch(event); });
            for (size_t index = 0; index < data.size(); ++index)
                if (consumed[index] || buffered.input.Suppress(data[index].first, data[index].second, true)) data[index].second = 0;
            if (!require(data[0].second == 0 && data[1].second == 1 && data[2].second == 0 && data[3].second == 0 && buffered.events.empty(),
                "buffered state filters a native quick tap and hold without swallowing another button")) return false;

            fixture overlap{backend, [&] { auto enabled = settings(); enabled.suppress_native_macros = true; return enabled; }(), native};
            overlap.raw(left, 128); overlap.raw(face, 1);
            auto filtered = overlap.input.Values(); filtered[left] = filtered[face] = 0;
            overlap.state(filtered); overlap.state(filtered);
            if (!require(overlap.presses(controller_action::crossbar_slot) == 1,
                "overlapping native and XIUI trigger ownership activates a slot exactly once across event and state paths")) return false;
            overlap.raw(face, 0); overlap.raw(left, 0);
            const auto releases = std::count_if(overlap.events.begin(), overlap.events.end(), [](const auto& event) {
                return event.action == controller_action::crossbar_slot && !event.pressed;
            });
            if (!require(releases == 1 && overlap.input.Bank() == crossbar_bank::none,
                "overlapping trigger ownership emits exactly one matching activation release")) return false;
        }
        return true;
    }

    auto CheckNativeMacroLifecycle(void) -> bool
    {
        controller_crossbar_settings config; config.suppress_native_macros = true;
        const std::vector<int32_t> native{0, 9, 0, 12};
        fixture blocked{controller_backend::xinput, config, native};
        if (!require(!blocked.raw(16, 128, true) && !blocked.input.Suppress(16, 128, false) && blocked.events.empty(),
            "another input owner's native trigger press cannot acquire new XIUI ownership")) return false;
        if (!require(!blocked.input.State(16, 128, ++blocked.now, true, true,
            [&](const controller_event& event) { return blocked.dispatch(event); }),
            "canonical event/state deduplication still honors a prior blocked owner")) return false;
        fixture nativeContext{controller_backend::xinput, config, native};
        if (!require(!nativeContext.raw(16, 128, false, false) && !nativeContext.input.Suppress(16, 128, false),
            "native text, menus, subtargets and lost foreground retain unclaimed macro inputs")) return false;

        fixture held{controller_backend::xinput, config, native};
        held.raw(16, 128); held.suspend();
        config.suppress_native_macros = false;
        held.input.Configure(controller_backend::xinput, native, config);
        if (!require(held.input.Suppress(16, 128, false) && held.raw(16, 0, false, false) && !held.input.Suppress(16, 0, false) &&
            !held.raw(16, 128) && held.events.empty(), "turning native suppression off drains an owned hold and release, then restores the next native press")) return false;
        config.suppress_native_macros = true;
        held.input.Configure(controller_backend::xinput, native, config); held.raw(16, 128); held.input.ResetDevice();
        if (!require(!held.input.Suppress(16, 0, false) && held.raw(16, 128) && held.events.empty(),
            "device replacement clears stale ownership while native suppression never creates an activation")) return false;

        auto enabled = settings(); enabled.suppress_native_macros = true;
        held.input.Configure(controller_backend::xinput, native, enabled);
        held.raw(12, 1);
        if (!require(held.input.Bank() == crossbar_bank::none && held.presses(controller_action::crossbar_slot) == 0,
            "enabling crossbar during a suppressed hold still requires fresh neutral")) return false;
        held.raw(16, 0); held.raw(12, 0); held.raw(16, 128); held.raw(12, 1);
        return require(held.presses(controller_action::crossbar_slot) == 1,
            "crossbar resumes after native trigger and face buttons return to neutral");
    }

    auto CheckMailbox(void) -> bool
    {
        controller_mailbox mailbox;
        controller_event bank{controller_action::crossbar_bank, controller_backend::xinput, 0, true, false, false, 100};
        bank.bank = crossbar_bank::left; bank.triggers = 1;
        if (!require(!mailbox.Submit(bank), "mailbox rejects crossbar until replacement is enabled")) return false;
        mailbox.CrossbarSettings(settings());
        mailbox.Publish(ControllerEntryBit(controller_action::up));
        if (!require(mailbox.Submit(bank) && (mailbox.State() & controller_capture_mask) == 0,
            "crossbar bank routing does not capture movement or acquire UI focus")) return false;
        controller_event slot{controller_action::crossbar_slot, controller_backend::xinput, 0, true, false, false, 101};
        slot.bank = crossbar_bank::left; slot.slot = 7;
        if (!require(mailbox.Submit(slot) && !mailbox.Submit(slot), "mailbox rejects duplicate activation press")) return false;
        slot.pressed = false;
        if (!require(mailbox.Submit(slot) && !mailbox.Submit(slot), "mailbox delivers matching release once")) return false;
        if (!require(!mailbox.Submit(bank, true), "native menu wins even if cached runtime routing is stale")) return false;
        mailbox.Publish(static_cast<uint32_t>(controller_capture_mode::exclusive));
        if (!require(!mailbox.Submit(bank), "existing XIUI modal owns input before crossbar")) return false;
        mailbox.Publish(0);
        bank.bank = crossbar_bank::right_double; bank.triggers = 2;
        if (!require(mailbox.Submit(bank), "sixth bank is accepted without entry-bit shifts")) return false;
        auto pending = mailbox.Take();
        auto disabled = settings(); disabled.enabled = false;
        mailbox.CrossbarSettings(disabled);
        if (!require(!mailbox.Current(pending.generation) && !mailbox.Submit(slot), "settings changes invalidate pending controller work")) return false;
        mailbox.CrossbarSettings(settings()); mailbox.Publish(0);
        bank.repeated = true;
        if (!require(!mailbox.Submit(bank), "repeated synthetic bank events cannot activate")) return false;
        bank.repeated = false; mailbox.Submit(bank); mailbox.Reset();
        if (!require(mailbox.Take().count == 0 && !mailbox.Submit(slot), "runtime replacement discards actions and stale releases")) return false;
        controller_status suspended; suspended.suspended = true;
        mailbox.Status(suspended); mailbox.Reset(); mailbox.Status({});
        const auto lifecycle = mailbox.Take();
        if (!require(lifecycle.has_status && lifecycle.status.suspended, "fast reconnect cannot overwrite pending suspension cleanup")) return false;
        mailbox.Status({});
        return require(!mailbox.Take().status.suspended, "ready status resumes after suspension was delivered");
    }
}

auto main(void) -> int
{
    if (!CheckMappings() || !CheckBanksAndEdges() || !CheckThresholdsAndDoubleTap() || !CheckStateDeduplication() || !CheckFreshPacketTransitions() ||
        !CheckSuspensionAndNativeOwnership() || !CheckPaletteAndDirectInput() || !CheckNativeMacroSuppression() ||
        !CheckNativeMacroStateAndBuffer() || !CheckNativeMacroLifecycle() || !CheckMailbox()) return 1;
    std::puts("Crossbar input checks passed.");
    return 0;
}
