#include "world_source.hpp"
#include "ashita_compat.hpp"
#include "compass_radar.hpp"
#include "entities/entity_name_colors.hpp"
#include "Memory.h"
#include "native_pattern.hpp"

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

    auto CompassEntityVisible(const uintptr_t function, const uintptr_t entity) noexcept -> bool
    {
        if (function == 0 || entity == 0) return false;
        __try { return reinterpret_cast<uint8_t(__thiscall*)(void*, uint32_t)>(function)(reinterpret_cast<void*>(entity), 0) != 0; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto CompassActorVisible(const uintptr_t actor) noexcept -> bool
    {
        if (actor == 0) return true;
        uint8_t first{}, second{};
        return Read(actor + 0xB2, first) && Read(actor + 0xB3, second) && (first & 0x80) == 0 && (second & 1) == 0;
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
            return FindUniquePattern(base, size, pattern);
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
        const auto radar = find("A1????????85C0742ED9442404D80D????????8B4C2404894834D95838D94424088B15????????8A44240CD95A308B0D????????88412FC3");
        if (radar != 0 && Read(radar + 1, offset) && inside(offset, 4)) this->compass_state_slot_ = offset;
        const auto markers = find("6A008BCDE8????????84C00F84????????8B85A000000085C0741F8A88B200000084C90F88????????85C0740DF680B3000000010F85????????8B9DD001000085DB");
        if (markers != 0 && Read(markers + 5, relative))
        {
            const auto visibility = markers + 9 + relative;
            constexpr char entry[] = "8A81EE0000005384C056741B3C0174173C0274133C06740F3C07740B3C0874075E32C05BC20400";
            if (inside(visibility, (sizeof(entry) - 1) / 2) && FindUniquePattern(visibility, (sizeof(entry) - 1) / 2, entry) == visibility)
                this->compass_visibility_function_ = visibility;
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
        auto entity = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        const auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        const auto player = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        this->state_.heading_valid = entity != nullptr && party != nullptr && player > 0 && player < entity->GetEntityMapSize();
        if (!this->state_.heading_valid) { this->state_.markers.clear(); return; }
        this->state_.heading = entity->GetHeading(player);
        this->state_.heading_valid = std::isfinite(this->state_.heading);
        if (now < this->next_markers_) return;
        this->next_markers_ = now + 150;
        this->state_.markers.clear();
        const float x = entity->GetLocalPositionX(player), y = entity->GetLocalPositionY(player), height = entity->GetLocalPositionZ(player);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(height)) return;
        const auto limit = std::min(entity->GetEntityMapSize(), 0x900u);
        const auto selected = target != nullptr ? target->GetTargetIndex(0) : 0;
        std::array<bool, 0x900> added{};
        const auto add = [&](const uint32_t index, const uint32_t color) {
            if (index == 0 || index == player || index >= limit || added[index] || entity->GetServerId(index) == 0) return;
            if (this->compass_visibility_function_ != 0 &&
                (!CompassEntityVisible(this->compass_visibility_function_, reinterpret_cast<uintptr_t>(entity->GetRawEntity(index))) ||
                    !CompassActorVisible(entity->GetActorPointer(index)))) return;
            const float dx = entity->GetLocalPositionX(index) - x, dy = entity->GetLocalPositionY(index) - y;
            if (!std::isfinite(dx) || !std::isfinite(dy)) return;
            added[index] = true;
            this->state_.markers.push_back({dx, dy, index == selected ? ReadEntityNameColor(*party, *entity, index) : color, index == selected});
        };
        for (uint32_t member = 1; member < 18; ++member)
            if (party->GetMemberIsActive(member) && party->GetMemberZone(member) == party->GetMemberZone(0))
            {
                const auto index = party->GetMemberTargetIndex(member);
                if (index > 0 && index < limit && entity->GetServerId(index) == party->GetMemberServerId(member))
                    add(index, member < 6 ? 0xFF9AE6DA : 0xFF9EBDF1);
            }
        uintptr_t radar{}; uint8_t enemies{};
        const bool enemyRadar = Read(this->compass_state_slot_, radar) && radar != 0 && Read(radar + 0x2F, enemies) && enemies == 1;
        if (this->compass_visibility_function_ != 0)
        {
            for (uint32_t index = 1; index < limit; ++index)
            {
                if (index == player || added[index] || entity->GetServerId(index) == 0) continue;
                const auto color = CompassRadarColor(entity->GetSpawnFlags(index), entity->GetStatus(index), entity->GetMonstrosityFlag(index) != 0, enemyRadar);
                if (!color) continue;
                const float dz = entity->GetLocalPositionZ(index) - height;
                if (!std::isfinite(dz) || std::abs(dz) >= 4) continue;
                const float dx = entity->GetLocalPositionX(index) - x, dy = entity->GetLocalPositionY(index) - y;
                if (!std::isfinite(dx) || !std::isfinite(dy) || dx * dx + dy * dy > 200 * 200) continue;
                add(index, *color);
            }
        }
        add(selected, 0xFFFFFFFF);
    }
}
