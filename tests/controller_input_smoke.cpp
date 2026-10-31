#include "controller_normalizer.hpp"
#include "controller_mailbox.hpp"

#include <cstdio>
#include <limits>

namespace
{
    using namespace xiui;

    auto require(const bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "Controller check failed: %s\n", message);
        return condition;
    }

    auto CheckNativeWindowCycle(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        {
            controller_normalizer input; input.Configure(backend, {}); input.SetNavigationOnly(true); input.SetNativeWindowCycle(true);
            controller_mailbox mailbox;
            mailbox.Publish(static_cast<uint32_t>(controller_capture_mode::navigation) | controller_native_cycle | ControllerEntryBit(controller_action::cycle_window));
            const auto dispatch = [&](const controller_event& event) { return mailbox.Submit(event, true); };
            const uint32_t cycle = backend == controller_backend::xinput ? 15 : 51;
            const uint32_t confirm = backend == controller_backend::xinput ? 12 : 49;
            if (!require(!input.Raw(cycle, 1, 10, false, false, true, dispatch) && !input.Suppress(cycle, 1, true) &&
                !input.Raw(cycle, 0, 20, false, false, true, dispatch) && !input.Suppress(cycle, 0, true),
                "native window cycle press, held state and release remain available in both backends")) return false;
            if (!require(input.Raw(confirm, 1, 30, false, false, true, dispatch) && input.Suppress(confirm, 1, true),
                "native cycle handoff retains ownership of chat confirmation")) return false;
            const auto batch = mailbox.Take();
            if (!require(batch.count == 1 && batch.events[0].action == controller_action::confirm,
                "native cycling queues no duplicate XIUI cycle action")) return false;
        }
        return true;
    }

    auto CheckMappings(void) -> bool
    {
        std::vector<int32_t> values;
        if (!require(ParseControllerValues("8,9,13,12,10,0,1,3,2,15,-1,-1,14,-33,-33,32,32,-36,-36,35,35,6,7,5,4,11,-1,-1,-1,-1,-1,77", values) && values.size() == 32 && values.back() == 77,
            "retain extended native mapping")) return false;
        if (!require(!ParseControllerValues("1,2,", values) && !ParseControllerValues("1,no", values) && !ParseControllerValues("999999999999", values), "reject malformed values")) return false;
        if (!require(controller_normalizer::XInputButton(3) == 12 && controller_normalizer::XInputButton(2) == 15 &&
            controller_normalizer::XInputButton(16) == -1, "native mappings are not event IDs")) return false;
        values[7] = 0;
        values[5] = 3;
        controller_normalizer input;
        input.Configure(controller_backend::xinput, values);
        std::vector<controller_event> events;
        const auto dispatch = [&](const controller_event& event) { events.push_back(event); return false; };
        input.Raw(13, 1, 10, true, false, true, dispatch);
        input.State(13, 0, 11, false, true, dispatch);
        input.Repeat(1000, true, dispatch);
        if (!require(events.size() == 1 && events.front().action == controller_action::confirm && events.front().injected,
            "remapped Confirm once, with injected provenance")) return false;
        if (!require(input.Raw(13, 0, 1001, true, false, false, dispatch) && events.size() == 2 && !events.back().pressed,
            "owned release consumed after capture closes")) return false;
        if (!require(!input.Suppress(13, 1, false), "released button is available to game")) return false;
        values[7] = std::numeric_limits<int32_t>::min();
        input.Configure(controller_backend::xinput, values);
        input.Raw(12, 1, 1010, false, false, true, dispatch);
        return true;
    }

    auto CheckOwnership(void) -> bool
    {
        controller_normalizer input;
        input.Configure(controller_backend::xinput, {});
        std::vector<controller_event> events;
        bool captured = true;
        const auto dispatch = [&](const controller_event& event) {
            events.push_back(event);
            if (event.action == controller_action::cancel && event.pressed) captured = false;
            return false;
        };
        if (!require(input.Raw(13, 1, 10, false, false, captured, dispatch) && !captured,
            "closing Cancel remains consumed")) return false;
        if (!require(input.Suppress(13, 1, captured), "held closing Cancel suppressed in game state")) return false;
        input.Raw(13, 1, 20, false, false, captured, dispatch);
        input.Repeat(1000, captured, dispatch);
        if (!require(events.size() == 1, "held Cancel never repeats")) return false;
        if (!require(input.Raw(13, 0, 1001, false, false, captured, dispatch), "Cancel release is drained")) return false;
        if (!require(!input.Raw(12, 1, 1010, false, false, false, dispatch), "gameplay button is not consumed")) return false;
        input.Suspend();
        if (!require(!input.Suppress(12, 1, false), "suspension does not claim gameplay-held buttons")) return false;

        input.Configure(controller_backend::xinput, {});
        events.clear();
        input.Raw(0, 1, 2000, false, true, true, dispatch);
        input.Repeat(3000, true, dispatch);
        if (!require(events.empty(), "already blocked event cannot activate or repeat")) return false;
        input.Raw(0, 0, 3010, false, true, false, dispatch);
        input.Raw(0, 1, 3020, false, false, true, dispatch);
        input.Repeat(3379, true, dispatch);
        if (!require(events.size() == 1, "direction initial delay")) return false;
        input.Repeat(3380, true, dispatch);
        input.Repeat(3381, true, dispatch);
        return require(events.size() == 2 && events.back().repeated, "direction repeat once at deadline");
    }

    auto CheckStateAndPov(void) -> bool
    {
        controller_normalizer input;
        input.Configure(controller_backend::direct_input, {});
        std::vector<controller_event> events;
        const auto dispatch = [&](const controller_event& event) { events.push_back(event); return false; };
        input.Raw(32, 0, 10, false, false, true, dispatch);
        input.Raw(32, 9000, 20, false, false, true, dispatch);
        if (!require(events.size() == 3 && events[0].action == controller_action::up && events[0].pressed &&
            events[1].action == controller_action::up && !events[1].pressed && events[2].action == controller_action::right && events[2].pressed,
            "POV direction changes during capture need no neutral")) return false;
        input.Raw(32, -1, 30, false, false, false, dispatch);
        if (!require(!input.Suppress(32, -1, false), "POV neutral releases drain")) return false;

        input.Configure(controller_backend::xinput, {});
        events.clear();
        input.State(12, 1, 100, false, true, dispatch);
        input.State(12, 1, 110, false, true, dispatch);
        if (!require(events.size() == 1 && events.front().action == controller_action::confirm, "state-only Confirm once")) return false;
        input.Raw(12, 1, 111, true, false, true, dispatch);
        input.State(12, 0, 112, false, true, dispatch);
        if (!require(events.size() == 1, "event plus state does not duplicate or falsely release")) return false;
        input.Raw(12, 0, 120, true, false, false, dispatch);
        if (!require(events.size() == 2 && !events.back().pressed, "canonical release after mixed input")) return false;

        input.Configure(controller_backend::xinput, {});
        events.clear();
        input.State(0, 1, 200, false, true, dispatch);
        input.Raw(0, 0, 201, false, false, true, dispatch);
        input.State(0, 1, 210, false, true, dispatch);
        if (!require(events.size() == 1, "filtered neutral echo cannot release state-only press")) return false;
        input.State(0, 0, 220, false, false, dispatch);
        if (!require(events.size() == 2 && !events.back().pressed, "state-only release remains available")) return false;

        input.State(18, 26000, 300, false, true, dispatch);
        if (!require(input.Suppress(18, 26000, false), "held stick drains after capture")) return false;
        input.State(18, 0, 310, false, false, dispatch);
        return require(!input.Suppress(18, 0, false), "stick neutral restores game state");
    }

    auto CheckOpeningCapture(void) -> bool
    {
        for (const auto backend : {controller_backend::direct_input, controller_backend::xinput})
        for (const bool stateOnly : {false, true})
        for (const bool directional : {false, true})
        {
            controller_normalizer input;
            input.Configure(backend, {});
            bool captured{};
            size_t opens{};
            const auto action = directional ? controller_action::up : controller_action::cycle_window;
            const auto field = directional ? (backend == controller_backend::xinput ? 0u : 32u) :
                (backend == controller_backend::xinput ? 15u : 51u);
            const bool pov = directional && backend == controller_backend::direct_input;
            const int32_t down = pov ? 0 : 1, neutral = pov ? -1 : 0;
            const auto dispatch = [&](const controller_event& event) {
                if (event.action == action && event.pressed) { captured = true; ++opens; }
                return event.action == action && event.pressed;
            };
            const auto press = [&](const int32_t value, const bool blocked = false) {
                const bool before = captured;
                const bool consumed = stateOnly ? input.State(field, value, 100, blocked, before, dispatch) :
                    input.Raw(field, value, 100, false, blocked, before, dispatch);
                return input.Suppress(field, value, before || captured) || consumed;
            };
            if (!require(!press(down, true) && opens == 0, "blocked entry cannot acquire focus")) return false;
            press(neutral);
            if (!require(press(down) && captured && opens == 1, "opening press is consumed on both backends and state/event paths")) return false;
            input.Repeat(459, captured, dispatch);
            if (!require(opens == 1, "opening direction keeps the initial repeat delay")) return false;
            input.Repeat(460, captured, dispatch);
            const size_t expected = directional ? 2 : 1;
            if (!require(opens == expected, "opening direction repeats while one-shot entry stays single")) return false;
            captured = false;
            if (!require(press(down) && opens == expected, "held opening button drains after focus ends")) return false;
            if (!require(press(neutral) && !input.Suppress(field, neutral, false), "opening button release drains once and returns to native")) return false;
        }
        return true;
    }

    auto CheckNavigationCapture(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        {
            controller_normalizer input;
            input.Configure(backend, {});
            const auto dispatch = [](const controller_event&) { return false; };
            const std::array<uint32_t, 4> axes = backend == controller_backend::xinput ?
                std::array<uint32_t, 4>{18, 19, 20, 21} : std::array<uint32_t, 4>{0, 4, 8, 20};
            const int32_t value = backend == controller_backend::xinput ? 24000 : 100;
            input.Raw(axes[0], value, 100, false, false, true, dispatch);
            input.SetNavigationOnly(true);
            for (const auto axis : axes)
            {
                if (!require(!input.Raw(axis, value, 110, false, false, true, dispatch) &&
                    !input.State(axis, value, 111, false, true, dispatch) && !input.Suppress(axis, value, true),
                    "chat preserves movement/camera axes in events and returned state, including a previously held stick")) return false;
            }
            const auto direction = backend == controller_backend::xinput ? 0u : 32u;
            const auto confirm = backend == controller_backend::xinput ? 12u : 49u;
            if (!require(input.Raw(direction, backend == controller_backend::xinput ? 1 : 0, 120, false, false, true, dispatch) &&
                input.Suppress(direction, backend == controller_backend::xinput ? 1 : 0, true), "chat still captures D-pad navigation")) return false;
            if (!require(input.Raw(confirm, 1, 130, false, false, true, dispatch) && input.Suppress(confirm, 1, true),
                "chat still captures selection buttons")) return false;
            const auto gameplay = backend == controller_backend::xinput ? 6u : 60u;
            if (!require(!input.Raw(gameplay, 1, 140, false, false, true, dispatch) && !input.Suppress(gameplay, 1, true),
                "unmapped gameplay buttons pass through chat")) return false;
            input.SetNavigationOnly(false);
            if (!require(input.Raw(axes[0], value, 150, false, false, true, dispatch) && input.Suppress(axes[0], value, true),
                "exclusive config capture still suppresses movement")) return false;
        }
        std::vector<int32_t> mapping;
        ParseControllerValues("8,9,13,12,10,0,1,9,2,15,-1,-1,14,-33,-33,32,32,-36,-36,35,35,6,7,5,4,11,-1", mapping);
        controller_normalizer remapped;
        remapped.Configure(controller_backend::xinput, mapping); remapped.SetNavigationOnly(true);
        return require(remapped.Suppress(16, 80, true) && !remapped.Suppress(12, 1, true), "navigation capture follows remapped controls");
    }

    auto CheckChatTabTriggers(void) -> bool
    {
        for (const auto backend : {controller_backend::xinput, controller_backend::direct_input})
        for (const bool stateOnly : {false, true})
        {
            controller_normalizer input;
            input.Configure(backend, {});
            input.SetNavigationOnly(true);
            std::vector<controller_event> events;
            const auto dispatch = [&](const controller_event& event) { events.push_back(event); return false; };
            uint64_t now{100};
            const auto send = [&](const uint32_t field, const int32_t value, const bool captured = true) {
                return stateOnly ? input.State(field, value, ++now, false, captured, dispatch) :
                    input.Raw(field, value, ++now, false, false, captured, dispatch);
            };
            const std::array<uint32_t, 2> bumpers = backend == controller_backend::xinput ?
                std::array<uint32_t, 2>{8, 9} : std::array<uint32_t, 2>{52, 53};
            const std::array<uint32_t, 2> triggers = backend == controller_backend::xinput ?
                std::array<uint32_t, 2>{16, 17} : std::array<uint32_t, 2>{54, 55};
            for (const auto field : bumpers)
            {
                if (!require(!send(field, 1) && !input.Suppress(field, 1, true) && !send(field, 0) && events.empty(),
                    "focused chat leaves both bumpers and auto-run available to native input")) return false;
            }
            for (size_t index = 0; index < triggers.size(); ++index)
            {
                const auto action = index == 0 ? controller_action::previous_tab : controller_action::next_tab;
                if (!require(send(triggers[index], 100) && input.Suppress(triggers[index], 100, true) &&
                    events.size() == index * 2 + 1 && events.back().action == action && events.back().pressed,
                    "chat trigger changes its tab once and suppresses the native macro trigger")) return false;
                send(triggers[index], 100);
                input.Repeat(now + 1000, true, dispatch);
                if (!require(events.size() == index * 2 + 1 && send(triggers[index], 0, false) && !events.back().pressed &&
                    !input.Suppress(triggers[index], 0, false), "held triggers do not repeat and their closing release drains")) return false;
            }
            input.SetNavigationOnly(false);
            events.clear();
            if (!require(send(bumpers[0], 1) && events.size() == 1 && events.back().action == controller_action::previous_tab,
                "configuration retains bumper tab navigation")) return false;
            send(bumpers[0], 0);
            send(triggers[0], 100, false);
            events.clear();
            input.SetNavigationOnly(true);
            send(bumpers[1], 1);
            if (!require(events.empty(), "entering chat with a trigger held cannot switch tabs")) return false;
            send(triggers[0], 0);
            events.clear();
            send(triggers[0], 100);
            if (!require(events.size() == 1 && events.back().pressed && events.back().action == controller_action::previous_tab,
                "a fresh trigger press works after changing focus")) return false;
        }

        std::vector<int32_t> mapping;
        ParseControllerValues("8,10,13,13,10,0,1,3,2,15,-1,-1,14,-33,-33,32,32,-36,-36,35,35,6,7,5,4,11,-1", mapping);
        controller_normalizer remapped;
        remapped.Configure(controller_backend::xinput, mapping);
        remapped.SetNavigationOnly(true);
        if (!require(remapped.Suppress(6, 1, true) && remapped.Suppress(7, 1, true) && !remapped.Suppress(16, 100, true) &&
            !remapped.Suppress(8, 1, true), "chat tabs follow remapped native macro controls")) return false;

        mapping[7] = 10;
        remapped.Configure(controller_backend::xinput, mapping);
        remapped.SetNavigationOnly(true);
        std::vector<controller_event> events;
        remapped.Raw(6, 1, 100, false, false, true, [&](const controller_event& event) { events.push_back(event); return false; });
        if (!require(events.size() == 1 && events.front().action == controller_action::confirm,
            "a conflicting native Confirm binding cannot also change chat tabs")) return false;

        mapping[1] = -36; mapping[3] = 36;
        mapping[5] = 2; mapping[6] = 0; mapping[7] = 1; mapping[8] = 3;
        mapping[21] = mapping[22] = 41; mapping[23] = mapping[24] = 40;
        remapped.Configure(controller_backend::direct_input, mapping);
        remapped.SetNavigationOnly(true);
        events.clear();
        const auto dispatch = [&](const controller_event& event) { events.push_back(event); return false; };
        remapped.Raw(16, -100, 100, false, false, true, dispatch);
        remapped.Raw(16, 0, 110, false, false, true, dispatch);
        remapped.Raw(16, 100, 120, false, false, true, dispatch);
        return require(events.size() == 3 && events.front().action == controller_action::previous_tab &&
            events.front().pressed && !events[1].pressed && events.back().action == controller_action::next_tab && events.back().pressed,
            "DirectInput shared-axis triggers select opposite chat tabs");
    }
}

auto main(void) -> int
{
    if (!CheckNativeWindowCycle() || !CheckMappings() || !CheckOwnership() || !CheckStateAndPov() || !CheckOpeningCapture() || !CheckNavigationCapture() || !CheckChatTabTriggers()) return 1;
    std::puts("Controller input checks passed.");
    return 0;
}
