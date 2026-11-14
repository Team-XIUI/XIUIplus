#include "minimap_source.hpp"
#include "ashita_compat.hpp"

#include "Memory.h"
#include "native_pattern.hpp"
#include "world/entity_name_colors.hpp"

#include <algorithm>
#include <cstring>

namespace
{
    template<typename T> auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto NativeFloor(const uintptr_t function, const uintptr_t terrain, float x, float height, float y, int& floor) noexcept -> bool
    {
        __try
        {
            floor = reinterpret_cast<int(__thiscall*)(void*, float, float, float)>(function)(reinterpret_cast<void*>(terrain), x, height, y);
            return floor >= 0 && floor < 128;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto minimap_source::Clear(void) -> void
    {
        this->state_ = {}; this->next_markers_ = this->next_floor_ = 0; this->floor_.reset(); this->zone_ = 0;
        this->zone_records_.clear(); this->map_ = this->single_map_ = nullptr;
        this->party_positions_ = {}; this->next_party_request_ = 0;
    }

    auto minimap_source::HandlePartyPosition(const std::span<const uint8_t> packet, const uint64_t now) -> void
    {
        if (packet.size() < 24) return;
        party_position value;
        std::memcpy(&value.id, packet.data() + 4, 4); std::memcpy(&value.zone, packet.data() + 8, 2);
        std::memcpy(&value.x, packet.data() + 12, 4); std::memcpy(&value.y, packet.data() + 16, 4); std::memcpy(&value.height, packet.data() + 20, 4);
        if (value.id == 0 || value.zone == 0 || value.zone != this->state_.zone || !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.height) ||
            std::abs(value.x) > 100000 || std::abs(value.y) > 100000 || std::abs(value.height) > 100000) return;
        value.at = now;
        auto slot = std::find_if(this->party_positions_.begin(), this->party_positions_.end(), [&](const auto& entry) { return entry.id == value.id || entry.id == 0; });
        if (slot == this->party_positions_.end()) slot = std::min_element(this->party_positions_.begin(), this->party_positions_.end(), [](const auto& a, const auto& b) { return a.at < b.at; });
        *slot = value;
    }

    auto minimap_source::Initialize(void) -> void
    {
        this->Clear(); this->records_.clear(); this->floor_function_ = this->terrain_pointer_ = 0;
        const auto base = reinterpret_cast<uintptr_t>(::GetModuleHandleA("FFXiMain.dll"));
        IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS nt{};
        if (!Read(base, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE || !Read(base + dos.e_lfanew, nt) || nt.Signature != IMAGE_NT_SIGNATURE) return;
        const auto size = nt.OptionalHeader.SizeOfImage;
        const auto inside = [base, size](const uintptr_t address, const size_t length) { return address >= base && length <= size && address - base <= size - length; };
        const auto find = [base, size](const char* pattern) {
            return FindUniquePattern(base, size, pattern);
        };
        const auto maps = find("8A0D????????5333C05684C95774??8A5424188B7424148B7C2410B9");
        uintptr_t table{};
        if (maps != 0 && Read(maps + 28, table) && inside(table, sizeof(native_map_record)))
        {
            bool complete{};
            for (size_t index = 0; index < 2048; ++index)
            {
                native_map_record row{};
                if (!inside(table + index * sizeof(row), sizeof(row)) || !Read(table + index * sizeof(row), row)) break;
                if (row.scale == 0) { complete = true; break; }
                if (row.zone <= -1024 || row.zone >= 1024 || std::abs(static_cast<int>(row.scale)) > 64) break;
                if (row.zone > 0) this->records_.push_back(row);
            }
            if (!complete) this->records_.clear();
        }
        this->InitializeFloorLookup(base, size);
    }

    auto minimap_source::InitializeFloorLookup(const uintptr_t base, const size_t size) -> void
    {
        this->floor_function_ = this->terrain_pointer_ = 0;
        // Match the floor call itself, independently of the native map display and collision implementation.
        const auto caller = FindUniquePattern(base, size, "8B0D????????565057E8????????");
        const auto function = FindUniquePattern(base, size, "8B542408568D4424108BF18B4C2410508B44240C");
        int32_t relative{}; uintptr_t pointer{};
        if (caller != 0 && function != 0 && Read(caller + 10, relative) && caller + 14 + relative == function &&
            Read(caller + 2, pointer) && size >= sizeof(uintptr_t) && pointer >= base && pointer - base <= size - sizeof(uintptr_t))
        { this->floor_function_ = function; this->terrain_pointer_ = pointer; }
    }

    auto minimap_source::Floor(const float x, const float height, const float y) const -> std::optional<int>
    {
        uintptr_t terrain{}; int floor{};
        if (this->floor_function_ == 0 || !Read(this->terrain_pointer_, terrain) || terrain == 0) return std::nullopt;
        return NativeFloor(this->floor_function_, terrain, x, height, y, floor) ? std::optional<int>{floor} : std::nullopt;
    }

    auto minimap_source::SelectMap(const uint16_t zone, const uint64_t now, const float x, const float height, const float y) -> const native_map_record*
    {
        if (this->zone_ != zone)
        {
            this->zone_ = zone; this->zone_records_.clear();
            for (const auto& record : this->records_)
                if (record.zone == zone && record.scale != 0 && record.texture > 0) this->zone_records_.push_back(record);
            this->single_map_ = this->zone_records_.size() == 1 ? &this->zone_records_.front() : nullptr;
            this->map_ = this->single_map_; this->floor_.reset(); this->next_floor_ = this->next_markers_ = 0;
        }
        if (this->single_map_ == nullptr && !this->zone_records_.empty() && now >= this->next_floor_)
        {
            this->next_floor_ = now + 100; this->floor_ = this->Floor(x, height, y);
            this->map_ = FindMinimapRecord(this->zone_records_, zone, this->floor_);
        }
        return this->map_;
    }

    auto minimap_source::Update(IAshitaCore* core, const uint64_t now, const bool npcs, const bool distantParty, const bool enemies) -> void
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto party = memory != nullptr ? xiui::ashita::View(memory->GetParty()) : nullptr;
        auto entity = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        const auto index = party != nullptr ? party->GetMemberTargetIndex(0) : 0;
        if (player == nullptr || player->GetIsZoning() || player->GetMainJob() == 0 || entity == nullptr || index == 0 ||
            index >= entity->GetEntityMapSize() || party->GetMemberServerId(0) == 0 || entity->GetServerId(index) != party->GetMemberServerId(0))
        { this->Clear(); return; }
        const auto zone = party->GetMemberZone(0);
        const float x = entity->GetLocalPositionX(index), y = entity->GetLocalPositionY(index), height = entity->GetLocalPositionZ(index), heading = entity->GetHeading(index);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(height) || !std::isfinite(heading)) { this->Clear(); return; }
        const auto* map = this->SelectMap(zone, now, x, height, y);
        if (map == nullptr) { this->state_ = {}; this->state_.zone = zone; this->next_markers_ = 0; return; }
        const bool changed = !this->state_.available || this->state_.zone != zone || this->state_.floor != map->floor;
        if (changed)
        {
            this->state_ = {}; this->next_markers_ = 0; this->state_.zone = zone; this->state_.floor = map->floor;
            this->state_.map_number = map->number;
            auto* resources = core->GetResourceManager();
            const auto* name = resources != nullptr ? resources->GetString("zones.names", zone) : nullptr;
            this->state_.name = name != nullptr ? name : "Unknown zone";
            char path[1024]{};
            if (resources != nullptr) resources->GetFilePath(MinimapNativeFile(*map), path, sizeof(path) - 1);
            this->state_.native_map = path;
        }
        this->state_.available = true; this->state_.player = MinimapUV(*map, x, y); this->state_.heading = heading;
        if (now < this->next_markers_) return;
        this->next_markers_ = now + 150; this->state_.markers.clear();
        const bool singleMap = this->single_map_ != nullptr;
        const auto add = [&](const uint32_t target, const uint32_t color, const minimap_marker_kind kind) -> bool {
            if (target == 0 || target == index || target >= entity->GetEntityMapSize() || entity->GetServerId(target) == 0) return false;
            const float mx = entity->GetLocalPositionX(target), my = entity->GetLocalPositionY(target), mh = entity->GetLocalPositionZ(target);
            if (!std::isfinite(mx) || !std::isfinite(my) || !std::isfinite(mh)) return false;
            const auto* other = singleMap ? map : FindMinimapRecord(this->zone_records_, zone, this->Floor(mx, mh, my));
            if (other == nullptr || other->floor != map->floor) return false;
            this->state_.markers.push_back({MinimapUV(*map, mx, my), color, kind});
            return true;
        };
        const auto pet = entity->GetPetTargetIndex(index);
        if (npcs || enemies)
        {
            std::array<uint32_t, 18> group{};
            if (enemies)
                for (uint32_t member = 0; member < group.size(); ++member)
                    if (member == 0 || party->GetMemberIsActive(member)) group[member] = party->GetMemberServerId(member);
            size_t npcCount{}, enemyCount{};
            const auto limit = std::min(entity->GetEntityMapSize(), 4096u);
            for (uint32_t candidate = 1; candidate < limit && ((npcs && npcCount < 128) || (enemies && enemyCount < 256)); ++candidate)
            {
                if (candidate == pet || candidate == index || entity->GetServerId(candidate) == 0) continue;
                const auto flags = entity->GetSpawnFlags(candidate), render = entity->GetRenderFlags0(candidate);
                const bool npc = npcs && npcCount < 128 && IsMinimapNpc(flags, render);
                const bool enemy = enemies && enemyCount < 256 && IsMinimapEnemy(flags, render, entity->GetHPPercent(candidate));
                if (!npc && !enemy) continue;
                const float dx = entity->GetLocalPositionX(candidate) - x, dy = entity->GetLocalPositionY(candidate) - y;
                if (!std::isfinite(dx) || !std::isfinite(dy) || dx * dx + dy * dy > 120 * 120) continue;
                if (npc)
                {
                    if (add(candidate, minimap_npc_color, minimap_marker_kind::npc)) ++npcCount;
                }
                else
                {
                    const auto claim = entity->GetClaimStatus(candidate);
                    const auto color = MinimapEnemyColor(claim, IsGroupClaim(claim, group));
                    if (add(candidate, color, minimap_marker_kind::enemy)) ++enemyCount;
                }
            }
        }
        bool requestPositions{};
        for (uint32_t member = 1; member < 18; ++member)
            if (party->GetMemberIsActive(member) && party->GetMemberZone(member) == zone)
            {
                const auto target = party->GetMemberTargetIndex(member);
                const auto id = party->GetMemberServerId(member);
                const auto color = member < 6 ? 0xFF9AE6DA : 0xFF9EBDF1;
                const auto kind = member < 6 ? minimap_marker_kind::party : minimap_marker_kind::alliance;
                if (target != 0 && target < entity->GetEntityMapSize() && entity->GetServerId(target) == id && entity->GetActorPointer(target) != 0 && add(target, color, kind)) continue;
                if (!distantParty) continue;
                requestPositions = true;
                for (const auto& position : this->party_positions_)
                    if (position.id == id && position.zone == zone && now >= position.at && now - position.at <= 20000)
                    {
                        const auto* other = singleMap ? map : FindMinimapRecord(this->zone_records_, zone, this->Floor(position.x, position.height, position.y));
                        if (other != nullptr && other->floor == map->floor) this->state_.markers.push_back({MinimapUV(*map, position.x, position.y), color, kind});
                        break;
                    }
            }
        if (requestPositions && now >= this->next_party_request_ && core->GetPacketManager() != nullptr)
        {
            this->next_party_request_ = now + 10000;
            std::array<uint8_t, 8> packet{}; const uint32_t requestZone = zone;
            std::memcpy(packet.data() + 4, &requestZone, 4);
            core->GetPacketManager()->AddOutgoingPacket(0xD2, static_cast<uint32_t>(packet.size()), packet.data());
        }
        add(pet, 0xFFF0C884, minimap_marker_kind::pet);
        if (const auto target = xiui::ashita::View(memory->GetTarget()))
        {
            const auto selected = target->GetTargetIndex(0);
            if (selected > 0 && selected < entity->GetEntityMapSize())
            {
                const auto name = ReadEntityNameState(*party, *entity, selected);
                const auto color = IsMonster(name.kind) ? MinimapEnemyColor(entity->GetClaimStatus(selected), name.kind == entity_class::group_mob) : name.color;
                add(selected, color, minimap_marker_kind::target);
            }
        }
    }
}
