#include "world_source.hpp"
#include "entities/entity_name_colors.hpp"
#include "Memory.h"

#include <algorithm>
#include <cstring>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ReadTime(const uintptr_t function, uint32_t& raw) noexcept -> bool
    {
        if (function == 0) return false;
        __try { raw = reinterpret_cast<uint32_t(__stdcall*)(void)>(function)(); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto ReadCompassRotation(const uintptr_t function, float& rotation) noexcept -> bool
    {
        if (function == 0) return false;
        __try { rotation = reinterpret_cast<float(__cdecl*)(void)>(function)(); return std::isfinite(rotation); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto world_source::Initialize(void) -> void
    {
        *this = {};
        const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleA("FFXiMain.dll"));
        IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS nt{};
        if (!Read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || !Read(base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE) return;
        const auto size = nt.OptionalHeader.SizeOfImage;
        const auto inside = [base, size](const uintptr_t address, const size_t bytes) { return address >= base && bytes <= size && address - base <= size - bytes; };
        const auto find = [base, size](const char* pattern) {
            const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            return match != 0 && Ashita::Memory::FindPattern(base, size, pattern, 0, 1) == 0 ? match : 0;
        };
        const auto time = find("E8????????0305????????C3");
        int32_t relative{}; uintptr_t offset{};
        if (time != 0 && Read(time + 1, relative) && Read(time + 7, offset) && inside(time + 5 + relative, 1) && inside(offset, 4)) this->time_function_ = time;
        const auto weather = find("66A1????????663D????72");
        if (weather != 0 && Read(weather + 2, offset) && inside(offset, 2)) this->weather_address_ = offset;
        const auto compass = find("51C744240000000000E8????????85C0740EE8????????8BC8E8????????EB04D9442400D82D????????D815");
        if (compass != 0)
        {
            bool valid = true;
            for (const size_t call : {9u, 18u, 25u})
                valid = valid && Read(compass + call + 1, relative) && inside(compass + call + 5 + relative, 1);
            if (valid) this->compass_rotation_function_ = compass;
        }
    }

    auto world_source::Update(IAshitaCore* core, const uint64_t now) -> void
    {
        this->state_.compass_rotation_valid = ReadCompassRotation(this->compass_rotation_function_, this->state_.compass_rotation);
        if (now >= this->next_environment_)
        {
            this->next_environment_ = now + 250;
            uint32_t raw{}; uint16_t weather{};
            this->state_.time_valid = ReadTime(this->time_function_, raw);
            if (this->state_.time_valid) this->state_.time = DecodeVanaTime(raw);
            this->state_.weather_valid = Read(this->weather_address_, weather) && weather < weather_names.size();
            this->state_.weather = this->state_.weather_valid ? weather : -1;
        }
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* entity = memory != nullptr ? memory->GetEntity() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        const auto player = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        this->state_.heading_valid = entity != nullptr && player > 0 && player < entity->GetEntityMapSize();
        if (!this->state_.heading_valid) { this->state_.markers.clear(); return; }
        this->state_.heading = entity->GetHeading(player);
        this->state_.heading_valid = std::isfinite(this->state_.heading);
        if (now < this->next_markers_) return;
        this->next_markers_ = now + 150;
        this->state_.markers.clear();
        const float x = entity->GetLocalPositionX(player), y = entity->GetLocalPositionY(player);
        const auto add = [&](const uint32_t index, const uint32_t color, const bool selected) {
            if (index == 0 || index == player || index >= entity->GetEntityMapSize() || entity->GetServerId(index) == 0) return;
            const float dx = entity->GetLocalPositionX(index) - x, dy = entity->GetLocalPositionY(index) - y;
            if (std::isfinite(dx) && std::isfinite(dy)) this->state_.markers.push_back({dx, dy, color, selected});
        };
        for (uint32_t member = 1; member < 18; ++member)
            if (party->GetMemberIsActive(member) && party->GetMemberZone(member) == party->GetMemberZone(0)) add(party->GetMemberTargetIndex(member), member < 6 ? 0xFF9AE6DA : 0xFF9EBDF1, false);
        if (target != nullptr)
        {
            const auto index = target->GetTargetIndex(0);
            if (index > 0 && index < entity->GetEntityMapSize()) add(index, ReadEntityNameColor(*party, *entity, index), true);
        }
    }
}
