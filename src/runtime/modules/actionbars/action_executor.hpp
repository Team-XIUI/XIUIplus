#pragma once

#include "action_model.hpp"

#include <functional>
#include <optional>
#include <span>
#include <unordered_map>

struct IAshitaCore;

namespace xiui
{
    struct action_guard_state
    {
        bool services{}, logged_in{}, zoning{}, has_character{}, has_zone{}, identity_valid{}, dead{};
        bool menu_open{}, action_target{}, subtarget{};
    };
    auto ActionGuardReason(const action_guard_state& state, bool presentation = false, bool allowMacroPalette = false) -> std::string;

    class action_executor
    {
    public:
        auto SetRestricted(bool restricted) -> void { this->restricted_ = restricted; if (restricted) this->Cancel(); }
        auto SetNativeMacroPalette(bool allowed) -> void { this->native_macro_palette_ = allowed; }
        auto Catalog(IAshitaCore* core) -> std::span<const action_catalog_entry>;
        auto Inspect(IAshitaCore* core, const action_slot& slot, bool presentation = false) -> action_availability;
        auto Activate(IAshitaCore* core, const action_slot& slot, uint64_t now) -> bool;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto Cancel(void) -> void;
        auto Status(void) const -> const std::string& { return this->status_; }
        auto Clear(void) -> void;
        auto SetNativeRunner(std::function<bool(const native_macro_ref&)> run) -> void { this->native_runner_ = std::move(run); }
        auto SetMacroResolver(std::function<action_slot(const action_slot&)> resolve) -> void { this->macro_resolver_ = std::move(resolve); }
        auto SetUtc(uint32_t utc) -> void { this->utc_ = utc; }
        static auto Commands(const action_slot& slot, const std::string& resourceName) -> std::vector<std::string>;
        static auto RepresentativeAction(const action_slot& slot) -> std::optional<action_slot>;

    private:
        friend struct action_executor_test;
        std::vector<action_catalog_entry> catalog_{};
        uint8_t job_{}, subjob_{}, level_{}, sublevel_{};
        uint32_t player_{};
        uint64_t next_catalog_{};
        bool restricted_{}, native_macro_palette_{};
        std::string status_{};
        std::function<bool(const native_macro_ref&)> native_runner_{};
        std::function<action_slot(const action_slot&)> macro_resolver_{};
        struct macro_step { std::string command{}; uint64_t delay{}; bool subtarget{}; };
        std::vector<macro_step> pending_{};
        size_t next_{};
        uint64_t due_{}, subtarget_deadline_{};
        uint32_t macro_player_{}, macro_zone_{};
        uint8_t macro_job_{}, macro_subjob_{};
        bool waiting_subtarget_{}, saw_subtarget_{};
        struct item_sample { uint32_t inventory{}, equipment{}, equipped{}, charges{}; float remaining{}; bool timed{}; };
        std::unordered_map<uint32_t, item_sample> items_{};
        uint64_t next_inventory_{};
        uint32_t utc_{};
        auto ReadItems(IAshitaCore* core) -> void;
    };
}
