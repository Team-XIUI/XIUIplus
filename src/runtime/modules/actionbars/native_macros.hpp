#pragma once

#include "action_model.hpp"
#include "runtime_api.hpp"

#include <cstdint>

namespace xiui
{
    class native_macros
    {
    public:
        auto Initialize(void) -> void;
        auto Available(void) const -> bool { return this->runner_ != nullptr || this->run_ != 0; }
        auto Entry(void) const -> uintptr_t { return this->run_; }
        auto SetRunEntry(uintptr_t entry) -> void { this->run_ = entry; }
        auto SetRunner(native_macro_runner runner, void* context) -> void { this->runner_ = runner; this->runner_context_ = context; }
        auto Run(const native_macro_ref& macro) const -> bool;

    private:
        uintptr_t container_slot_{}, controller_slot_{}, can_use_{}, run_{}, set_book_{}, set_page_{};
        native_macro_runner runner_{};
        void* runner_context_{};
    };
}
