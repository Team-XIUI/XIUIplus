#include "native_macros.hpp"

#include "Ashita.h"
#include "Memory.h"

#include <cstring>

namespace
{
    auto ReadPointer(const uintptr_t address, uintptr_t& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return value != 0; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto RunMacro(const uintptr_t canUse, const uintptr_t run, const uintptr_t book, const uintptr_t page,
        const uintptr_t container, const uintptr_t controller, const xiui::native_macro_ref& ref) noexcept -> bool
    {
        using can_fn = bool(__thiscall*)(void*);
        using select_fn = int32_t(__thiscall*)(void*, uint32_t);
        using run_fn = void(__thiscall*)(void*, uint8_t, uint8_t);
        __try
        {
            auto* const macros = reinterpret_cast<void*>(container);
            auto* const controls = reinterpret_cast<void*>(controller);
            if (!reinterpret_cast<can_fn>(canUse)(controls)) return false;
            reinterpret_cast<select_fn>(book)(macros, ref.book - 1);
            reinterpret_cast<select_fn>(page)(macros, (ref.set - 1) * 2);
            if (!reinterpret_cast<can_fn>(canUse)(controls)) return false;
            reinterpret_cast<run_fn>(run)(controls, ref.alt ? 2 : 1, ref.slot - 1);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto native_macros::Initialize(void) -> void
    {
        const auto runner = this->runner_; auto* context = this->runner_context_;
        *this = {}; this->SetRunner(runner, context);
        if (runner != nullptr) return;
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size == 0) return;
        const auto unique = [base, size](const char* pattern) -> uintptr_t {
            const auto found = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            return found != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0 ? found : 0;
        };
        const auto macro = unique("8B0D????????E8????????8B4424105EC38B15????????68");
        // The shorter Lua signature also matches the adjacent display controller.
        const auto controller = unique("A3????????EB??891D????????6A50E8????????83C4043BC374??8B15????????8B0D????????52518BC8E8????????A3");
        const auto can = unique("66A1????????5633F66685C074??0FBFC08B3485????????8B0D");
        const auto run = unique("8A44240453563C018BF175??8A5C24108B4E040FBEC350E8");
        const auto book = unique("8B442404568BF18B8EC01D00003BC875??8BC15EC20400");
        const auto page = unique("568BF1578B7C240C8B86C01D00008D04");
        uintptr_t macroSlot{}, controllerSlot{}, linkedMacro{}, linkedController{};
        if (macro == 0 || controller == 0 || can == 0 || run == 0 || book == 0 || page == 0 ||
            !ReadPointer(macro + 2, macroSlot) || !ReadPointer(controller + 1, controllerSlot) ||
            !ReadPointer(controller + 29, linkedController) || !ReadPointer(controller + 35, linkedMacro) ||
            linkedController != controllerSlot || linkedMacro != macroSlot ||
            macroSlot < base || macroSlot - base > size - sizeof(uintptr_t) ||
            controllerSlot < base || controllerSlot - base > size - sizeof(uintptr_t)) return;
        this->container_slot_ = macroSlot; this->controller_slot_ = controllerSlot;
        this->can_use_ = can; this->run_ = run; this->set_book_ = book; this->set_page_ = page;
    }

    auto native_macros::Run(const native_macro_ref& macro) const -> bool
    {
        if (!this->Available() || macro.book < 1 || macro.book > 40 || macro.set < 1 || macro.set > 10 || macro.slot < 1 || macro.slot > 10) return false;
        if (this->runner_ != nullptr) return this->runner_(this->runner_context_, macro.book, macro.set, macro.slot, macro.alt ? TRUE : FALSE) != FALSE;
        uintptr_t container{}, controller{};
        return ReadPointer(this->container_slot_, container) && ReadPointer(this->controller_slot_, controller) &&
            RunMacro(this->can_use_, this->run_, this->set_book_, this->set_page_, container, controller, macro);
    }
}
