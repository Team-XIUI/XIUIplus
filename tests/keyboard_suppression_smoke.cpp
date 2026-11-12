#include "keyboard_suppression.hpp"

#include <array>
#include <cstdio>

namespace
{
    constexpr uint32_t digit = 3;
    constexpr uint32_t control = 0x1d;
    struct entry
    {
        uint32_t scan{}, value{}, timestamp{17}, sequence{23}, extra{31};
    };

    auto require(const bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "Keyboard suppression check failed: %s\n", message);
        return condition;
    }

    template<size_t Size>
    auto filter(xiui::keyboard_suppression& input, std::array<entry, Size>& data, const bool capture = false, const bool peek = false, const int32_t result = 0) -> void
    {
        input.FilterData(data.data(), sizeof(entry), data.size(), capture, peek, result);
    }

    auto CheckQuickTap(void) -> bool
    {
        xiui::keyboard_suppression input;
        if (!require(input.Observe(digit, true, false, true) && input.Observe(digit, false, true, false),
            "owned WM press and release stay owned")) return false;

        std::array<uint8_t, 256> state{};
        input.FilterState(state.data(), state.size(), false);
        std::array<entry, 2> data{{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        if (!require(data[0].value == 0 && data[1].value == 0 && data[0].timestamp == 17 && data[1].sequence == 23 && data[1].extra == 31,
            "WM up and a neutral state read cannot release buffered ownership")) return false;

        input.Observe(digit, true, false, false);
        input.Observe(digit, false, true, false);
        data = {{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        return require(data[0].value == 0x80 && data[1].value == 0x01, "next unassigned same-digit gesture passes");
    }

    auto CheckMixedBufferedGestures(void) -> bool
    {
        xiui::keyboard_suppression input;
        input.Observe(control, true, false, false);
        input.Observe(digit, true, false, true);
        input.Observe(digit, false, true, false);
        input.Observe(digit, true, false, false);

        std::array<uint8_t, 256> state{};
        state[control] = state[digit] = 0x80;
        input.FilterState(state.data(), state.size(), false);
        if (!require(state[control] == 0x80 && state[digit] == 0x80, "fresh unassigned gesture and generic Ctrl remain native")) return false;

        input.Observe(digit, false, true, false);
        std::array<entry, 5> data{{{control, 0x80}, {digit, 0x80}, {digit, 0x01}, {digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        return require(data[0].value == 0x80 && data[1].value == 0 && data[2].value == 0 && data[3].value == 0x80 && data[4].value == 0x01,
            "one buffer distinguishes an owned tap from a later unassigned tap");
    }

    auto CheckIndependentStateDrain(void) -> bool
    {
        xiui::keyboard_suppression input;
        input.Observe(digit, true, false, true);
        input.Observe(digit, false, true, false);
        std::array<entry, 2> data{{{digit, 0x80}, {digit, 0}}};
        filter(input, data);

        std::array<uint8_t, 256> state{};
        state[digit] = 0x80;
        state[control] = 0x80;
        input.FilterState(state.data(), state.size(), false);
        if (!require(state[digit] == 0 && state[control] == 0x80, "buffered release does not clear immediate-state drainage")) return false;
        input.FilterState(state.data(), state.size(), false);
        state[digit] = 0x80;
        input.FilterState(state.data(), state.size(), false);
        return require(state[digit] == 0x80, "immediate ownership ends at neutral");
    }

    auto CheckPeekAndFailures(void) -> bool
    {
        xiui::keyboard_suppression input;
        input.Observe(digit, true, false, true);
        input.Observe(digit, false, true, false);
        input.Observe(digit, true, false, false);
        input.Observe(digit, false, true, false);
        const std::array<entry, 4> raw{{{digit, 0x80}, {digit, 0x01}, {digit, 0x80}, {digit, 0x01}}};
        auto data = raw;
        filter(input, data, false, false, -1);
        if (!require(data[0].value == 0x80 && data[1].value == 0x01, "failed HRESULT leaves data untouched")) return false;

        for (int attempt = 0; attempt < 2; ++attempt)
        {
            data = raw;
            filter(input, data, false, true);
            if (!require(data[0].value == 0 && data[1].value == 0 && data[2].value == 0x80 && data[3].value == 0x01,
                "repeated peek does not consume gesture provenance")) return false;
        }
        data = raw;
        filter(input, data, false, false, 1);
        if (!require(data[0].value == 0 && data[1].value == 0 && data[2].value == 0x80 && data[3].value == 0x01,
            "successful buffer-overflow read consumes the same history after peek")) return false;

        data = raw;
        filter(input, data);
        return require(data[0].value == 0x80 && data[1].value == 0x01, "committed read drains the buffered history once");
    }

    auto CheckModalAndLifecycle(void) -> bool
    {
        xiui::keyboard_suppression input;
        std::array<uint8_t, 256> state{};
        state[control] = state[digit] = 0x80;
        input.FilterState(state.data(), state.size(), true);
        if (!require(state[control] == 0 && state[digit] == 0, "full modal capture still suppresses all keys")) return false;
        state[control] = state[digit] = 0x80;
        input.FilterState(state.data(), state.size(), false);
        if (!require(state[control] == 0 && state[digit] == 0, "modal state holds drain after focus closes")) return false;
        input.FilterState(state.data(), state.size(), false);
        state[control] = 0x80;
        input.FilterState(state.data(), state.size(), false);
        if (!require(state[control] == 0x80, "neutral returns modal-held Ctrl to native input")) return false;

        input.Observe(digit, true, false, true);
        input.Suspend();
        if (!require(input.Observe(digit, true, true, false) && input.Observe(digit, false, true, false),
            "suspension drains owned repeats and the eventual WM release")) return false;
        std::array<entry, 2> data{{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        if (!require(data[0].value == 0 && data[1].value == 0, "suspension preserves already-owned buffered edges")) return false;

        input.Observe(digit, true, false, true);
        input.Reset();
        if (!require(!input.Observe(digit, true, false, false), "new plugin lifetime has no stale WM owner")) return false;
        data = {{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        return require(data[0].value == 0x80 && data[1].value == 0x01, "reset clears buffered ownership");
    }

    auto CheckRepeatsAndFormats(void) -> bool
    {
        xiui::keyboard_suppression input;
        input.Observe(digit, true, false, false);
        input.Observe(digit, true, true, true);
        for (int repeat = 0; repeat < 10; ++repeat) input.Observe(digit, true, true, false);
        input.Observe(digit, false, true, false);
        input.Observe(digit, true, false, false);
        input.Observe(digit, false, true, false);
        struct short_entry { uint32_t scan{}, value{}; };
        std::array<short_entry, 4> data{{{digit, 0x80}, {digit, 0x01}, {digit, 0x80}, {digit, 0x01}}};
        input.FilterData(data.data(), sizeof(short_entry), data.size(), false, false, 0);
        if (!require(data[0].value == 0 && data[1].value == 0 && data[2].value == 0x80 && data[3].value == 0x01,
            "WM repeat upgrades the current gesture without adding buffered edges")) return false;

        input.Reset();
        std::array<entry, 2> modal{{{control, 0x80}, {digit, 0x80}}};
        filter(input, modal, true);
        std::array<entry, 3> released{{{control, 0x01}, {digit, 0x01}, {digit, 0x80}}};
        filter(input, released);
        if (!require(modal[0].value == 0 && modal[1].value == 0 && released[0].value == 0 && released[1].value == 0 && released[2].value == 0x80,
            "modal buffered owners drain releases without capturing the next gesture")) return false;

        return require(xiui::keyboard_suppression::ScanCode(0x001d0000, 99) == 0x1d &&
            xiui::keyboard_suppression::ScanCode(0x011d0000, 99) == 0x9d &&
            xiui::keyboard_suppression::ScanCode(0, 99) == 99, "physical scan distinguishes both modifier sides with a mapping fallback");
    }

    auto CheckBoundedHistory(void) -> bool
    {
        xiui::keyboard_suppression input;
        input.Observe(digit, true, false, true);
        input.Observe(digit, false, true, false);
        for (int gesture = 0; gesture < 80; ++gesture)
        {
            input.Observe(digit, true, false, false);
            input.Observe(digit, false, true, false);
        }
        std::array<entry, 2> data{{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        if (!require(data[0].value == 0x80 && data[1].value == 0x01, "bounded history cannot leave a stale owner on a known native gesture")) return false;
        input.Reset();
        input.Observe(digit, true, false, true);
        input.Observe(digit, false, true, false);
        data = {{{digit, 0x80}, {digit, 0x01}}};
        filter(input, data);
        return require(data[0].value == 0 && data[1].value == 0, "lifecycle reset removes overflow history");
    }

    auto CheckPreemptiveHistory(void) -> bool
    {
        using namespace xiui;
        constexpr uint32_t enter = 0x1C, arrow = 0xC8, movement = 0x48, cycle = 0x21;
        keyboard_suppression input;
        std::array<uint8_t, 256> state{};
        state[enter] = state[arrow] = state[movement] = state[digit] = 0x80;
        input.FilterState(state.data(), state.size(), keyboard_capture_history);
        if (!require(state[enter] == 0 && state[arrow] == 0 && state[movement] == 0x80 && state[digit] == 0x80,
            "history blocks Enter and navigation before WM delivery while preserving movement and typing")) return false;
        std::array<entry, 3> first{{{enter, 0x80}, {movement, 0x80}, {digit, 0x80}}};
        input.FilterData(first.data(), sizeof(entry), first.size(), keyboard_capture_history, false, 0);
        if (!require(first[0].value == 0 && first[1].value == 0x80 && first[2].value == 0x80,
            "buffered Enter cannot open the native log before XIUI+ receives its key event")) return false;
        input.Observe(enter, true, false, true);
        input.Observe(enter, false, false, false);
        std::array<entry, 1> release{{{enter, 0x01}}};
        filter(input, release);
        state[enter] = 0x80;
        input.FilterState(state.data(), state.size(), 0);
        if (!require(release[0].value == 0 && state[enter] == 0, "closing history drains Enter from buffered and immediate input")) return false;
        state.fill(0); input.FilterState(state.data(), state.size(), 0);
        input.Observe(enter, true, false, false); input.Observe(enter, false, false, false);
        std::array<entry, 2> native{{{enter, 0x80}, {enter, 0x01}}}; filter(input, native);
        if (!require(native[0].value == 0x80 && native[1].value == 0x01, "a fresh Enter returns to native attack/menu controls after history closes")) return false;
        input.Reset();
        state.fill(0); state[cycle] = state[enter] = 0x80;
        input.FilterState(state.data(), state.size(), keyboard_capture_cycle);
        if (!require(state[cycle] == 0 && state[enter] == 0x80, "compact F entry is reserved before it can select native chat")) return false;
        if (!require(KeyboardCapturesScan(keyboard_capture_numpad_cycle, 0x4E) && !KeyboardCapturesScan(keyboard_capture_numpad_cycle, cycle) &&
            !KeyboardCapturesScan(keyboard_capture_cycle, 0x4E) && KeyboardCapturesScan(keyboard_capture_status, 0x4E) &&
            !KeyboardCapturesScan(FilterKeyboardCapture(keyboard_capture_numpad_cycle, true), 0x4E),
            "numpad + is reserved for XIUI+'s window cycle in every layout and stays native with modifiers")) return false;
        return require(!KeyboardCapturesScan(FilterKeyboardCapture(keyboard_capture_cycle, true), cycle) &&
            KeyboardCapturesScan(keyboard_capture_status, 0x0F) && !KeyboardCapturesScan(keyboard_capture_history, 0x0F),
            "modified F shortcuts and Alt+Tab stay native while status focus owns its tab navigation");
    }
}

auto main(void) -> int
{
    if (!CheckQuickTap() || !CheckMixedBufferedGestures() || !CheckIndependentStateDrain() || !CheckPeekAndFailures() ||
        !CheckModalAndLifecycle() || !CheckRepeatsAndFormats() || !CheckBoundedHistory() || !CheckPreemptiveHistory()) return 1;
    std::puts("Keyboard suppression checks passed.");
    return 0;
}
