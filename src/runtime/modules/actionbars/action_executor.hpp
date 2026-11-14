#pragma once

#include "action_model.hpp"

#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>

struct IAshitaCore;

namespace xiui
{
    struct action_guard_state
    {
        bool services{}, logged_in{}, zoning{}, has_character{}, has_zone{}, identity_valid{}, dead{};
        bool menu_open{}, action_target{}, subtarget{};
    };
    auto ActionGuardReason(const action_guard_state& state, bool presentation = false, bool allowNativeMenu = false) -> std::string;

    class action_executor
    {
    public:
        auto SetRestricted(bool restricted, std::string_view reason = "Macro cancelled because another window owns input.") -> void
        { this->restricted_ = restricted; if (restricted) this->Cancel(reason); }
        auto SetNativeMenuAllowed(bool allowed) -> void { this->native_menu_allowed_ = allowed; }
        auto SetNativeInputBusy(bool active) -> void { this->native_input_busy_ = active; }
        auto Catalog(IAshitaCore* core) -> std::span<const action_catalog_entry>;
        auto Inspect(IAshitaCore* core, const action_slot& slot, bool presentation = false) -> action_availability;
        // background lets this run continue while the game is unfocused; it ends with the run.
        auto Activate(IAshitaCore* core, const action_slot& slot, uint64_t now, bool background = false) -> bool;
        auto Update(IAshitaCore* core, uint64_t now) -> void;
        auto Cancel(std::string_view reason = {}) -> void;
        auto Status(void) const -> const std::string& { return this->status_; }
        auto Diagnostics(uint64_t now) const -> std::string;
        auto Background(void) const -> bool { return this->background_ && !this->pending_.empty(); }
        // Dispatch times of the latest command macro run, for automated checks.
        auto Timeline(void) const -> std::vector<std::string>;
        auto Clear(void) -> void;
        auto SetNativeRunner(std::function<bool(const native_macro_ref&)> run) -> void { this->native_runner_ = std::move(run); }
        auto SetMacroResolver(std::function<action_slot(const action_slot&)> resolve) -> void { this->macro_resolver_ = std::move(resolve); }
        auto SetUtc(uint32_t utc) -> void { this->utc_ = utc; }
        static auto Commands(const action_slot& slot, const std::string& resourceName) -> std::vector<std::string>;
        static auto ResolveCommandName(std::string command, const std::function<std::string(const action_slot&)>& resolve) -> std::string;
        static auto RepresentativeAction(const action_slot& slot) -> std::optional<action_slot>;
        static auto IconAction(const action_slot& slot) -> std::optional<action_slot>;
        auto SuggestedIcon(IAshitaCore* core, const action_slot& slot) const -> std::string;

    private:
        friend struct action_executor_test;
        std::vector<action_catalog_entry> catalog_{};
        uint8_t job_{}, subjob_{}, level_{}, sublevel_{};
        uint32_t player_{};
        uint64_t next_catalog_{};
        bool restricted_{}, native_menu_allowed_{}, native_input_busy_{};
        std::string status_{};
        std::string last_command_{};
        uint64_t last_dispatch_{};
        size_t last_line_{}, last_total_{};
        std::function<bool(const native_macro_ref&)> native_runner_{};
        std::function<action_slot(const action_slot&)> macro_resolver_{};
        struct macro_step { std::string command{}; uint64_t delay{}; bool subtarget{}; int mode{-1}; };
        std::vector<macro_step> pending_{};
        size_t next_{};
        uint64_t due_{}, subtarget_deadline_{};
        uint32_t macro_player_{}, macro_zone_{};
        uint8_t macro_job_{}, macro_subjob_{};
        bool waiting_subtarget_{}, saw_subtarget_{}, background_{};
        struct dispatch_record { uint64_t at{}; size_t line{}; std::string command{}; };
        std::vector<dispatch_record> timeline_{};
        uint64_t timeline_start_{};
        size_t timeline_lines_{};
        bool timeline_timed_{}, timeline_running_{};
        std::string timeline_end_{};
        auto Record(size_t line, const std::string& command, uint64_t now) -> void
        { if (this->timeline_.size() < 64) this->timeline_.push_back({now - this->timeline_start_, line, command}); }
        struct item_sample { uint32_t inventory{}, equipment{}, equipped{}, charges{}; float remaining{}; bool timed{}; };
        std::unordered_map<uint32_t, item_sample> items_{};
        uint64_t next_inventory_{};
        uint32_t utc_{};
        auto StepReady(uint64_t now) const -> bool
        { return now >= this->due_ && (!this->native_input_busy_ || this->next_ >= this->pending_.size() || this->pending_[this->next_].command.empty()); }
        auto BuildSteps(const action_slot& slot, const std::vector<std::string>& commands,
            const std::function<std::string(const action_slot&)>& resolveName, bool& timed) -> std::vector<macro_step>;
        auto ReadItems(IAshitaCore* core) -> void;
    };
}
