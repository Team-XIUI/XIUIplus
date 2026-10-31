#pragma once

#include "../shared/controller_input.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

namespace xiui
{
    inline auto ParseControllerValues(std::string_view text, std::vector<int32_t>& values) -> bool
    {
        std::vector<int32_t> parsed;
        while (!text.empty() && parsed.size() < 128)
        {
            const auto separator = text.find(',');
            auto token = text.substr(0, separator);
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.remove_prefix(1);
            while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) token.remove_suffix(1);
            int32_t value{};
            const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
            if (token.empty() || result.ec != std::errc{} || result.ptr != token.data() + token.size()) return false;
            parsed.push_back(value);
            if (separator == std::string_view::npos) { values = std::move(parsed); return true; }
            text.remove_prefix(separator + 1);
        }
        return false;
    }

    class controller_normalizer
    {
    public:
        auto Configure(const controller_backend backend, const std::vector<int32_t>& mapping) -> void
        {
            *this = {};
            this->backend_ = backend;
            this->raw_[32] = -1;
            this->mapping_.fill(-1);
            if (mapping.size() >= this->mapping_.size())
                std::copy_n(mapping.begin(), this->mapping_.size(), this->mapping_.begin());
            else if (backend == controller_backend::xinput)
            {
                this->mapping_[1] = 9; this->mapping_[3] = 12;
                this->mapping_[5] = 0; this->mapping_[6] = 1; this->mapping_[7] = 3; this->mapping_[8] = 2;
                this->mapping_[21] = 6; this->mapping_[22] = 7; this->mapping_[23] = 5; this->mapping_[24] = 4;
            }
            else
            {
                this->mapping_[1] = 6; this->mapping_[3] = 7;
                this->mapping_[5] = 2; this->mapping_[6] = 0; this->mapping_[7] = 1; this->mapping_[8] = 3;
                this->mapping_[21] = this->mapping_[22] = 41;
                this->mapping_[23] = this->mapping_[24] = 40;
            }
            this->RebuildNavigationFields();
        }

        static auto XInputButton(const int32_t mapped) -> int32_t
        {
            constexpr std::array<int32_t, 16> events{13, 14, 15, 12, 3, 2, 0, 1, 8, 16, 6, 9, 17, 7, 4, 5};
            return mapped >= 0 && mapped < static_cast<int32_t>(events.size()) ? events[mapped] : -1;
        }

        template<typename Dispatch>
        auto Raw(const uint32_t field, const int32_t value, const uint64_t now, const bool injected,
            const bool blocked, const bool captured, Dispatch&& dispatch, const bool fromState = false) -> bool
        {
            const bool owned = this->Owns(field, captured);
            if (field >= this->raw_.size()) return owned;
            // An event-driven control's filtered state is not a hardware release.
            if (fromState && this->event_seen_[field]) return owned || this->draining_[field];
            if (!fromState && !this->event_seen_[field] && this->state_seen_[field] &&
                this->Active(field, this->raw_[field]) && !this->Active(field, value))
                return owned || this->draining_[field];
            if (fromState) this->state_seen_[field] = true;
            else this->event_seen_[field] = true;
            const bool wasDraining = this->draining_[field];
            this->raw_[field] = value;
            const bool active = this->Active(field, value);
            this->draining_[field] = active && (owned || wasDraining);
            bool consumed = owned || wasDraining;
            for (size_t index = 1; index < this->held_.size(); ++index)
            {
                const auto action = static_cast<controller_action>(index);
                const bool down = this->Down(action);
                if (down == this->held_[index]) continue;
                this->held_[index] = down;
                this->repeat_at_[index] = down && !blocked && owned ? now + 360 : 0;
                this->repeat_injected_[index] = injected;
                if (!blocked && (owned || !wasDraining))
                {
                    if (dispatch(controller_event{action, this->backend_, this->user_index_, down, false, injected, now}))
                    {
                        consumed = true;
                        if (active) this->draining_[field] = true;
                        if (down) this->repeat_at_[index] = now + 360;
                    }
                }
                else if (!down && !blocked)
                    dispatch(controller_event{action, this->backend_, this->user_index_, false, false, injected, now});
            }
            return consumed;
        }

        template<typename Dispatch>
        auto State(const uint32_t field, const int32_t value, const uint64_t now, const bool blocked,
            const bool captured, Dispatch&& dispatch) -> bool
        {
            return this->Raw(field, value, now, false, blocked, captured, std::forward<Dispatch>(dispatch), true);
        }

        template<typename Dispatch>
        auto Repeat(const uint64_t now, const bool captured, Dispatch&& dispatch) -> void
        {
            for (size_t index = static_cast<size_t>(controller_action::up); index <= static_cast<size_t>(controller_action::right); ++index)
            {
                if (!captured) { this->repeat_at_[index] = 0; continue; }
                if (!this->held_[index] || this->repeat_at_[index] == 0 || now < this->repeat_at_[index]) continue;
                this->repeat_at_[index] = now + 85;
                dispatch(controller_event{static_cast<controller_action>(index), this->backend_, this->user_index_, true, true, this->repeat_injected_[index], now});
            }
        }

        auto Suppress(const uint32_t field, const int32_t value, const bool captured) -> bool
        {
            const bool owned = this->Owns(field, captured);
            if (field >= this->draining_.size()) return owned;
            if (owned && this->Active(field, value)) this->draining_[field] = true;
            return owned || this->draining_[field];
        }

        auto NavigationOnly(void) const -> bool { return this->navigation_only_; }
        auto SetNativeWindowCycle(const bool enabled) -> void { this->native_cycle_ = enabled; }
        auto SetNavigationOnly(const bool enabled) -> void
        {
            if (this->navigation_only_ == enabled) return;
            this->navigation_only_ = enabled;
            this->RebuildNavigationFields();
            // Changing surfaces must not activate a tab from an already held control.
            for (const auto action : {controller_action::previous_tab, controller_action::next_tab})
            {
                const auto index = static_cast<size_t>(action);
                this->held_[index] = this->Down(action);
                this->repeat_at_[index] = 0;
            }
            if (enabled)
                for (size_t field = 0; field < this->draining_.size(); ++field)
                    if (!this->navigation_fields_[field]) this->draining_[field] = false;
        }

        auto Suspend(void) -> void
        {
            this->repeat_at_.fill(0);
        }

        auto SetUserIndex(const uint8_t user) -> void { this->user_index_ = user; }

    private:
        controller_backend backend_{};
        uint8_t user_index_{};
        std::array<int32_t, 27> mapping_{};
        std::array<int32_t, 176> raw_{};
        std::array<bool, 176> draining_{};
        std::array<bool, 176> event_seen_{};
        std::array<bool, 176> state_seen_{};
        std::array<bool, 12> held_{};
        std::array<uint64_t, 12> repeat_at_{};
        std::array<bool, 12> repeat_injected_{};
        std::array<bool, 176> navigation_fields_{};
        bool navigation_only_{};
        bool native_cycle_{};

        auto RebuildNavigationFields(void) -> void
        {
            this->navigation_fields_.fill(false);
            for (size_t index = 1; index < static_cast<size_t>(controller_action::focus); ++index)
                if (const auto field = this->Field(this->Binding(static_cast<controller_action>(index))); field >= 0)
                    this->navigation_fields_[field] = true;
        }

        auto Owns(const uint32_t field, const bool captured) const -> bool
        {
            if (this->native_cycle_ && static_cast<int32_t>(field) == this->Field(this->Binding(controller_action::cycle_window))) return false;
            return captured && (!this->navigation_only_ || (field < this->navigation_fields_.size() && this->navigation_fields_[field]));
        }

        auto Field(const int32_t binding) const -> int32_t
        {
            if (binding < -127 || binding > 127 || binding == -1) return -1;
            const auto mapped = std::abs(binding);
            if (this->backend_ == controller_backend::xinput)
            {
                if (mapped < 32) return XInputButton(binding);
                return mapped == 32 ? 18 : mapped == 33 ? 19 : mapped == 35 ? 20 : mapped == 36 ? 21 : -1;
            }
            if (mapped < 32 || mapped >= 42) return binding >= 0 ? 48 + binding : -1;
            if (mapped == 40 || mapped == 41) return 32;
            return (mapped - 32) * 4;
        }

        auto Active(const uint32_t field, const int32_t value) const -> bool
        {
            if (this->backend_ == controller_backend::direct_input)
            {
                if (field >= 32 && field < 48) return value != -1 && static_cast<uint32_t>(value) != 0xFFFFu;
                if (field < 32) return std::abs(value) > 42;
                return value != 0;
            }
            if (field >= 18 && field <= 21) return std::abs(value) > 10000;
            if (field == 16 || field == 17) return value > 30;
            return value != 0;
        }

        auto MappedDown(const int32_t binding, const controller_action action) const -> bool
        {
            const auto field = this->Field(binding);
            if (field < 0) return false;
            const auto mapped = std::abs(binding);
            if ((this->backend_ == controller_backend::xinput && field < 18) ||
                (this->backend_ == controller_backend::direct_input && field >= 48)) return this->Active(field, this->raw_[field]);

            const bool vertical = action == controller_action::up || action == controller_action::down;
            const bool backwards = action == controller_action::up || action == controller_action::left;
            int32_t value{};
            if (this->backend_ == controller_backend::direct_input && (mapped == 40 || mapped == 41))
            {
                const auto pov = this->raw_[32];
                if (pov < 0 || pov >= 36000) return false;
                if (mapped == 41) value = pov >= 13500 && pov <= 22500 ? 127 : pov <= 4500 || pov >= 31500 ? -127 : 0;
                else value = pov >= 4500 && pov <= 13500 ? 127 : pov >= 22500 && pov <= 31500 ? -127 : 0;
            }
            else
            {
                value = this->raw_[field];
                if (this->backend_ == controller_backend::xinput) value /= 258;
            }
            if (binding < 0) value = -value;
            if (action == controller_action::previous_tab || action == controller_action::next_tab)
                return value > 42;
            if (!vertical && action != controller_action::left && action != controller_action::right) return false;
            return backwards ? value < -42 : value > 42;
        }

        auto Binding(const controller_action action) const -> int32_t
        {
            const auto slot = action == controller_action::up ? 21 : action == controller_action::down ? 22 :
                action == controller_action::left ? 23 : action == controller_action::right ? 24 :
                action == controller_action::confirm ? 7 : action == controller_action::cancel ? 5 :
                action == controller_action::cycle_window ? 8 : action == controller_action::actions ? 6 : -1;
            if (slot >= 0) return this->mapping_[slot];
            if (action != controller_action::previous_tab && action != controller_action::next_tab) return -1;
            const auto binding = this->navigation_only_ ? this->mapping_[action == controller_action::previous_tab ? 1 : 3] :
                this->backend_ == controller_backend::xinput ?
                    (action == controller_action::previous_tab ? 8 : 11) : (action == controller_action::previous_tab ? 4 : 5);
            const auto field = this->Field(binding);
            if (field < 0) return -1;
            for (const auto mappedSlot : {5, 6, 7, 8, 21, 22, 23, 24})
                if (this->Field(this->mapping_[mappedSlot]) == field) return -1;
            if (this->navigation_only_ && this->mapping_[1] == this->mapping_[3]) return -1;
            return binding;
        }

        auto Down(const controller_action action) const -> bool
        { return this->MappedDown(this->Binding(action), action); }
    };
}
