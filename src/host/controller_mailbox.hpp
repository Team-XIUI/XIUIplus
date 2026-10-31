#pragma once

#include "../shared/controller_input.hpp"

#include <array>
#include <cstddef>
#include <mutex>

namespace xiui
{
    // Never hold this lock while calling the runtime or a native UI function.
    class controller_mailbox
    {
    public:
        struct batch
        {
            std::array<controller_event, 64> events{};
            size_t count{};
            uint64_t generation{};
            controller_status status{};
            bool has_status{};
        };

        auto State(const bool menuOpen = false) const -> uint32_t
        {
            std::lock_guard lock(this->mutex_);
            return FilterControllerMenu(this->state_, menuOpen);
        }

        auto Submit(const controller_event& event, const bool menuOpen = false) -> bool
        {
            std::lock_guard lock(this->mutex_);
            auto state = FilterControllerMenu(this->state_, menuOpen);
            if (event.action == controller_action::none || event.action > controller_action::palette_next) return false;
            if (event.action == controller_action::cycle_window && (state & controller_native_cycle) != 0) return false;
            if (event.action > controller_action::focus)
            {
                if (event.repeated || menuOpen || !this->crossbar_settings_.enabled || !this->crossbar_settings_.replace_native ||
                    (state & controller_capture_mask) != 0) return false;
                if (event.action == controller_action::crossbar_bank)
                {
                    if (event.bank > crossbar_bank::right_double || event.slot != 0 || event.triggers > 3 ||
                        event.pressed != (event.bank != crossbar_bank::none)) return false;
                    this->crossbar_bank_ = event.bank;
                }
                else if (event.action == controller_action::crossbar_slot)
                {
                    if (event.slot < 1 || event.slot > 8 || event.bank == crossbar_bank::none || event.bank > crossbar_bank::right_double) return false;
                    auto& held = this->crossbar_held_[static_cast<size_t>(event.bank) - 1][event.slot - 1];
                    if (event.pressed ? held || event.bank != this->crossbar_bank_ : !held) return false;
                    held = event.pressed;
                }
                else
                {
                    auto& held = this->palette_held_[event.action == controller_action::palette_next ? 1 : 0];
                    if (event.pressed ? held || this->crossbar_bank_ == crossbar_bank::none : !held) return false;
                    held = event.pressed;
                }
                if (this->pending_.count < this->pending_.events.size()) this->pending_.events[this->pending_.count++] = event;
                return true;
            }
            const bool captured = (state & controller_capture_mask) != 0;
            if (!captured && (!event.pressed || event.repeated || (state & ControllerEntryBit(event.action)) == 0)) return false;
            if (this->pending_.count < this->pending_.events.size())
                this->pending_.events[this->pending_.count++] = event;
            // Owned input remains suppressed even if the bounded queue is full.
            this->state_ = Predict(state, event);
            return true;
        }

        auto Status(const controller_status& status) -> void
        {
            std::lock_guard lock(this->mutex_);
            if (status.suspended) this->ResetLocked();
            if (!status.suspended && this->pending_.has_status && this->pending_.status.suspended) return;
            this->pending_.status = status;
            this->pending_.has_status = true;
        }

        auto Take(void) -> batch
        {
            std::lock_guard lock(this->mutex_);
            auto result = this->pending_;
            result.generation = this->generation_;
            this->pending_ = {};
            return result;
        }

        auto Current(const uint64_t generation) const -> bool
        {
            std::lock_guard lock(this->mutex_);
            return generation == this->generation_;
        }

        auto Publish(uint32_t state) -> void
        {
            std::lock_guard lock(this->mutex_);
            if ((state & controller_revision_mask) != this->revision_)
            {
                this->revision_ = state & controller_revision_mask;
                this->ResetLocked();
            }
            for (size_t index = 0; index < this->pending_.count; ++index) state = Predict(state, this->pending_.events[index]);
            this->state_ = state;
        }

        auto Reset(void) -> void
        {
            std::lock_guard lock(this->mutex_);
            this->ResetLocked();
        }

        auto CrossbarSettings(void) const -> controller_crossbar_settings
        {
            std::lock_guard lock(this->mutex_);
            return this->crossbar_settings_;
        }

        auto CrossbarSettings(const controller_crossbar_settings& settings) -> void
        {
            std::lock_guard lock(this->mutex_);
            if (settings == this->crossbar_settings_) return;
            this->crossbar_settings_ = settings;
            this->ResetLocked();
        }

    private:
        mutable std::mutex mutex_{};
        batch pending_{};
        uint64_t generation_{};
        uint32_t state_{};
        uint32_t revision_{};
        controller_crossbar_settings crossbar_settings_{};
        crossbar_bank crossbar_bank_{};
        std::array<std::array<bool, 8>, 6> crossbar_held_{};
        std::array<bool, 2> palette_held_{};

        static auto Predict(uint32_t state, const controller_event& event) -> uint32_t
        {
            if ((state & controller_capture_mask) == 0 && event.pressed && !event.repeated &&
                (state & ControllerEntryBit(event.action)) != 0)
            {
                const auto mode = event.action == controller_action::cycle_window ? controller_capture_mode::navigation : controller_capture_mode::exclusive;
                state &= ~controller_party_input;
                state |= static_cast<uint32_t>(mode);
                if (event.action == controller_action::up || event.action == controller_action::down) state |= controller_party_input;
            }
            return state;
        }

        auto ResetLocked(void) -> void
        {
            const bool suspended = this->pending_.has_status && this->pending_.status.suspended;
            const auto status = this->pending_.status;
            this->pending_ = {};
            if (suspended) { this->pending_.status = status; this->pending_.has_status = true; }
            this->state_ = this->revision_;
            this->crossbar_bank_ = crossbar_bank::none;
            this->crossbar_held_ = {};
            this->palette_held_ = {};
            ++this->generation_;
        }
    };
}
