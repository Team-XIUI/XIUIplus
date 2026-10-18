#include "action_store.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <utility>

namespace
{
    auto Trim(std::string value) -> std::string
    {
        const auto first = value.find_first_not_of(" \r\n\t");
        if (first == std::string::npos) return {};
        return value.substr(first, value.find_last_not_of(" \r\n\t") - first + 1);
    }
    auto Hex(const std::string& value) -> std::string
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::string result;
        for (const unsigned char c : value) { result += digits[c >> 4]; result += digits[c & 15]; }
        return result;
    }
    auto Unhex(const std::string& value) -> std::string
    {
        if (value.size() % 2 != 0) return {};
        std::string result;
        const auto digit = [](const unsigned char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
        for (size_t i = 0; i < value.size(); i += 2)
        {
            const auto a = digit(value[i]), b = digit(value[i + 1]);
            if (a < 0 || b < 0 || (a == 0 && b == 0)) return {};
            result += static_cast<char>(a * 16 + b);
        }
        return result;
    }
    auto ReadAction(const xiui::settings_store& settings, const std::string& section) -> xiui::action_slot
    {
        xiui::action_slot slot{};
        slot.kind = static_cast<xiui::action_kind>(std::clamp(settings.ReadInt(section.c_str(), "kind", 0), 0, 10));
        if (slot.Empty()) return slot;
        slot.id = static_cast<uint32_t>(std::max(0, settings.ReadInt(section.c_str(), "id", 0)));
        const auto text = [&](const char* key) { return Unhex(settings.ReadString(section.c_str(), key)); };
        slot.recast_kind = static_cast<xiui::action_kind>(std::clamp(settings.ReadInt(section.c_str(), "recast_kind", 0), 0, 9));
        slot.recast_name = text("recast_name"); slot.recast_id = static_cast<uint32_t>(std::max(0, settings.ReadInt(section.c_str(), "recast_id", 0)));
        slot.always_available = settings.ReadBool(section.c_str(), "always_available", false);
        slot.recast_disabled = settings.ReadBool(section.c_str(), "recast_disabled", false);
        slot.inherit_appearance = settings.ReadBool(section.c_str(), "inherit_appearance", false);
        slot.name = text("name"); slot.target = text("target"); slot.label = text("label"); slot.icon = text("icon"); slot.equipment_slot = text("equipment_slot"); slot.macro_id = text("macro_id");
        const auto count = std::clamp(settings.ReadInt(section.c_str(), "line_count", 0), 0, 32);
        for (int line = 0; line < count; ++line) slot.lines.push_back(text(("line" + std::to_string(line)).c_str()));
        const int book = settings.ReadInt(section.c_str(), "book", 1), set = settings.ReadInt(section.c_str(), "set", 1), index = settings.ReadInt(section.c_str(), "slot", 1);
        if (slot.kind == xiui::action_kind::native_macro && (book < 1 || book > 40 || set < 1 || set > 10 || index < 1 || index > 10)) return {};
        slot.native.book = static_cast<uint8_t>(book); slot.native.set = static_cast<uint8_t>(set); slot.native.slot = static_cast<uint8_t>(index);
        slot.native.alt = settings.ReadBool(section.c_str(), "alt", false);
        return xiui::ValidateAction(slot).empty() ? slot : xiui::action_slot{};
    }
    auto WriteAction(const xiui::settings_store& settings, const std::string& section, const xiui::action_slot& slot) -> void
    {
        settings.WriteInt(section.c_str(), "kind", static_cast<int>(slot.kind));
        if (slot.Empty()) return;
        settings.WriteInt(section.c_str(), "id", static_cast<int>(slot.id));
        const auto text = [&](const char* key, const std::string& value) { settings.WriteString(section.c_str(), key, Hex(value)); };
        settings.WriteInt(section.c_str(), "recast_kind", static_cast<int>(slot.recast_kind));
        text("recast_name", slot.recast_name); settings.WriteInt(section.c_str(), "recast_id", static_cast<int>(slot.recast_id));
        settings.WriteBool(section.c_str(), "always_available", slot.always_available);
        settings.WriteBool(section.c_str(), "recast_disabled", slot.recast_disabled);
        settings.WriteBool(section.c_str(), "inherit_appearance", slot.inherit_appearance);
        text("name", slot.name); text("target", slot.target); text("label", slot.label); text("icon", slot.icon); text("equipment_slot", slot.equipment_slot); text("macro_id", slot.macro_id);
        settings.WriteInt(section.c_str(), "line_count", static_cast<int>(slot.lines.size()));
        for (size_t line = 0; line < slot.lines.size(); ++line) text(("line" + std::to_string(line)).c_str(), slot.lines[line]);
        settings.WriteInt(section.c_str(), "book", slot.native.book); settings.WriteInt(section.c_str(), "set", slot.native.set);
        settings.WriteInt(section.c_str(), "slot", slot.native.slot); settings.WriteBool(section.c_str(), "alt", slot.native.alt);
    }
    auto ReadLayout(const xiui::settings_store& settings, const std::string& section, xiui::action_layout& layout) -> void
    {
        for (size_t bar = 0; bar < layout.hotbar.size(); ++bar)
            for (size_t slot = 0; slot < layout.hotbar[bar].size(); ++slot)
                layout.hotbar[bar][slot] = ReadAction(settings, section + ".h" + std::to_string(bar) + "." + std::to_string(slot));
        for (size_t bank = 0; bank < layout.crossbar.size(); ++bank)
            for (size_t slot = 0; slot < layout.crossbar[bank].size(); ++slot)
                layout.crossbar[bank][slot] = ReadAction(settings, section + ".c" + std::to_string(bank) + "." + std::to_string(slot));
    }
    auto WriteLayout(const xiui::settings_store& settings, const std::string& section, const xiui::action_layout& layout) -> void
    {
        for (size_t bar = 0; bar < layout.hotbar.size(); ++bar)
            for (size_t slot = 0; slot < layout.hotbar[bar].size(); ++slot)
                WriteAction(settings, section + ".h" + std::to_string(bar) + "." + std::to_string(slot), layout.hotbar[bar][slot]);
        for (size_t bank = 0; bank < layout.crossbar.size(); ++bank)
            for (size_t slot = 0; slot < layout.crossbar[bank].size(); ++slot)
                WriteAction(settings, section + ".c" + std::to_string(bank) + "." + std::to_string(slot), layout.crossbar[bank][slot]);
    }
    auto Slot(xiui::action_layout& layout, xiui::action_surface surface, size_t bar, size_t slot) -> xiui::action_slot*
    {
        if (surface == xiui::action_surface::hotbar) return bar < layout.hotbar.size() && slot < layout.hotbar[bar].size() ? &layout.hotbar[bar][slot] : nullptr;
        return bar < layout.crossbar.size() && slot < layout.crossbar[bar].size() ? &layout.crossbar[bar][slot] : nullptr;
    }
}

namespace xiui
{
    auto ActionKindName(const action_kind kind) -> const char*
    {
        constexpr const char* names[]{"Empty", "Spell", "Ability", "Weapon skill", "Item", "Equip", "Pet", "Macro", "Native macro", "Command"};
        return static_cast<size_t>(kind) < std::size(names) ? names[static_cast<size_t>(kind)] : "Unknown";
    }
    auto ActionKindFromName(const std::string& name) -> action_kind
    {
        std::string key = name;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (key == "spell" || key == "ma" || key == "magic" || key == "trust") return action_kind::spell;
        if (key == "ability" || key == "ja" || key == "jobability") return action_kind::ability;
        if (key == "ws" || key == "weaponskill" || key == "weapon skill") return action_kind::weapon_skill;
        if (key == "item") return action_kind::item;
        if (key == "equip" || key == "equipment") return action_kind::equip;
        if (key == "pet" || key == "petability" || key == "petcommand" || key == "rage" || key == "ward" || key == "bloodpactrage" || key == "bloodpactward") return action_kind::pet;
        if (key == "macro") return action_kind::macro;
        if (key == "native_macro" || key == "native macro") return action_kind::native_macro;
        if (key == "command") return action_kind::command;
        return action_kind::none;
    }
    auto ValidateAction(const action_slot& slot) -> std::string
    {
        if (static_cast<unsigned>(slot.kind) > static_cast<unsigned>(action_kind::command)) return "Unknown action type.";
        if (slot.Empty()) return {};
        const auto safe = [](const std::string& value, size_t max) { return value.size() <= max && value.find_first_of("\r\n\0", 0, 3) == std::string::npos; };
        if (!safe(slot.recast_name, 240) || (slot.recast_kind != action_kind::none && slot.recast_kind != action_kind::spell && slot.recast_kind != action_kind::ability && slot.recast_kind != action_kind::pet && slot.recast_kind != action_kind::item)) return "Choose a spell, ability, pet action or item cooldown source.";
        if (slot.recast_kind != action_kind::none && slot.recast_id == 0 && slot.recast_name.empty()) return "Enter a cooldown source action.";
        if (!safe(slot.name, 240) || !safe(slot.label, 120) || !safe(slot.icon, 240) || !safe(slot.equipment_slot, 24) || !safe(slot.macro_id, 120)) return "Action text is too long or contains a line break.";
        constexpr const char* targets[]{"", "<t>", "<me>", "<st>", "<stpc>", "<stnpc>", "<stpt>", "<stal>", "<bt>", "<pet>", "<lastst>", "<p0>", "<p1>", "<p2>", "<p3>", "<p4>", "<p5>"};
        if (std::find(std::begin(targets), std::end(targets), slot.target) == std::end(targets)) return "Choose a supported target placeholder.";
        if (slot.lines.size() > 32) return "Macros support up to 32 lines.";
        for (const auto& line : slot.lines) if (!safe(line, 240)) return "Each macro line must fit within 240 characters.";
        if (slot.kind == action_kind::macro || slot.kind == action_kind::command)
        {
            if (slot.lines.empty() && (slot.kind != action_kind::macro || slot.macro_id.empty())) return "Enter at least one command line.";
            for (const auto& line : slot.lines) if (!line.empty() && line.front() != '/') return "Commands must start with /.";
        }
        else if (slot.kind == action_kind::native_macro)
        { if (slot.native.book < 1 || slot.native.book > 40 || slot.native.set < 1 || slot.native.set > 10 || slot.native.slot < 1 || slot.native.slot > 10) return "Native macro book, set or slot is out of range."; }
        else if (slot.id == 0 && slot.name.empty()) return "Choose an action.";
        if (slot.kind == action_kind::equip && slot.equipment_slot.empty()) return "Choose an equipment slot.";
        return {};
    }

    action_store::action_store()
    {
        this->hotbar_scope_.fill(action_scope::job_subjob); this->crossbar_scope_.fill(action_scope::job_subjob);
        this->Ensure(action_surface::hotbar, 0); this->changed_ = false;
    }
    auto action_store::Touch(void) -> void { this->changed_ = true; ++this->revision_; }
    auto action_store::Changed(void) -> bool { return std::exchange(this->changed_, false); }
    auto action_store::Scope(action_surface surface, size_t bar) const -> action_scope
    {
        if (surface == action_surface::hotbar) return bar < this->hotbar_scope_.size() ? this->hotbar_scope_[bar] : action_scope::job_subjob;
        return bar < this->crossbar_scope_.size() ? this->crossbar_scope_[bar] : action_scope::job_subjob;
    }
    auto action_store::Key(action_surface surface, size_t bar) const -> std::pair<uint8_t, uint8_t>
    {
        const auto scope = this->Scope(surface, bar);
        return {static_cast<uint8_t>(scope == action_scope::global ? 0 : this->context_.job), static_cast<uint8_t>(scope == action_scope::job_subjob ? this->context_.subjob : 0)};
    }
    auto action_store::PetAware(action_surface surface, size_t bar) const -> bool
    { return surface == action_surface::hotbar ? bar < this->hotbar_pet_.size() && this->hotbar_pet_[bar] : bar < this->crossbar_pet_.size() && this->crossbar_pet_[bar]; }
    auto action_store::SetPetAware(action_surface surface, size_t bar, bool enabled) -> void
    {
        if (surface == action_surface::hotbar && bar < this->hotbar_pet_.size()) this->hotbar_pet_[bar] = enabled;
        else if (surface == action_surface::crossbar && bar < this->crossbar_pet_.size()) this->crossbar_pet_[bar] = enabled;
        else return;
        this->Touch();
    }
    auto action_store::Palettes(action_surface surface, size_t bar) const -> std::vector<const action_palette*>
    {
        auto [job, subjob] = this->Key(surface, bar);
        std::vector<const action_palette*> result;
        const auto collect = [&]() { for (const auto& palette : this->palettes_) if (palette.job == job && palette.subjob == subjob && (palette.surfaces & (1 << static_cast<int>(surface))) != 0) result.push_back(&palette); };
        collect();
        if (result.empty() && subjob != 0) { subjob = 0; collect(); }
        if (result.empty() && job != 0) { job = 0; collect(); }
        const auto& order = this->orders_[static_cast<size_t>(surface)];
        std::stable_sort(result.begin(), result.end(), [&](const auto* a, const auto* b) { return std::find(order.begin(), order.end(), a->id) < std::find(order.begin(), order.end(), b->id); });
        return result;
    }
    auto action_store::ActivePalette(action_surface surface, size_t bar) const -> const action_palette*
    {
        const auto available = this->Palettes(surface, bar);
        if (available.empty()) return nullptr;
        for (const auto& selected : this->selections_)
            if (selected.surface == surface && selected.job == available.front()->job && selected.subjob == available.front()->subjob)
                for (const auto* palette : available) if (palette->id == selected.id) return palette;
        return available.front();
    }
    auto action_store::Ensure(action_surface surface, size_t bar) -> void
    {
        const auto available = this->Palettes(surface, bar);
        const auto [job, subjob] = this->Key(surface, bar);
        if (available.empty() || (job != 0 && available.front()->job == 0)) this->Create("Default", surface, bar);
    }
    auto action_store::SetContext(action_context context) -> void
    {
        context.job = std::min<uint8_t>(22, context.job); context.subjob = std::min<uint8_t>(22, context.subjob);
        if (context.pet.size() > 120) context.pet.resize(120);
        this->context_ = std::move(context);
        this->Ensure(action_surface::hotbar, 0); this->Ensure(action_surface::crossbar, 0);
    }
    auto action_store::SetScope(action_surface surface, size_t bar, action_scope scope) -> void
    {
        if (static_cast<unsigned>(scope) > 2 || (surface == action_surface::hotbar ? bar >= action_bar_count : bar >= action_crossbar_banks)) return;
        if (scope == this->Scope(surface, bar)) return;
        action_layout previous{};
        if (const auto* palette = this->ActivePalette(surface, bar)) previous = palette->actions;
        if (surface == action_surface::hotbar) this->hotbar_scope_[bar] = scope; else this->crossbar_scope_[bar] = scope;
        const auto [job, subjob] = this->Key(surface, bar);
        const bool existing = std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.job == job && p.subjob == subjob && (p.surfaces & (1 << static_cast<int>(surface))) != 0; });
        if (!existing)
        {
            const auto id = this->Create("Default", surface, bar);
            for (auto& p : this->palettes_) if (p.id == id) p.actions = std::move(previous);
            this->Select(surface, id, bar);
        }
        this->Touch();
    }
    auto action_store::Create(std::string name, action_surface surface, size_t bar) -> std::string
    {
        name = Trim(std::move(name));
        if (name.empty() || name.size() > 120 || name.find_first_of("\r\n") != std::string::npos || this->palettes_.size() >= 512) return {};
        const auto [job, subjob] = this->Key(surface, bar);
        std::string id;
        do { id = "palette-" + std::to_string(this->sequence_++); } while (std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.id == id; }));
        action_palette palette{}; palette.id = id; palette.name = std::move(name); palette.job = job; palette.subjob = subjob; palette.surfaces = static_cast<uint8_t>(1 << static_cast<int>(surface));
        this->palettes_.push_back(std::move(palette)); this->Touch(); return id;
    }
    auto action_store::Select(action_surface surface, const std::string& id, size_t bar) -> bool
    {
        const auto available = this->Palettes(surface, bar);
        const auto found = std::find_if(available.begin(), available.end(), [&](const auto* p) { return p->id == id; });
        if (found == available.end()) return false;
        for (auto& selection : this->selections_)
            if (selection.surface == surface && selection.job == (*found)->job && selection.subjob == (*found)->subjob)
            { if (selection.id != id) { selection.id = id; this->Touch(); } return true; }
        this->selections_.push_back({(*found)->job, (*found)->subjob, surface, id}); this->Touch(); return true;
    }
    auto action_store::Rename(const std::string& id, std::string name) -> bool
    {
        name = Trim(std::move(name));
        if (name.empty() || name.size() > 120 || name.find_first_of("\r\n") != std::string::npos) return false;
        for (auto& palette : this->palettes_) if (palette.id == id) { palette.name = std::move(name); this->Touch(); return true; }
        return false;
    }
    auto action_store::Copy(const std::string& id, std::string name) -> std::string
    {
        for (const auto& palette : this->palettes_) if (palette.id == id)
        {
            auto copy = palette;
            if (Trim(name).empty() || name.size() > 120 || this->palettes_.size() >= 512) return {};
            do { copy.id = "palette-" + std::to_string(this->sequence_++); } while (std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& value) { return value.id == copy.id; }));
            copy.name = Trim(std::move(name));
            const auto result = copy.id; this->palettes_.push_back(std::move(copy)); this->Touch(); return result;
        }
        return {};
    }
    auto action_store::Move(const std::string& id, int delta) -> bool
    {
        if (delta == 0) return false;
        const auto found = std::find_if(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.id == id; });
        if (found == this->palettes_.end()) return false;
        const auto surface = found->surfaces == 2 ? action_surface::crossbar : action_surface::hotbar;
        auto ids = this->orders_[static_cast<size_t>(surface)];
        for (const auto& p : this->palettes_) if (std::find(ids.begin(), ids.end(), p.id) == ids.end()) ids.push_back(p.id);
        auto at = std::find(ids.begin(), ids.end(), id);
        const auto eligible = [&](const std::string& key) {
            return std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.id == key && p.job == found->job && p.subjob == found->subjob && (p.surfaces & found->surfaces) != 0; });
        };
        for (auto next = at - ids.begin() + (delta > 0 ? 1 : -1); next >= 0 && next < static_cast<ptrdiff_t>(ids.size()); next += delta > 0 ? 1 : -1)
            if (eligible(ids[next])) { std::iter_swap(at, ids.begin() + next); this->SetPaletteOrder(surface, std::move(ids)); return true; }
        return false;
    }
    auto action_store::SetPaletteOrder(const action_surface surface, std::vector<std::string> ids) -> void
    { this->orders_[static_cast<size_t>(surface)] = std::move(ids); this->Touch(); }
    auto action_store::Delete(const std::string& id) -> bool
    {
        for (auto it = this->palettes_.begin(); it != this->palettes_.end(); ++it) if (it->id == id)
        {
            if (std::count_if(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.job == it->job && p.subjob == it->subjob && (p.surfaces & it->surfaces) != 0; }) <= 1) return false;
            this->palettes_.erase(it);
            std::erase_if(this->selections_, [&](const auto& s) { return s.id == id; });
            this->Touch(); return true;
        }
        return false;
    }
    auto action_store::Cycle(action_surface surface, int delta, size_t bar) -> bool
    {
        const auto available = this->Palettes(surface, bar); const auto* active = this->ActivePalette(surface, bar);
        if (available.size() < 2 || active == nullptr || delta == 0) return false;
        auto at = std::find(available.begin(), available.end(), active) - available.begin();
        const auto size = static_cast<ptrdiff_t>(available.size()); at = ((at + delta) % size + size) % size;
        return this->Select(surface, available[at]->id, bar);
    }
    auto action_store::Resolve(action_surface surface, size_t bar, size_t slot) const -> const action_slot&
    {
        static const action_slot empty{};
        const auto* palette = this->ActivePalette(surface, bar); if (palette == nullptr) return empty;
        const auto* layout = &palette->actions;
        if (this->PetAware(surface, bar) && !this->context_.pet.empty())
            for (const auto& pet : palette->pets) if (pet.pet == this->context_.pet) { layout = &pet.actions; break; }
        const auto* result = Slot(const_cast<action_layout&>(*layout), surface, bar, slot);
        return result != nullptr ? *result : empty;
    }
    auto action_store::Assign(action_surface surface, size_t bar, size_t slot, action_slot action, bool petOverride) -> bool
    {
        if (!ValidateAction(action).empty()) return false;
        const auto* selected = this->ActivePalette(surface, bar); if (selected == nullptr) return false;
        auto& palette = const_cast<action_palette&>(*selected);
        auto* layout = &palette.actions;
        if (petOverride)
        {
            if (this->context_.pet.empty()) return false;
            auto pet = std::find_if(palette.pets.begin(), palette.pets.end(), [&](const auto& p) { return p.pet == this->context_.pet; });
            if (pet == palette.pets.end())
            {
                if (palette.pets.size() >= 32) return false;
                palette.pets.push_back({this->context_.pet, palette.actions}); pet = std::prev(palette.pets.end());
            }
            layout = &pet->actions;
        }
        auto* destination = Slot(*layout, surface, bar, slot); if (destination == nullptr) return false;
        if (*destination != action) { *destination = std::move(action); this->Touch(); }
        return true;
    }
    auto action_store::Swap(action_surface source, size_t sourceBar, size_t sourceSlot, action_surface destination, size_t destinationBar, size_t destinationSlot) -> bool
    {
        if ((source == action_surface::hotbar ? sourceBar >= 10 || sourceSlot >= 12 : sourceBar >= 6 || sourceSlot >= 8) ||
            (destination == action_surface::hotbar ? destinationBar >= 10 || destinationSlot >= 12 : destinationBar >= 6 || destinationSlot >= 8)) return false;
        const auto left = this->Resolve(source, sourceBar, sourceSlot), right = this->Resolve(destination, destinationBar, destinationSlot);
        const bool sourcePet = this->PetAware(source, sourceBar) && !this->context_.pet.empty();
        const bool destinationPet = this->PetAware(destination, destinationBar) && !this->context_.pet.empty();
        return this->Assign(source, sourceBar, sourceSlot, right, sourcePet) && this->Assign(destination, destinationBar, destinationSlot, left, destinationPet);
    }
    auto action_store::ImportPalette(action_palette palette, action_surface surface, size_t bar, bool sourceContext) -> std::string
    {
        if (palette.surfaces == 3)
        {
            if (this->palettes_.size() > 510) return {};
            auto hotbar = palette; hotbar.surfaces = 1; palette.surfaces = 2;
            const auto hotbarId = this->ImportPalette(std::move(hotbar), action_surface::hotbar, std::min(bar, action_bar_count - 1), sourceContext);
            const auto crossbarId = this->ImportPalette(std::move(palette), action_surface::crossbar, std::min(bar, action_crossbar_banks - 1), sourceContext);
            return surface == action_surface::hotbar ? hotbarId : crossbarId;
        }
        surface = palette.surfaces == 2 ? action_surface::crossbar : action_surface::hotbar;
        const auto id = this->Create(palette.name.empty() ? "Imported" : palette.name, surface, bar);
        for (auto& destination : this->palettes_) if (destination.id == id)
        {
            destination.actions = std::move(palette.actions); destination.pets = std::move(palette.pets);
            if (sourceContext) { destination.job = std::min<uint8_t>(22, palette.job); destination.subjob = destination.job == 0 ? 0 : std::min<uint8_t>(22, palette.subjob); }
            break;
        }
        return id;
    }
    auto action_store::Macro(const std::string& id) const -> const action_macro*
    { for (const auto& macro : this->macros_) if (macro.id == id) return &macro; return nullptr; }
    auto action_store::CreateMacro(std::string name, std::vector<std::string> lines, std::string icon) -> std::string
    {
        action_macro macro{};
        do { macro.id = "macro-" + std::to_string(this->sequence_++); } while (this->Macro(macro.id) != nullptr);
        macro.name = Trim(std::move(name)); macro.lines = std::move(lines); macro.icon = std::move(icon);
        if (this->macros_.size() >= 1024 || !this->SaveMacro(macro)) return {};
        return macro.id;
    }
    auto action_store::SaveMacro(const action_macro& macro) -> bool
    {
        if (macro.id.empty() || macro.id.size() > 120 || macro.name.empty() || macro.name.size() > 120 || macro.id.find_first_of("\r\n") != std::string::npos || macro.name.find_first_of("\r\n") != std::string::npos) return false;
        action_slot slot = macro.definition;
        if (slot.Empty()) { slot.kind = action_kind::macro; slot.name = macro.name; slot.icon = macro.icon; slot.lines = macro.lines; }
        slot.macro_id.clear();
        if (slot.kind == action_kind::macro || slot.kind == action_kind::command) { slot.lines = macro.lines; slot.name = macro.name; }
        slot.icon = macro.icon;
        if (!ValidateAction(slot).empty()) return false;
        auto normalized = macro; normalized.definition = slot; normalized.lines = slot.lines;
        for (auto& existing : this->macros_) if (existing.id == macro.id) { existing = normalized; this->Touch(); return true; }
        if (this->macros_.size() >= 1024) return false;
        this->macros_.push_back(std::move(normalized)); this->Touch(); return true;
    }
    auto action_store::ImportMacro(action_macro macro) -> std::string
    {
        do { macro.id = "macro-" + std::to_string(this->sequence_++); } while (this->Macro(macro.id) != nullptr);
        return this->SaveMacro(macro) ? macro.id : std::string{};
    }
    auto action_store::DeleteMacro(const std::string& id) -> bool
    {
        const auto* macro = this->Macro(id); if (macro == nullptr) return false;
        // Keep assigned actions usable when removing their library entry.
        const auto detach = [&](action_layout& layout) {
            const auto slot = [&](action_slot& action) { if (action.macro_id == id) { action = this->ExpandMacro(action); action.macro_id.clear(); } };
            for (auto& bar : layout.hotbar) for (auto& action : bar) slot(action);
            for (auto& bank : layout.crossbar) for (auto& action : bank) slot(action);
        };
        for (auto& palette : this->palettes_) { detach(palette.actions); for (auto& pet : palette.pets) detach(pet.actions); }
        std::erase_if(this->macros_, [&](const auto& value) { return value.id == id; }); this->Touch(); return true;
    }
    auto action_store::ExpandMacro(const action_slot& slot) const -> action_slot
    {
        auto expanded = slot;
        if (!slot.macro_id.empty())
            if (const auto* macro = this->Macro(slot.macro_id))
            { if (!macro->definition.Empty()) expanded = macro->definition; else expanded.lines = macro->lines; if (!slot.inherit_appearance) { if (!slot.label.empty()) expanded.label = slot.label; if (!slot.icon.empty()) expanded.icon = slot.icon; } if (expanded.name.empty()) expanded.name = macro->name; if (expanded.icon.empty()) expanded.icon = macro->icon; expanded.macro_id.clear(); }
        return expanded;
    }

    auto action_store::Save(const settings_store& settings) const -> void
    {
        constexpr const char* section = "actionbars.data";
        settings.WriteInt(section, "version", 1); settings.WriteString(section, "sequence", std::to_string(this->sequence_));
        settings.WriteInt(section, "palette_count", static_cast<int>(this->palettes_.size()));
        for (size_t bar = 0; bar < 10; ++bar)
        { settings.WriteInt(section, ("scope.h" + std::to_string(bar)).c_str(), static_cast<int>(this->hotbar_scope_[bar])); settings.WriteBool(section, ("pet.h" + std::to_string(bar)).c_str(), this->hotbar_pet_[bar]); }
        for (size_t bank = 0; bank < action_crossbar_banks; ++bank)
        { settings.WriteInt(section, ("scope.c" + std::to_string(bank)).c_str(), static_cast<int>(this->crossbar_scope_[bank])); settings.WriteBool(section, ("pet.c" + std::to_string(bank)).c_str(), this->crossbar_pet_[bank]); }
        settings.WriteInt(section, "selection_count", static_cast<int>(this->selections_.size()));
        settings.WriteInt(section, "macro_count", static_cast<int>(this->macros_.size()));
        for (size_t surface = 0; surface < 2; ++surface)
        {
            std::string order; for (const auto& id : this->orders_[surface]) { if (!order.empty()) order += ','; order += id; }
            settings.WriteString(section, surface == 0 ? "hotbar_order" : "crossbar_order", order);
        }
        for (size_t i = 0; i < this->macros_.size(); ++i)
        {
            const auto prefix = "actionbars.macro." + std::to_string(i); const auto& macro = this->macros_[i];
            action_slot action = macro.definition;
            if (action.Empty()) { action.kind = action_kind::macro; action.name = macro.name; action.icon = macro.icon; action.lines = macro.lines; }
            settings.WriteString(prefix.c_str(), "stable_id", macro.id); WriteAction(settings, prefix, action);
        }
        for (size_t i = 0; i < this->selections_.size(); ++i)
        {
            const auto& s = this->selections_[i]; const auto key = "selection" + std::to_string(i);
            settings.WriteString(section, key.c_str(), std::to_string(s.job) + ":" + std::to_string(s.subjob) + ":" + std::to_string(static_cast<int>(s.surface)) + ":" + s.id);
        }
        for (size_t i = 0; i < this->palettes_.size(); ++i)
        {
            const auto& p = this->palettes_[i]; const auto prefix = "actionbars.palette." + std::to_string(i);
            settings.WriteInt(prefix.c_str(), "surfaces", p.surfaces);
            settings.WriteString(prefix.c_str(), "id", p.id); settings.WriteString(prefix.c_str(), "name", Hex(p.name));
            settings.WriteInt(prefix.c_str(), "job", p.job); settings.WriteInt(prefix.c_str(), "subjob", p.subjob);
            settings.WriteInt(prefix.c_str(), "pet_count", static_cast<int>(p.pets.size())); WriteLayout(settings, prefix, p.actions);
            for (size_t pet = 0; pet < p.pets.size(); ++pet)
            { const auto petPrefix = prefix + ".pet" + std::to_string(pet); settings.WriteString(petPrefix.c_str(), "name", Hex(p.pets[pet].pet)); WriteLayout(settings, petPrefix, p.pets[pet].actions); }
        }
    }
    auto action_store::Load(const settings_store& settings) -> void
    {
        const auto context = this->context_; *this = action_store{}; this->context_ = context;
        constexpr const char* section = "actionbars.data";
        if (settings.ReadInt(section, "version", 0) != 1) { this->SetContext(context); this->changed_ = false; return; }
        this->palettes_.clear(); this->selections_.clear(); this->macros_.clear();
        const auto sequence = settings.ReadString(section, "sequence", "1");
        std::from_chars(sequence.data(), sequence.data() + sequence.size(), this->sequence_); this->sequence_ = std::max<uint64_t>(1, this->sequence_);
        const auto macroCount = std::clamp(settings.ReadInt(section, "macro_count", 0), 0, 1024);
        for (int i = 0; i < macroCount; ++i)
        {
            const auto prefix = "actionbars.macro." + std::to_string(i); const auto action = ReadAction(settings, prefix); const auto id = settings.ReadString(prefix.c_str(), "stable_id");
            if (!action.Empty()) this->SaveMacro({id, action.label.empty() ? action.name : action.label, action.icon, action.lines, action});
        }
        for (size_t bar = 0; bar < 10; ++bar)
        { this->hotbar_scope_[bar] = static_cast<action_scope>(std::clamp(settings.ReadInt(section, ("scope.h" + std::to_string(bar)).c_str(), 2), 0, 2)); this->hotbar_pet_[bar] = settings.ReadBool(section, ("pet.h" + std::to_string(bar)).c_str(), false); }
        for (size_t bank = 0; bank < action_crossbar_banks; ++bank)
        { this->crossbar_scope_[bank] = static_cast<action_scope>(std::clamp(settings.ReadInt(section, ("scope.c" + std::to_string(bank)).c_str(), 2), 0, 2)); this->crossbar_pet_[bank] = settings.ReadBool(section, ("pet.c" + std::to_string(bank)).c_str(), false); }
        const auto count = std::clamp(settings.ReadInt(section, "palette_count", 0), 0, 512);
        for (int i = 0; i < count; ++i)
        {
            const auto prefix = "actionbars.palette." + std::to_string(i); action_palette p{};
            p.surfaces = static_cast<uint8_t>(std::clamp(settings.ReadInt(prefix.c_str(), "surfaces", 3), 1, 3));
            p.id = settings.ReadString(prefix.c_str(), "id"); p.name = Unhex(settings.ReadString(prefix.c_str(), "name"));
            if (p.id.empty() || p.id.size() > 120 || p.name.empty() || p.name.size() > 120 || std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& other) { return other.id == p.id; })) continue;
            p.job = static_cast<uint8_t>(std::clamp(settings.ReadInt(prefix.c_str(), "job", 0), 0, 22)); p.subjob = static_cast<uint8_t>(std::clamp(settings.ReadInt(prefix.c_str(), "subjob", 0), 0, 22));
            ReadLayout(settings, prefix, p.actions);
            const auto pets = std::clamp(settings.ReadInt(prefix.c_str(), "pet_count", 0), 0, 32);
            for (int pet = 0; pet < pets; ++pet)
            { const auto petPrefix = prefix + ".pet" + std::to_string(pet); action_pet_layout data{}; data.pet = Unhex(settings.ReadString(petPrefix.c_str(), "name")); if (data.pet.empty() || data.pet.size() > 120) continue; ReadLayout(settings, petPrefix, data.actions); p.pets.push_back(std::move(data)); }
            this->palettes_.push_back(std::move(p));
        }
        const auto selections = std::clamp(settings.ReadInt(section, "selection_count", 0), 0, 1024);
        for (int i = 0; i < selections; ++i)
        {
            const auto value = settings.ReadString(section, ("selection" + std::to_string(i)).c_str());
            const auto a = value.find(':'), b = a == std::string::npos ? a : value.find(':', a + 1), c = b == std::string::npos ? b : value.find(':', b + 1);
            if (c == std::string::npos) continue;
            unsigned job{}, subjob{}, surface{};
            if (std::from_chars(value.data(), value.data() + a, job).ec != std::errc{} || std::from_chars(value.data() + a + 1, value.data() + b, subjob).ec != std::errc{} || std::from_chars(value.data() + b + 1, value.data() + c, surface).ec != std::errc{} || job > 22 || subjob > 22 || surface > 1) continue;
            this->selections_.push_back({static_cast<uint8_t>(job), static_cast<uint8_t>(subjob), static_cast<action_surface>(surface), value.substr(c + 1)});
        }
        for (size_t surface = 0; surface < 2; ++surface)
        {
            auto& order = this->orders_[surface]; order.clear();
            const auto text = settings.ReadString(section, surface == 0 ? "hotbar_order" : "crossbar_order");
            for (size_t start = 0; start < text.size() && order.size() < 512;)
            { const auto end = text.find(',', start); order.push_back(text.substr(start, end == text.npos ? text.size() - start : end - start)); if (end == text.npos) break; start = end + 1; }
        }
        const auto oldCount = this->palettes_.size();
        for (size_t i = 0; i < oldCount; ++i)
        {
            if (this->palettes_[i].surfaces != 3 || this->palettes_.size() >= 512) continue;
            auto copy = this->palettes_[i]; const auto oldId = copy.id;
            do { copy.id = "palette-" + std::to_string(this->sequence_++); } while (std::any_of(this->palettes_.begin(), this->palettes_.end(), [&](const auto& p) { return p.id == copy.id; }));
            this->palettes_[i].surfaces = 1; copy.surfaces = 2;
            for (auto& selection : this->selections_) if (selection.surface == action_surface::crossbar && selection.id == oldId) selection.id = copy.id;
            auto& order = this->orders_[1]; for (auto& id : order) if (id == oldId) id = copy.id;
            this->palettes_.push_back(std::move(copy));
        }
        this->SetContext(context); this->changed_ = false; ++this->revision_;
    }
}
