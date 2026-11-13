#pragma once

#include "action_model.hpp"
#include "settings/settings_store.hpp"

#include <span>

namespace xiui
{
    class action_store
    {
    public:
        action_store();
        auto Load(const settings_store& settings) -> void;
        auto Save(const settings_store& settings) const -> void;
        auto SetContext(action_context context) -> void;
        auto Context(void) const -> const action_context& { return this->context_; }
        auto Scope(action_surface surface, size_t bar = 0) const -> action_scope;
        auto SetScope(action_surface surface, size_t bar, action_scope scope) -> void;
        // Under job + subjob storage a subjob uses the job's shared palettes until it is given its own.
        auto SubjobSet(action_surface surface, size_t bar = 0) const -> action_subjob_set;
        auto SeparateSubjob(action_surface surface, size_t bar = 0) -> bool;
        // Fails without discard when shared palettes already exist and the subjob's own would be removed.
        auto ShareSubjob(action_surface surface, size_t bar = 0, bool discard = false) -> bool;
        auto PetAware(action_surface surface, size_t bar = 0) const -> bool;
        auto SetPetAware(action_surface surface, size_t bar, bool enabled) -> void;
        auto Palettes(action_surface surface, size_t bar = 0) const -> std::vector<const action_palette*>;
        auto AllPalettes(void) const -> std::span<const action_palette> { return this->palettes_; }
        auto ActivePalette(action_surface surface, size_t bar = 0) const -> const action_palette*;
        auto Select(action_surface surface, const std::string& id, size_t bar = 0) -> bool;
        auto Create(std::string name, action_surface surface = action_surface::hotbar, size_t bar = 0) -> std::string;
        auto Rename(const std::string& id, std::string name) -> bool;
        auto Copy(const std::string& id, std::string name) -> std::string;
        auto Move(const std::string& id, int delta) -> bool;
        auto SetPaletteOrder(action_surface surface, std::vector<std::string> ids) -> void;
        auto PaletteOrder(action_surface surface) const -> const std::vector<std::string>& { return this->orders_[static_cast<size_t>(surface)]; }
        auto Delete(const std::string& id) -> bool;
        auto Cycle(action_surface surface, int delta, size_t bar = 0) -> bool;
        auto Resolve(action_surface surface, size_t bar, size_t slot) const -> const action_slot&;
        auto Assign(action_surface surface, size_t bar, size_t slot, action_slot action, bool petOverride = false) -> bool;
        auto Swap(action_surface source, size_t sourceBar, size_t sourceSlot,
            action_surface destination, size_t destinationBar, size_t destinationSlot) -> bool;
        auto ImportPalette(action_palette palette, action_surface surface, size_t bar = 0, bool sourceContext = false) -> std::string;
        auto Changed(void) -> bool;
        auto Revision(void) const -> uint64_t { return this->revision_; }
        auto Macros(void) const -> std::span<const action_macro> { return this->macros_; }
        auto Macro(const std::string& id) const -> const action_macro*;
        auto CreateMacro(std::string name, std::vector<std::string> lines, std::string icon = {}) -> std::string;
        auto SaveMacro(const action_macro& macro) -> bool;
        auto ImportMacro(action_macro macro) -> std::string;
        auto DeleteMacro(const std::string& id) -> bool;
        auto ExpandMacro(const action_slot& slot) const -> action_slot;

    private:
        struct selection { uint8_t job{}, subjob{}; action_surface surface{}; std::string id{}; };
        action_context context_{};
        std::vector<action_palette> palettes_{};
        std::vector<selection> selections_{};
        std::vector<action_macro> macros_{};
        std::array<std::vector<std::string>, 2> orders_{};
        std::array<action_scope, action_bar_count> hotbar_scope_{};
        std::array<action_scope, action_crossbar_banks> crossbar_scope_{};
        std::array<bool, action_bar_count> hotbar_pet_{};
        std::array<bool, action_crossbar_banks> crossbar_pet_{};
        uint64_t sequence_{1}, revision_{};
        bool changed_{};
        auto Key(action_surface surface, size_t bar) const -> std::pair<uint8_t, uint8_t>;
        auto Home(action_surface surface, size_t bar) const -> std::pair<uint8_t, uint8_t>;
        auto Has(action_surface surface, uint8_t job, uint8_t subjob) const -> bool;
        auto Touch(void) -> void;
        auto Ensure(action_surface surface, size_t bar) -> void;
    };
}
