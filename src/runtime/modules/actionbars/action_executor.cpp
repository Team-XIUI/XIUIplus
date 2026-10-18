#include "action_executor.hpp"
#include "chat/chat_encoding.hpp"
#include "combat/ability_recast.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

namespace
{
    auto PetType(const uint8_t type) -> bool { return type == 2 || type == 6 || type == 10 || type == 18; }
    auto Name(const char* const* names) -> std::string
    { const auto* name = names[2] != nullptr ? names[2] : names[0]; return name != nullptr ? xiui::FromGameText(name) : std::string{}; }
    auto Guard(IAshitaCore* core, const bool presentation = false, const bool allowMacroPalette = false) -> std::string
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        xiui::action_guard_state state{};
        state.services = player != nullptr && party != nullptr && target != nullptr && entity != nullptr && core->GetResourceManager() != nullptr && core->GetChatManager() != nullptr;
        if (!state.services) return xiui::ActionGuardReason(state, presentation, allowMacroPalette);
        state.logged_in = player->GetLoginStatus() == static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn);
        state.zoning = player->GetIsZoning() != 0; state.has_character = party->GetMemberServerId(0) != 0; state.has_zone = party->GetMemberZone(0) != 0;
        if (!state.logged_in || state.zoning || !state.has_character || !state.has_zone) return xiui::ActionGuardReason(state, presentation, allowMacroPalette);
        const auto index = party->GetMemberTargetIndex(0);
        state.identity_valid = index != 0 && index < entity->GetEntityMapSize() && entity->GetServerId(index) == party->GetMemberServerId(0);
        if (!state.identity_valid) return xiui::ActionGuardReason(state, presentation, allowMacroPalette);
        state.dead = entity->GetStatus(index) == 4 || party->GetMemberHP(0) == 0;
        state.menu_open = target->GetIsMenuOpen() != 0; state.action_target = target->GetActionTargetActive() != 0; state.subtarget = target->GetIsSubTargetActive() != 0;
        return xiui::ActionGuardReason(state, presentation, allowMacroPalette);
    }
    auto SpellKnown(IPlayer* player, const ISpell& spell) -> bool
    {
        if (!player->HasSpellData() || !player->HasSpell(spell.Index)) return false;
        const auto usable = [&](uint8_t job, uint8_t level, bool main) {
            if (job == 0 || job >= 24) return false;
            const auto required = spell.LevelRequired[job];
            if (required < 0) return false;
            if ((spell.JobPointMask & (1u << job)) != 0) return main && level >= 99 && player->GetJobPointsSpent(job) >= required;
            return required > 0 && required < 255 && level >= required;
        };
        return usable(player->GetMainJob(), player->GetMainJobLevel(), true) || usable(player->GetSubJob(), player->GetSubJobLevel(), false);
    }
    auto EquipmentIndex(const std::string& slot) -> int
    {
        if (slot == "ear1") return 11; if (slot == "ear2") return 12; if (slot == "ring1") return 13; if (slot == "ring2") return 14;
        constexpr const char* names[]{"main", "sub", "range", "ammo", "head", "body", "hands", "legs", "feet", "neck", "waist", "lear", "rear", "lring", "rring", "back"};
        for (int index = 0; index < 16; ++index) if (slot == names[index]) return index;
        return -1;
    }
    auto Lower(std::string value) -> std::string
    { std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); }); return value; }
    auto Trim(std::string value) -> std::string
    { const auto start = value.find_first_not_of(" \t"); return start == std::string::npos ? std::string{} : value.substr(start, value.find_last_not_of(" \t") - start + 1); }
    auto IsSubtarget(const std::string& command) -> bool
    { return command.find("<st>") != std::string::npos || command.find("<stpc>") != std::string::npos || command.find("<stnpc>") != std::string::npos || command.find("<stpt>") != std::string::npos || command.find("<stal>") != std::string::npos; }
    auto WaitMilliseconds(const std::string& text) -> uint64_t
    {
        if (text.empty()) return 1000;
        char* end{}; const auto seconds = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || *end != '\0' || !std::isfinite(seconds) || seconds < 0 || seconds > 3600) return UINT64_MAX;
        return static_cast<uint64_t>(std::ceil(seconds * 1000));
    }
    auto QueueMode(const std::string& command) -> int
    {
        const auto key = Lower(command.substr(0, command.find(' ')));
        return key == "/ma" || key == "/magic" || key == "/ja" || key == "/jobability" || key == "/ws" || key == "/weaponskill" || key == "/pet" || key == "/item" ? 2 : -1;
    }
}

namespace xiui
{
    auto ActionGuardReason(const action_guard_state& state, const bool presentation, const bool allowMacroPalette) -> std::string
    {
        if (!state.services) return "Action services are unavailable.";
        if (!state.logged_in || state.zoning || !state.has_character || !state.has_zone) return "Wait until your character has finished loading.";
        if (!state.identity_valid) return "Your character is unavailable.";
        if (state.dead) return "Actions are unavailable while KO'd.";
        if (!presentation && (state.action_target || state.subtarget || (state.menu_open && !allowMacroPalette))) return "Finish the native menu or target selection first.";
        return {};
    }
    auto action_executor::Cancel(void) -> void
    { this->pending_.clear(); this->next_ = 0; this->waiting_subtarget_ = false; this->saw_subtarget_ = false; this->due_ = 0; }
    auto action_executor::Clear(void) -> void
    { this->Cancel(); this->catalog_.clear(); this->items_.clear(); this->next_inventory_ = 0; this->player_ = 0; this->native_macro_palette_ = false; this->status_.clear(); }
    auto action_executor::ReadItems(IAshitaCore* core) -> void
    {
        const auto now = GetTickCount64(); if (now < this->next_inventory_) return;
        this->next_inventory_ = now + 100; this->items_.clear();
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* inventory = memory != nullptr ? memory->GetInventory() : nullptr; if (inventory == nullptr) return;
        std::set<uint32_t> equipped;
        for (uint32_t slot = 0; slot < 16; ++slot) if (const auto* item = inventory->GetEquippedItem(slot)) equipped.insert(item->Index);
        constexpr uint8_t bags[]{0, 3, 8, 10, 11, 12, 13, 14, 15, 16};
        for (const auto bag : bags)
        {
            if ((inventory->GetContainerUpdateFlags() & (1u << bag)) == 0) continue;
            const auto maximum = std::min<uint32_t>(80, inventory->GetContainerCountMax(bag));
            for (uint32_t slot = 1; slot <= maximum; ++slot)
                if (const auto* item = inventory->GetContainerItem(bag, slot); item != nullptr && item->Id != 0 && item->Count != 0)
                {
                    auto& sample = this->items_[item->Id];
                    if (bag == 0 || bag == 3) sample.inventory += item->Count;
                    if (bag != 3) sample.equipment += item->Count;
                    if (equipped.contains(static_cast<uint32_t>(bag) << 8 | slot) && item->Extra[0] == 1)
                    {
                        ++sample.equipped; sample.charges = std::max<uint32_t>(sample.charges, item->Extra[1]);
                        if (this->utc_ > 0x3C307D70u)
                        {
                            uint32_t use{}, equip{}; std::memcpy(&use, item->Extra + 4, 4); if (item->Flags == 5) std::memcpy(&equip, item->Extra + 8, 4);
                            const auto latest = std::max(use, equip);
                            const auto remaining = latest > 0 && static_cast<uint64_t>(latest) + 0x3C307D70u > this->utc_ ? static_cast<float>(static_cast<uint64_t>(latest) + 0x3C307D70u - this->utc_) : 0;
                            sample.remaining = sample.timed ? std::min(sample.remaining, remaining) : remaining; sample.timed = true;
                        }
                    }
                }
        }
    }
    auto action_executor::Catalog(IAshitaCore* core) -> std::span<const action_catalog_entry>
    {
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        auto* resources = core != nullptr ? core->GetResourceManager() : nullptr;
        if (player == nullptr || party == nullptr || resources == nullptr || player->GetIsZoning() || player->GetMainJob() == 0) { this->catalog_.clear(); return this->catalog_; }
        const auto now = GetTickCount64();
        if (!this->catalog_.empty() && this->player_ == party->GetMemberServerId(0) && this->job_ == player->GetMainJob() && this->subjob_ == player->GetSubJob() && this->level_ == player->GetMainJobLevel() && this->sublevel_ == player->GetSubJobLevel() && now < this->next_catalog_) return this->catalog_;
        this->next_catalog_ = now + 2000;
        this->player_ = party->GetMemberServerId(0); this->job_ = player->GetMainJob(); this->subjob_ = player->GetSubJob(); this->level_ = player->GetMainJobLevel(); this->sublevel_ = player->GetSubJobLevel();
        this->catalog_.clear();
        for (uint32_t id = 1; id < 2048; ++id)
            if (const auto* spell = resources->GetSpellById(id); spell != nullptr)
            {
                auto name = Name(spell->Name); if (name.empty()) continue;
                this->catalog_.push_back({action_kind::spell, id, std::move(name), "spell:" + std::to_string(id), spell->ManaCost, 0, id, SpellKnown(player, *spell)});
            }
        for (uint32_t id = 1; id < 0x700; ++id)
            if (const auto* ability = resources->GetAbilityById(id); ability != nullptr && ability->Type != 4 && ability->Type != 20)
            {
                constexpr uint32_t stubs[]{567, 603, 609, 636, 684, 694, 695, 710, 711, 712, 725, 735, 763, 775, 869, 891, 892};
                if (std::find(std::begin(stubs), std::end(stubs), id) != std::end(stubs)) continue;
                auto name = Name(ability->Name); if (name.empty()) continue;
                const auto kind = id < 256 ? action_kind::weapon_skill : PetType(ability->Type) ? action_kind::pet : action_kind::ability;
                const auto known = kind == action_kind::weapon_skill ? player->HasWeaponSkill(id) : player->HasAbility(id) || player->HasPetCommand(id);
                this->catalog_.push_back({kind, id, std::move(name), "ability:" + std::to_string(id), kind == action_kind::weapon_skill ? 0u : ability->ManaCost, kind == action_kind::weapon_skill ? 1000u : static_cast<uint32_t>(std::clamp<int>(ability->TPCost, 0, 3000)), ability->RecastTimerId, known});
            }
        if (auto* inventory = memory->GetInventory())
        {
            std::set<uint32_t> seen;
            for (uint8_t bag = 0; bag < 17; ++bag)
            {
                if ((inventory->GetContainerUpdateFlags() & (1u << bag)) == 0) continue;
                const auto maximum = std::min<uint32_t>(80, inventory->GetContainerCountMax(bag));
                for (uint32_t slot = 1; slot <= maximum; ++slot)
                    if (const auto* native = inventory->GetContainerItem(bag, slot); native != nullptr && native->Id != 0 && native->Count != 0 && seen.insert(native->Id).second)
                        if (const auto* item = resources->GetItemById(native->Id))
                        {
                            const bool gear = item->Type == 4 || item->Type == 5;
                            auto name = Name(item->Name); if (name.empty()) continue;
                            this->catalog_.push_back({gear ? action_kind::equip : action_kind::item, native->Id, std::move(name), "item:" + std::to_string(native->Id), 0, 0, 0, true});
                            if (gear && item->MaxCharges != 0) this->catalog_.push_back({action_kind::item, native->Id, Name(item->Name), "item:" + std::to_string(native->Id), 0, 0, 0, true});
                        }
            }
        }
        std::stable_sort(this->catalog_.begin(), this->catalog_.end(), [](const auto& a, const auto& b) { return a.kind != b.kind ? a.kind < b.kind : a.name < b.name; });
        return this->catalog_;
    }
    auto action_executor::Inspect(IAshitaCore* core, const action_slot& slot, const bool presentation) -> action_availability
    {
        if (!slot.macro_id.empty())
        {
            const auto expanded = this->macro_resolver_ ? this->macro_resolver_(slot) : slot;
            if (expanded.macro_id.empty()) return this->Inspect(core, expanded, presentation);
            action_availability missing{}; missing.name = slot.label.empty() ? slot.name : slot.label; missing.reason = "The referenced macro is unavailable."; return missing;
        }
        action_availability result{}; result.name = slot.label.empty() ? slot.name : slot.label; result.icon = slot.icon;
        result.reason = ValidateAction(slot); if (!result.reason.empty() || slot.Empty()) return result;
        if (!presentation && this->restricted_) { result.reason = "Close the editor or finish typing first."; return result; }
        result.reason = Guard(core, presentation, this->native_macro_palette_); if (!result.reason.empty()) return result;
        auto* memory = core->GetMemoryManager(); auto* player = memory->GetPlayer(); auto* resources = core->GetResourceManager(); auto* party = memory->GetParty(); auto* recast = memory->GetRecast();
        const auto index = party->GetMemberTargetIndex(0); uint32_t timer{}, chargeCost{}; float base{};
        if (slot.kind == action_kind::macro || slot.kind == action_kind::command)
        {
            result.known = result.learned = result.usable = true;
            if (presentation && !slot.recast_disabled)
                if (const auto representative = slot.recast_kind != action_kind::none ? std::optional<action_slot>{[&] { action_slot value{}; value.kind = slot.recast_kind; value.id = slot.recast_id; value.name = slot.recast_name; return value; }()} : RepresentativeAction(slot))
                {
                    auto metadata = this->Inspect(core, *representative, true);
                    if (metadata.known)
                    {
                        if (!result.name.empty()) metadata.name = result.name;
                        if (!result.icon.empty()) metadata.icon = result.icon;
                        if (slot.always_available) { metadata.reason.clear(); metadata.learned = metadata.usable = true; }
                        return metadata;
                    }
                }
            if (result.name.empty()) result.name = ActionKindName(slot.kind);
            return result;
        }
        if (slot.kind == action_kind::native_macro)
        { result.known = result.learned = true; result.usable = static_cast<bool>(this->native_runner_); if (!result.usable) result.reason = "The native macro bridge is unavailable."; if (result.name.empty()) result.name = "Native macro"; return result; }
        std::string gameName;
        if (!slot.name.empty() && !ToGameText(slot.name, gameName)) { result.reason = "The action name cannot be encoded for the game."; return result; }
        if (slot.kind == action_kind::spell)
        {
            const auto* spell = slot.id != 0 ? resources->GetSpellById(slot.id) : resources->GetSpellByName(gameName.c_str(), 2);
            if (spell == nullptr) { result.reason = "Spell resource is unavailable."; return result; }
            result.name = slot.label.empty() ? Name(spell->Name) : slot.label; result.known = true; result.learned = SpellKnown(player, *spell);
            result.mp = spell->ManaCost; base = spell->RecastDelay / 4.0f;
            if (result.icon.empty()) result.icon = "spell:" + std::to_string(spell->Index);
            if (recast != nullptr) { result.remaining = recast->GetSpellTimer(spell->Index) / 60.0f; result.recast_known = true; }
        }
        else if (slot.kind == action_kind::ability || slot.kind == action_kind::weapon_skill || slot.kind == action_kind::pet)
        {
            const auto* ability = slot.id != 0 ? resources->GetAbilityById(slot.id) : resources->GetAbilityByName(gameName.c_str(), 2);
            if (ability == nullptr) { result.reason = "Ability resource is unavailable."; return result; }
            result.name = slot.label.empty() ? Name(ability->Name) : slot.label; result.known = true;
            if (result.icon.empty()) result.icon = "ability:" + std::to_string(ability->Id);
            result.learned = slot.kind == action_kind::weapon_skill ? player->HasWeaponSkill(ability->Id) : player->HasAbility(ability->Id) || player->HasPetCommand(ability->Id);
            if (slot.kind == action_kind::weapon_skill) result.tp = 1000;
            else
            {
                result.tp = static_cast<uint32_t>(std::clamp<int>(ability->TPCost, 0, 3000)); timer = ability->RecastTimerId;
                if (timer == 102 || timer == 195 || timer == 231) chargeCost = std::clamp<uint32_t>(ability->ManaCost, 1, 3); else result.mp = ability->ManaCost;
                if (slot.kind == action_kind::pet && memory->GetEntity()->GetPetTargetIndex(index) == 0) { result.reason = "An active pet is required."; return result; }
                if (recast != nullptr)
                {
                    result.recast_known = true;
                    uint32_t frames{};
                    int16_t modifier{};
                    for (uint32_t entry = 0; entry < 32; ++entry)
                        if (recast->GetAbilityTimerId(entry) == timer)
                        {
                            frames = recast->GetAbilityTimer(entry);
                            modifier = static_cast<int16_t>(recast->GetAbilityCalc2(entry));
                            base = static_cast<float>(recast->GetAbilityRecast(entry));
                            break;
                        }
                    result.remaining = frames / 60.0f;
                    const auto pool = AbilityChargePool(core, timer);
                    if (timer == 102 && pool.maximum == 0) chargeCost = 0;
                    if (pool.maximum != 0)
                    {
                        const auto charges = CalculateChargeRecast(frames, pool, modifier);
                        result.charges = charges.charges;
                        result.maximum_charges = pool.maximum;
                        result.remaining = charges.remaining;
                        result.total = charges.duration;
                    }
                }
            }
        }
        else if (slot.kind == action_kind::item || slot.kind == action_kind::equip)
        {
            const auto* item = slot.id != 0 ? resources->GetItemById(slot.id) : resources->GetItemByName(gameName.c_str(), 2);
            if (item == nullptr) { result.reason = "Item resource is unavailable."; return result; }
            result.known = result.learned = true; result.name = slot.label.empty() ? Name(item->Name) : slot.label;
            if (result.icon.empty()) result.icon = "item:" + std::to_string(item->Id);
            this->ReadItems(core); const auto sample = this->items_[item->Id];
            const bool enchanted = (item->Type == 4 || item->Type == 5) && item->MaxCharges != 0;
            result.item_count = slot.kind == action_kind::equip ? sample.equipment : enchanted ? sample.equipped : sample.inventory;
            if (result.item_count == 0) { result.reason = slot.kind == action_kind::equip ? "This equipment is not in Inventory or a Wardrobe." : enchanted ? "Equip this enchanted item before using it." : "This item is not in Inventory or temporary items."; return result; }
            if (slot.kind == action_kind::equip)
            {
                const auto equipment = EquipmentIndex(Lower(slot.equipment_slot));
                if (equipment < 0 || (item->Slots & (1u << equipment)) == 0) { result.reason = "This item does not fit the selected equipment slot."; return result; }
                if (item->Level > player->GetMainJobLevel() || (item->Jobs & (1u << player->GetMainJob())) == 0) { result.reason = "Your current job or level cannot equip this item."; return result; }
            }
            else if (enchanted)
            {
                result.charges = item->MaxCharges == 255 ? 1 : sample.charges; result.maximum_charges = item->MaxCharges;
                result.remaining = sample.remaining; result.recast_known = sample.timed;
                if (result.charges == 0) { result.reason = "This item has no charges remaining."; return result; }
                if (sample.equipped == 0) { result.reason = "Equip this enchanted item before using it."; return result; }
            }
            else if (item->Type != 7 || (item->Flags & 0x200) == 0) { result.reason = "This item cannot be used."; return result; }
        }
        result.total = std::max({result.total, base, result.remaining});
        if (!result.learned) result.reason = "This action is unavailable for your current job or level.";
        else if (result.mp > party->GetMemberMP(0)) result.reason = "Not enough MP.";
        else if (result.tp > party->GetMemberTP(0)) result.reason = "Not enough TP.";
        else if (chargeCost != 0 && (result.maximum_charges == 0 || result.charges < chargeCost)) result.reason = "Not enough charges.";
        else if (chargeCost == 0 && result.remaining > 0) result.reason = "This action is on cooldown.";
        else if (slot.target.size() == 4 && slot.target[0] == '<' && slot.target[1] == 'p' && slot.target[2] >= '0' && slot.target[2] <= '5' && party->GetMemberServerId(slot.target[2] - '0') == 0) result.reason = "That party member is unavailable.";
        result.usable = result.reason.empty(); return result;
    }
    auto action_executor::RepresentativeAction(const action_slot& slot) -> std::optional<action_slot>
    {
        if (slot.kind != action_kind::macro && slot.kind != action_kind::command) return std::nullopt;
        for (const auto& source : slot.lines)
        {
            const auto line = Trim(source); const auto separator = line.find_first_of(" \t");
            if (separator == std::string::npos) continue;
            const auto verb = Lower(line.substr(0, separator));
            action_slot action{};
            if (verb == "/ma" || verb == "/magic") action.kind = action_kind::spell;
            else if (verb == "/ja" || verb == "/jobability") action.kind = action_kind::ability;
            else if (verb == "/ws" || verb == "/weaponskill") action.kind = action_kind::weapon_skill;
            else if (verb == "/pet") action.kind = action_kind::pet;
            else if (verb == "/item") action.kind = action_kind::item;
            else continue;
            auto arguments = Trim(line.substr(separator + 1));
            if (arguments.empty()) continue;
            if (arguments.front() == '"' || arguments.front() == '\'')
            {
                const auto end = arguments.find(arguments.front(), 1);
                if (end == std::string::npos || (end + 1 < arguments.size() && arguments[end + 1] != ' ' && arguments[end + 1] != '\t')) continue;
                action.name = Trim(arguments.substr(1, end - 1));
                arguments = Trim(arguments.substr(end + 1));
            }
            else
            {
                const auto target = arguments.find('<');
                action.name = Trim(arguments.substr(0, target));
                arguments = target == std::string::npos ? std::string{} : arguments.substr(target);
            }
            if (action.name.empty() || action.name.find_first_of("\"<>\r\n") != std::string::npos) continue;
            action.target.clear();
            if (!arguments.empty() && arguments.front() == '<')
            {
                const auto end = arguments.find('>');
                if (end != std::string::npos)
                {
                    action.target = Lower(arguments.substr(0, end + 1));
                    if (!ValidateAction(action).empty()) action.target.clear();
                }
            }
            if (ValidateAction(action).empty()) return action;
        }
        return std::nullopt;
    }
    auto action_executor::Commands(const action_slot& slot, const std::string& resourceName) -> std::vector<std::string>
    {
        if (!ValidateAction(slot).empty() || slot.Empty()) return {};
        if (slot.kind == action_kind::macro || slot.kind == action_kind::command) return slot.lines;
        if (slot.kind == action_kind::native_macro) return {};
        if (resourceName.empty() || resourceName.find_first_of("\"\r\n") != std::string::npos) return {};
        const char* prefix = slot.kind == action_kind::spell ? "/ma" : slot.kind == action_kind::weapon_skill ? "/ws" : slot.kind == action_kind::pet ? "/pet" : slot.kind == action_kind::item ? "/item" : slot.kind == action_kind::equip ? "/equip" : "/ja";
        return {std::string{prefix} + (slot.kind == action_kind::equip ? " " + Lower(slot.equipment_slot) : "") + " \"" + resourceName + "\"" + (slot.kind != action_kind::equip && !slot.target.empty() ? " " + slot.target : "")};
    }
    auto action_executor::Activate(IAshitaCore* core, const action_slot& slot, uint64_t now) -> bool
    {
        if (this->restricted_) { this->status_ = "Close the editor or finish typing first."; return false; }
        if (!slot.macro_id.empty())
        {
            const auto expanded = this->macro_resolver_ ? this->macro_resolver_(slot) : slot;
            if (expanded.macro_id.empty()) return this->Activate(core, expanded, now);
            this->status_ = "The referenced macro is unavailable."; return false;
        }
        if (slot.kind == action_kind::item || slot.kind == action_kind::equip) this->next_inventory_ = 0;
        const auto available = this->Inspect(core, slot); this->status_ = available.reason;
        if (!available.usable) return false;
        if (slot.kind == action_kind::native_macro)
        { this->Cancel(); const bool ok = this->native_runner_ && this->native_runner_(slot.native); if (!ok) this->status_ = "The native macro could not run."; return ok; }
        std::string resourceName = available.name;
        if (!slot.label.empty())
        { auto plain = slot; plain.label.clear(); resourceName = this->Inspect(core, plain).name; }
        const auto commands = Commands(slot, resourceName);
        if (commands.empty()) { this->status_ = "No valid commands are assigned."; return false; }
        std::vector<macro_step> steps;
        bool timed = slot.kind == action_kind::command && commands.size() > 1;
        for (auto command : commands)
        {
            command = Trim(std::move(command)); if (command.empty()) continue;
            const auto lower = Lower(command); const auto end = lower.find_first_of(" \t"); const auto verb = lower.substr(0, end);
            macro_step step{};
            if (verb == "/wait" || verb == "/pause" || verb == "/sleep")
            { step.delay = WaitMilliseconds(end == std::string::npos ? std::string{} : Trim(command.substr(end + 1))); timed = true; }
            else
            {
                if (const auto wait = lower.rfind("<wait "); wait != std::string::npos && lower.back() == '>')
                { step.delay = WaitMilliseconds(Trim(command.substr(wait + 6, command.size() - wait - 7))); command = Trim(command.substr(0, wait)); timed = true; }
                step.command = std::move(command); step.subtarget = IsSubtarget(Lower(step.command)); timed = timed || step.subtarget;
            }
            if (step.delay == UINT64_MAX) { this->status_ = "Waits must be between 0 and 3600 seconds."; return false; }
            std::string encoded;
            if (!step.command.empty() && (!ToGameText(step.command, encoded) || encoded.size() > 240)) { this->status_ = "A command exceeds the game's input limit or cannot be encoded."; return false; }
            step.command = std::move(encoded); steps.push_back(std::move(step));
        }
        if (steps.empty()) return false;
        this->Cancel();
        if (!timed)
        {
            // Native macro commands use a LIFO stack, including non-action lines.
            for (auto step = steps.rbegin(); step != steps.rend(); ++step) if (!step->command.empty()) core->GetChatManager()->QueueCommand(slot.kind == action_kind::macro ? 2 : QueueMode(step->command), step->command.c_str());
            return true;
        }
        auto* memory = core->GetMemoryManager(); auto* party = memory->GetParty(); auto* player = memory->GetPlayer();
        this->macro_player_ = party->GetMemberServerId(0); this->macro_zone_ = party->GetMemberZone(0); this->macro_job_ = player->GetMainJob(); this->macro_subjob_ = player->GetSubJob();
        this->pending_ = std::move(steps); this->due_ = now; this->Update(core, now); return true;
    }
    auto action_executor::Update(IAshitaCore* core, uint64_t now) -> void
    {
        if (this->pending_.empty()) return;
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr; auto* party = memory != nullptr ? memory->GetParty() : nullptr; auto* player = memory != nullptr ? memory->GetPlayer() : nullptr; auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (this->restricted_ || party == nullptr || player == nullptr || target == nullptr || player->GetIsZoning() || party->GetMemberServerId(0) != this->macro_player_ || party->GetMemberZone(0) != this->macro_zone_ || player->GetMainJob() != this->macro_job_ || player->GetSubJob() != this->macro_subjob_)
        { this->Cancel(); this->status_ = "Macro cancelled because the input or character context changed."; return; }
        if (this->waiting_subtarget_)
        {
            const bool active = target->GetIsSubTargetActive() || target->GetActionTargetActive();
            this->saw_subtarget_ = this->saw_subtarget_ || active;
            if (active)
            { if (now >= this->subtarget_deadline_) { this->Cancel(); this->status_ = "Macro cancelled after target selection timed out."; } return; }
            if (!this->saw_subtarget_ && now < this->due_) return;
            this->waiting_subtarget_ = false; this->due_ = now + this->pending_[this->next_ - 1].delay;
        }
        if (now < this->due_) return;
        const auto guard = Guard(core, false, this->native_macro_palette_); if (!guard.empty()) { this->Cancel(); this->status_ = guard; return; }
        if (this->next_ >= this->pending_.size()) { this->Cancel(); return; }
        const auto& step = this->pending_[this->next_++];
        if (!step.command.empty()) core->GetChatManager()->QueueCommand(QueueMode(step.command), step.command.c_str());
        if (step.subtarget)
        { this->waiting_subtarget_ = true; this->saw_subtarget_ = false; this->due_ = now + 1000; this->subtarget_deadline_ = now + 120000; }
        else this->due_ = now + step.delay;
        if (this->next_ >= this->pending_.size() && !this->waiting_subtarget_) this->Cancel();
    }
}
