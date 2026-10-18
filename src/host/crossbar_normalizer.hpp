#pragma once

#include "controller_normalizer.hpp"

namespace xiui
{
    class crossbar_normalizer
    {
    public:
        auto Configure(const controller_backend backend, const std::vector<int32_t>& nativeMapping,
            const controller_crossbar_settings& settings) -> void
        {
            const bool preserve = this->backend_ == backend && backend != controller_backend::none;
            const auto previousRaw = this->raw_;
            const auto previousDrain = this->draining_;
            const auto previousEvents = this->event_seen_;
            const auto previousStates = this->state_seen_;
            *this = {};
            this->backend_ = backend;
            this->settings_ = settings;
            for (size_t field = 32; field < 48; field += 4) this->raw_[field] = -1;
            if (preserve)
            {
                this->raw_ = previousRaw;
                this->draining_ = previousDrain;
                this->event_seen_ = previousEvents;
                this->state_seen_ = previousStates;
            }
            const auto& overrides = backend == controller_backend::xinput ? settings.xinput : settings.direct_input;
            const auto resolve = [&](const int16_t value, const size_t slot) -> int16_t {
                if (value != -2) return value;
                return nativeMapping.size() > slot && nativeMapping[slot] >= -127 && nativeMapping[slot] <= 127 ?
                    static_cast<int16_t>(nativeMapping[slot]) : -1;
            };
            this->native_fields_[0] = this->Field(resolve(-2, 1), false);
            this->native_fields_[1] = this->Field(resolve(-2, 3), false);
            this->mapping_.left_trigger = resolve(overrides.left_trigger, 1);
            this->mapping_.right_trigger = resolve(overrides.right_trigger, 3);
            this->mapping_.palette_modifier = overrides.palette_modifier != -2 ? overrides.palette_modifier :
                static_cast<int16_t>(backend == controller_backend::xinput ? (settings.palette_left_shoulder ? 8 : 11) :
                    (settings.palette_left_shoulder ? 4 : 5));
            constexpr std::array<size_t, 8> nativeSlots{21, 24, 22, 23, 8, 5, 7, 6};
            for (size_t index = 0; index < nativeSlots.size(); ++index)
                this->mapping_.slots[index] = resolve(overrides.slots[index], nativeSlots[index]);
            this->fields_[0] = this->Field(this->mapping_.left_trigger, false);
            this->fields_[1] = this->Field(this->mapping_.right_trigger, false);
            this->fields_[10] = this->Field(this->mapping_.palette_modifier, false);
            this->ready_ = this->fields_[0] >= 0 && this->fields_[1] >= 0 && this->fields_[0] != this->fields_[1];
            for (size_t index = 0; index < nativeSlots.size(); ++index)
            {
                this->fields_[index + 2] = this->Field(this->mapping_.slots[index], index < 4);
                const auto field = this->fields_[index + 2];
                this->ready_ = this->ready_ && field >= 0 && field != this->fields_[0] && field != this->fields_[1];
                for (size_t other = 0; other < index; ++other)
                {
                    if (field >= 48 || (backend == controller_backend::xinput && field >= 0))
                        this->ready_ = this->ready_ && field != this->fields_[other + 2];
                    if (backend == controller_backend::direct_input && field == 32 && this->fields_[other + 2] == 32)
                        this->ready_ = this->ready_ && this->PovAngle(index) != this->PovAngle(other);
                }
            }
            if (settings.palette_cycle)
            {
                this->ready_ = this->ready_ && this->fields_[10] >= 0 && this->fields_[10] != this->fields_[0] && this->fields_[10] != this->fields_[1];
                for (size_t index = 2; index < 10; ++index)
                    this->ready_ = this->ready_ && this->fields_[10] != this->fields_[index];
            }
        }

        auto Mapping(void) const -> const controller_crossbar_mapping& { return this->mapping_; }
        auto Ready(void) const -> bool { return this->ready_; }
        auto Bank(void) const -> crossbar_bank { return this->bank_; }
        auto Values(void) const -> const std::array<int32_t, 176>& { return this->raw_; }
        auto SetUserIndex(const uint8_t user) -> void { this->user_index_ = user; }

        auto ResetDevice(void) -> void
        {
            this->raw_.fill(0);
            for (size_t field = 32; field < 48; field += 4) this->raw_[field] = -1;
            this->event_seen_.fill(false); this->state_seen_.fill(false); this->draining_.fill(false);
            this->slots_down_.fill(false); this->require_neutral_.fill(true);
            this->slot_banks_.fill(crossbar_bank::none); this->palette_held_.fill(controller_action::none);
            this->last_release_.fill(0); this->press_at_.fill(0);
            this->bank_ = crossbar_bank::none;
            this->left_ = this->right_ = this->armed_ = false;
        }

        template<typename Dispatch>
        auto Raw(const uint32_t field, const int32_t value, const uint64_t now, const bool injected,
            const bool blocked, const bool eligible, Dispatch&& dispatch, const bool fromState = false) -> bool
        {
            if (field >= this->raw_.size() || !this->Related(field)) return false;
            if (fromState && this->event_seen_[field]) return this->Owned(field, eligible && !blocked) || this->draining_[field];
            if (!fromState && !this->event_seen_[field] && this->state_seen_[field] &&
                this->Active(field, this->raw_[field]) && !this->Active(field, value))
                return this->Owned(field, eligible && !blocked) || this->draining_[field];
            if (fromState) this->state_seen_[field] = true;
            else this->event_seen_[field] = true;
            const bool draining = this->draining_[field];
            this->raw_[field] = value;
            this->draining_[field] = draining && this->Active(field, value);
            const bool nativeOwned = !blocked && this->NativeOwned(field, eligible);
            if (nativeOwned && this->Active(field, value)) this->draining_[field] = true;
            if (blocked || !eligible || !this->ready_ || !this->settings_.enabled || !this->settings_.replace_native)
            {
                this->Suspend(now, dispatch);
                this->SnapshotSlots();
                return draining || nativeOwned;
            }
            const bool left = this->TriggerDown(0, this->left_);
            const bool right = this->TriggerDown(1, this->right_);
            if (!this->armed_)
            {
                this->SnapshotSlots();
                if (!left && !right && std::none_of(this->slots_down_.begin(), this->slots_down_.end(), [](const bool down) { return down; }))
                    this->armed_ = true;
                return draining || this->Claim(field, eligible);
            }

            const auto previousBank = this->bank_;
            const auto previousTriggers = this->Triggers();
            const auto nextBank = this->UpdateBank(left, right, now);
            if (nextBank != previousBank || this->Triggers() != previousTriggers)
            {
                this->ReleaseSlots(now, dispatch);
                this->bank_ = nextBank;
                for (size_t index = 0; index < this->slots_down_.size(); ++index)
                    this->require_neutral_[index] = this->slots_down_[index] && this->SlotDown(index);
                dispatch(this->Event(controller_action::crossbar_bank, now, this->bank_ != crossbar_bank::none));
            }

            for (size_t index = 0; index < this->slots_down_.size(); ++index)
            {
                const bool down = this->SlotDown(index);
                const bool pressed = down && !this->slots_down_[index];
                this->slots_down_[index] = down;
                if (!down)
                {
                    this->require_neutral_[index] = false;
                    this->ReleaseSlot(index, now, dispatch);
                }
                if (!pressed || this->require_neutral_[index] || this->bank_ == crossbar_bank::none) continue;
                if (this->settings_.palette_cycle && (index == 0 || index == 2) && this->ButtonDown(10))
                {
                    this->ReleaseSlots(now, dispatch);
                    for (size_t other = 0; other < this->slots_down_.size(); ++other)
                        this->require_neutral_[other] = this->SlotDown(other);
                    const auto action = index == 0 ? controller_action::palette_previous : controller_action::palette_next;
                    if (dispatch(this->Event(action, now, true, 0, injected))) this->palette_held_[index] = action;
                    continue;
                }
                auto event = this->Event(controller_action::crossbar_slot, now, true, static_cast<uint8_t>(index + 1), injected);
                if (dispatch(event)) this->slot_banks_[index] = event.bank;
            }
            return draining || this->Claim(field, eligible);
        }

        template<typename Dispatch>
        auto State(const uint32_t field, const int32_t value, const uint64_t now, const bool blocked,
            const bool eligible, Dispatch&& dispatch) -> bool
        { return this->Raw(field, value, now, false, blocked, eligible, std::forward<Dispatch>(dispatch), true); }

        template<typename Dispatch>
        auto State(const std::array<int32_t, 176>& values, const uint64_t now, const bool blocked,
            const bool eligible, Dispatch&& dispatch) -> void
        {
            uint32_t evaluate = static_cast<uint32_t>(values.size());
            for (uint32_t field = 0; field < values.size(); ++field)
            {
                if (!this->Related(field) || this->event_seen_[field]) continue;
                this->raw_[field] = values[field];
                this->state_seen_[field] = true;
                this->draining_[field] = this->draining_[field] && this->Active(field, values[field]);
                evaluate = field;
            }
            // Resolve the complete packet before deciding a bank or button edge.
            if (evaluate < values.size()) this->Raw(evaluate, values[evaluate], now, false, blocked, eligible, dispatch, true);
        }

        auto Suppress(const uint32_t field, const int32_t value, const bool eligible) -> bool
        {
            if (field >= this->raw_.size() || !this->Related(field)) return false;
            const bool owned = this->Owned(field, eligible);
            if (owned && this->Active(field, value)) this->draining_[field] = true;
            return owned || this->draining_[field];
        }

        template<typename Dispatch>
        auto Suspend(const uint64_t now, Dispatch&& dispatch) -> void
        {
            this->ReleaseSlots(now, dispatch);
            if (this->bank_ != crossbar_bank::none)
            {
                this->bank_ = crossbar_bank::none;
                this->left_ = this->right_ = false;
                dispatch(this->Event(controller_action::crossbar_bank, now, false));
            }
            this->left_ = this->right_ = this->armed_ = false;
            this->last_release_.fill(0);
            this->press_at_.fill(0);
            this->require_neutral_.fill(true);
        }

    private:
        controller_backend backend_{};
        uint8_t user_index_{};
        controller_crossbar_settings settings_{};
        controller_crossbar_mapping mapping_{};
        std::array<int32_t, 2> native_fields_{-1, -1};
        std::array<int32_t, 11> fields_{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
        std::array<int32_t, 176> raw_{};
        std::array<bool, 176> event_seen_{}, state_seen_{}, draining_{};
        std::array<bool, 8> slots_down_{}, require_neutral_{};
        std::array<crossbar_bank, 8> slot_banks_{};
        std::array<controller_action, 8> palette_held_{};
        std::array<uint64_t, 2> last_release_{}, press_at_{};
        crossbar_bank bank_{};
        bool ready_{}, armed_{}, left_{}, right_{};

        auto Field(const int16_t binding, const bool direction) const -> int32_t
        {
            if (binding < -127 || binding > 127 || binding == -1) return -1;
            const auto mapped = std::abs(static_cast<int32_t>(binding));
            if (this->backend_ == controller_backend::xinput) return binding >= 0 ? controller_normalizer::XInputButton(binding) : -1;
            if (mapped == 40 || mapped == 41) return direction ? 32 : -1;
            if (mapped >= 32 && mapped <= 41) return -1;
            return binding >= 0 && 48 + binding < static_cast<int32_t>(this->raw_.size()) ? 48 + binding : -1;
        }

        auto Related(const uint32_t field) const -> bool
        { return (field < this->draining_.size() && this->draining_[field]) ||
            std::find(this->native_fields_.begin(), this->native_fields_.end(), static_cast<int32_t>(field)) != this->native_fields_.end() ||
            std::find(this->fields_.begin(), this->fields_.end(), static_cast<int32_t>(field)) != this->fields_.end(); }

        auto Active(const uint32_t field, const int32_t value) const -> bool
        {
            if (this->backend_ == controller_backend::direct_input && field >= 32 && field < 48)
                return value >= 0 && value < 36000;
            const auto press = std::clamp<uint16_t>(this->settings_.trigger_press, 1, 255);
            const auto release = std::clamp<uint16_t>(this->settings_.trigger_release, 1, press);
            return this->backend_ == controller_backend::xinput && (field == 16 || field == 17) ? value >= release : value != 0;
        }

        auto ButtonDown(const size_t control) const -> bool
        { return this->fields_[control] >= 0 && this->Active(static_cast<uint32_t>(this->fields_[control]), this->raw_[this->fields_[control]]); }

        auto TriggerDown(const size_t index, const bool held) const -> bool
        {
            const auto field = this->fields_[index];
            if (field < 0) return false;
            if (this->backend_ != controller_backend::xinput || (field != 16 && field != 17)) return this->raw_[field] != 0;
            const auto press = std::clamp<uint16_t>(this->settings_.trigger_press, 1, 255);
            const auto release = std::clamp<uint16_t>(this->settings_.trigger_release, 1, press);
            return this->raw_[field] >= (held ? release : press);
        }

        auto SlotDown(const size_t index) const -> bool
        {
            const auto field = this->fields_[index + 2];
            if (field < 0) return false;
            if (this->backend_ != controller_backend::direct_input || field != 32) return this->Active(static_cast<uint32_t>(field), this->raw_[field]);
            const auto pov = this->raw_[field];
            if (pov < 0 || pov >= 36000 || index >= 4) return false;
            return pov == this->PovAngle(index);
        }

        auto PovAngle(const size_t index) const -> int32_t
        {
            const auto binding = this->mapping_.slots[index];
            const bool reverse = binding < 0;
            const bool backwards = index == 0 || index == 3;
            const bool negative = backwards != reverse;
            return std::abs(static_cast<int32_t>(binding)) == 41 ? (negative ? 0 : 18000) : (negative ? 27000 : 9000);
        }

        auto Owned(const uint32_t field, const bool eligible) const -> bool
        {
            if (this->NativeOwned(field, eligible)) return true;
            if (!eligible || !this->ready_ || !this->settings_.enabled || !this->settings_.replace_native) return false;
            if (static_cast<int32_t>(field) == this->fields_[0] || static_cast<int32_t>(field) == this->fields_[1]) return true;
            if (!this->armed_ || this->bank_ == crossbar_bank::none) return false;
            for (size_t control = 2; control < 10; ++control)
                if (static_cast<int32_t>(field) == this->fields_[control]) return true;
            return this->settings_.palette_cycle && static_cast<int32_t>(field) == this->fields_[10];
        }

        auto NativeOwned(const uint32_t field, const bool eligible) const -> bool
        {
            return eligible && this->settings_.suppress_native_macros &&
                std::find(this->native_fields_.begin(), this->native_fields_.end(), static_cast<int32_t>(field)) != this->native_fields_.end();
        }

        auto Claim(const uint32_t field, const bool eligible) -> bool
        {
            const bool owned = this->Owned(field, eligible);
            if (owned && this->Active(field, this->raw_[field])) this->draining_[field] = true;
            return owned;
        }

        auto Triggers(void) const -> uint8_t { return (this->left_ ? 1 : 0) | (this->right_ ? 2 : 0); }

        auto Event(const controller_action action, const uint64_t now, const bool pressed,
            const uint8_t slot = 0, const bool injected = false) const -> controller_event
        {
            controller_event event{action, this->backend_, this->user_index_, pressed, false, injected, now};
            event.bank = this->bank_; event.slot = slot; event.triggers = this->Triggers();
            return event;
        }

        auto SnapshotSlots(void) -> void
        {
            for (size_t index = 0; index < this->slots_down_.size(); ++index) this->slots_down_[index] = this->SlotDown(index);
        }

        template<typename Dispatch>
        auto ReleaseSlot(const size_t index, const uint64_t now, Dispatch&& dispatch) -> void
        {
            if (this->slot_banks_[index] != crossbar_bank::none)
            {
                auto event = this->Event(controller_action::crossbar_slot, now, false, static_cast<uint8_t>(index + 1));
                event.bank = this->slot_banks_[index];
                dispatch(event);
                this->slot_banks_[index] = crossbar_bank::none;
            }
            if (this->palette_held_[index] != controller_action::none)
            {
                dispatch(this->Event(this->palette_held_[index], now, false));
                this->palette_held_[index] = controller_action::none;
            }
        }

        template<typename Dispatch>
        auto ReleaseSlots(const uint64_t now, Dispatch&& dispatch) -> void
        { for (size_t index = 0; index < this->slot_banks_.size(); ++index) this->ReleaseSlot(index, now, dispatch); }

        auto UpdateBank(const bool left, const bool right, const uint64_t now) -> crossbar_bank
        {
            const bool leftPressed = left && !this->left_, rightPressed = right && !this->right_;
            for (size_t index = 0; index < 2; ++index)
            {
                const bool wasHeld = index == 0 ? this->left_ : this->right_;
                const bool held = index == 0 ? left : right;
                if (wasHeld && !held)
                    this->last_release_[index] = now >= this->press_at_[index] && now - this->press_at_[index] >= this->settings_.min_hold_ms ? now : 0;
                if (held && !wasHeld) this->press_at_[index] = now;
            }
            this->left_ = left; this->right_ = right;
            if (!left && !right) return crossbar_bank::none;
            if (left && right)
            {
                if (!this->settings_.expanded) return this->bank_ != crossbar_bank::none ? this->bank_ : crossbar_bank::left;
                if (leftPressed && !rightPressed) return crossbar_bank::right_left;
                if (rightPressed) return crossbar_bank::left_right;
                return this->bank_;
            }
            const size_t index = left ? 0 : 1;
            const bool pressed = left ? leftPressed : rightPressed;
            const auto normal = left ? crossbar_bank::left : crossbar_bank::right;
            const auto twice = left ? crossbar_bank::left_double : crossbar_bank::right_double;
            if (!pressed) return this->bank_ == twice ? twice : normal;
            const auto release = this->last_release_[index];
            return this->settings_.double_tap && this->bank_ == crossbar_bank::none && release != 0 &&
                now >= release && now - release <= this->settings_.double_tap_ms ? twice : normal;
        }
    };
}
