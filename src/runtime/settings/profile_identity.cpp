#include "profile_identity.hpp"
#include "Memory.h"
#include <cstring>

namespace
{
    template<typename T>
    auto ReadMemory(uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    struct login_character
    {
        uint32_t content_id{};
        uint16_t local_id{}, world{}, status{};
        uint8_t flags[2]{};
        char name[16]{};
    };
    static_assert(sizeof(login_character) == 28);
}

namespace xiui
{
    auto profile_identity_source::Initialize(void) -> void
    {
        *this = {};
        const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleA("FFXiMain.dll"));
        IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS nt{};
        if (!ReadMemory(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
            !ReadMemory(base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE) return;
        const auto find = [base, size = nt.OptionalHeader.SizeOfImage](const char* pattern) {
            const auto address = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            return address != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0 ? address : 0;
        };
        // The same character-list lookups used by Ashita's ffxi.account library.
        this->characters_ = find("8B0D????????8D04808B8481????????C3");
        this->selection_ = find("8B0D????????8B81????????50E8");
    }

    auto profile_identity_source::Read(IAshitaCore* core) const -> std::optional<std::string>
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        if (player == nullptr || party == nullptr || entity == nullptr || player->GetIsZoning() || player->GetMainJob() == 0 ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn)) return {};
        const auto id = party->GetMemberServerId(0), index = party->GetMemberTargetIndex(0);
        if (id == 0 || index == 0 || index >= entity->GetEntityMapSize() || entity->GetServerId(index) != id || party->GetMemberZone(0) == 0) return {};
        if (this->characters_ == 0 || this->selection_ == 0) return "local-" + std::to_string(id);

        uintptr_t address{}, offset{}, characters{}, selection{};
        uint32_t count{}, selected{};
        if (!ReadMemory(this->characters_ + 2, address) || !ReadMemory(address, characters) || characters == 0 ||
            !ReadMemory(this->characters_ + 12, offset) || offset < 4 || offset > 0x1000000) return {};
        characters += offset;
        if (!ReadMemory(characters - 4, count) || count == 0 || count > 16 ||
            !ReadMemory(this->selection_ + 2, address) || !ReadMemory(address, selection) || selection == 0 ||
            !ReadMemory(this->selection_ + 8, offset) || offset > 0x1000000 || !ReadMemory(selection + offset, selected) || selected >= count) return {};
        login_character character{};
        const auto* name = party->GetMemberName(0);
        // lpkt_chr_info_sub2 has a 140-byte stride; only its identity prefix is read.
        if (!ReadMemory(characters + selected * 140, character) || name == nullptr ||
            strnlen_s(character.name, sizeof(character.name)) == sizeof(character.name) ||
            character.name[0] == '\0' || ::_stricmp(character.name, name) != 0) return {};
        return "world-" + std::to_string(character.world) + "-" + std::to_string(id);
    }
}
