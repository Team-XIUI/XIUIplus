#include "ashita_compat.hpp"
#include "mouse_input.hpp"

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    { if (!condition) throw std::runtime_error{message}; }

    struct native_fixture
    {
        void** vtable{};
        mutable uint32_t value{1000};
        std::array<void*, 280> slots{};
        auto Number(void) const -> uint32_t { return this->value; }
        auto Indexed(uint32_t index) const -> uint32_t { return this->value + index; }
        auto Index(uint32_t index) const -> uint16_t { return static_cast<uint16_t>(this->value + index); }
        auto Byte(uint32_t index) const -> uint8_t { return static_cast<uint8_t>(index + 1); }
        auto Flag(void) const -> uint8_t { return 1; }
        auto Scalar(uint32_t index) const -> float { return static_cast<float>(this->value + index) + 0.25f; }
        auto Skill(uint32_t index) const -> Ashita::FFXI::combatskill_t { return {static_cast<uint16_t>(this->value + index)}; }
        auto Target(uint32_t index, bool force) const -> void { this->value = index + (force ? 100 : 0); }
        auto Type(uint32_t index, uint8_t type) const -> void { this->value = index + type; }
        template<typename Method> auto Bind(size_t slot, Method method) -> void
        {
            static_assert(sizeof(method) == sizeof(void*));
            this->slots.fill(nullptr);
            std::memcpy(&this->slots[slot], &method, sizeof(method));
            this->vtable = this->slots.data();
        }
    };

    auto CheckInterfaces(void) -> void
    {
        for (const bool legacy : {true, false})
        {
            native_fixture native;
            const xiui::ashita::party_view party{reinterpret_cast<const IParty*>(&native), legacy};
            native.Bind(legacy ? 18 : 19, &native_fixture::Indexed);
            Check(party->GetMemberHP(7) == 1007, "party getter follows the selected SDK layout");
            native.Bind(legacy ? 38 : 41, &native_fixture::Byte);
            Check(party->GetMemberIsActive(17) == 18, "party getters after the master-level additions retain their slot");
            native.Bind(36, &native_fixture::Byte);
            Check(party->GetMemberMasterLevel(9) == (legacy ? 0 : 10), "legacy party master level is unavailable rather than another virtual method");

            const xiui::ashita::player_view player{reinterpret_cast<const IPlayer*>(&native), legacy};
            native.Bind(legacy ? 34 : 33, &native_fixture::Number);
            Check(player->GetMasteryExp() == 1000, "mastery data accounts for the removed player method");
            native.Bind(legacy ? 36 : 35, &native_fixture::Skill);
            Check(player->GetCombatSkill(33).GetSkill() == 1033, "small struct returns preserve the x86 member-call ABI");
            native.Bind(legacy ? 53 : 52, &native_fixture::Number);
            Check(player->GetIsZoning() == 1000, "zoning reads the correct player method");
            native.Bind(77, &native_fixture::Number);
            Check(player->GetPetTP() == 1000, "player slots realign after the new submap method");

            const xiui::ashita::entity_view entity{reinterpret_cast<const IEntity*>(&native), legacy};
            native.Bind(legacy ? 114 : 115, &native_fixture::Index);
            Check(entity->GetPetTargetIndex(3) == 1003, "pet targeting accounts for the inserted entity method");
            native.Bind(legacy ? 120 : 121, &native_fixture::Scalar);
            Check(entity->GetModelSize(4) == 1004.25f, "entity float returns retain their ABI");
            native.Bind(legacy ? 174 : 175, &native_fixture::Type);
            entity->SetType(90, 2);
            Check(native.value == 92, "entity writes use the legacy setter slot");

            const xiui::ashita::target_view target{reinterpret_cast<const ITarget*>(&native), legacy};
            native.Bind(legacy ? 26 : 27, &native_fixture::Flag);
            Check(target->GetActionTargetActive() == 1, "native target ownership uses the selected SDK layout");
            native.Bind(legacy ? 41 : 42, &native_fixture::Number);
            Check(target->GetActionCallback() == 92, "native target callbacks retain their method position");
            native.Bind(legacy ? 69 : 70, &native_fixture::Target);
            target->SetTarget(200, true);
            Check(native.value == 300, "target selection preserves its arguments and calling convention");
        }
        Check(!xiui::ashita::View(static_cast<const IParty*>(nullptr)), "compatibility views preserve null availability");
    }

    auto CheckMouse(void) -> void
    {
        xiui::mouse_input mouse;
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == 0, "polling a held button never creates a press");
        Check(mouse.Normalize(WM_LBUTTONDOWN, 0, 0, 0) == MK_LBUTTON, "zero-data callbacks still report a fresh left press");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == MK_LBUTTON, "zero-data movement retains a real held drag without Shift");
        Check(mouse.Normalize(WM_LBUTTONUP, 0, MK_LBUTTON, 0) == 0, "the release message wins over a stale physical snapshot");
        Check(mouse.Normalize(WM_LBUTTONDBLCLK, 0, 0, MK_SHIFT) == (MK_LBUTTON | MK_SHIFT), "double clicks and physical Shift remain separate");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_LBUTTON, 0) == MK_LBUTTON, "releasing Shift does not release the pointer gesture");
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, 0, 0) == 0, "a confirmed lost release can cancel the gesture");
        Check(mouse.Normalize(WM_RBUTTONDOWN, 0, 0, 0) == MK_RBUTTON, "camera button presses are tracked independently");
        const auto wheel = MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA));
        Check(mouse.Normalize(WM_MOUSEWHEEL, wheel, MK_RBUTTON, MK_CONTROL) == (wheel | MK_RBUTTON | MK_CONTROL), "wheel sign and held buttons survive normalization");
        Check(mouse.Normalize(WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON2), MK_RBUTTON, 0) == (MAKEWPARAM(0, XBUTTON2) | MK_RBUTTON | MK_XBUTTON2), "X button identity stays in the high word");
        mouse.Reset();
        Check(mouse.Normalize(WM_MOUSEMOVE, 0, MK_RBUTTON | MK_XBUTTON2, 0) == 0, "suspension drains button history without adopting held input");
    }

    auto CheckPlugin(const char* path) -> void
    {
        const auto module = ::LoadLibraryA(path);
        Check(module != nullptr, "load standalone plugin against an isolated host fixture");
        struct cleanup { HMODULE module; ~cleanup() { ::FreeLibrary(this->module); } } release{module};
        const auto version = reinterpret_cast<double(__stdcall*)(void)>(::GetProcAddress(module, "expGetInterfaceVersion"));
        const auto create = reinterpret_cast<IPlugin*(__stdcall*)(const char*)>(::GetProcAddress(module, "expCreatePlugin"));
        const auto destroy = reinterpret_cast<void(__stdcall*)(void*)>(::GetProcAddress(module, "expDestroyPlugin"));
        Check(version && create && destroy, "standard plugin exports remain available");
        Check(version() == xiui::ashita::HostInterfaceVersion(), "plugin loader receives the supported host interface");
        const auto plugin = create("");
        Check(plugin != nullptr, "create compatible plugin metadata");
        const auto objectVersion = plugin->GetInterfaceVersion();
        destroy(plugin);
        Check(objectVersion == version(), "plugin metadata agrees with the loader export");
    }
}

int main(int argc, char** argv)
{
    HMODULE fixture{};
    try
    {
        Check(argc == 1 || argc == 3, "optional arguments: host fixture DLL and standalone plugin DLL");
        if (argc == 3) { fixture = ::LoadLibraryA(argv[1]); Check(fixture != nullptr, "load isolated host fixture"); }
        CheckInterfaces(); CheckMouse();
        if (argc == 3) CheckPlugin(argv[2]);
        if (fixture) ::FreeLibrary(fixture);
        std::puts(argc == 3 ? "Ashita SDK dispatch, plugin negotiation and mouse normalization passed." :
            "Ashita 4.16/4.30 SDK dispatch and mouse normalization passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        if (fixture) ::FreeLibrary(fixture);
        std::fprintf(stderr, "Ashita compatibility check failed: %s\n", error.what());
        return 1;
    }
}
