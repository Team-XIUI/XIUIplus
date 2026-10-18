#include "mob_database.hpp"
#include "resources/embedded_resource.hpp"
#include "resources/phoenix_spawn_data.hpp"

#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <span>
#include <map>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui
{
    namespace
    {
        struct database
        {
            std::vector<uint8_t> storage{};
            std::span<const mob_record> records{};
            std::string_view names{};
            database(void)
            {
                const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
                this->storage = resources::LoadEmbeddedResource(module, 701, 16 * 1024 * 1024);
                const auto size = this->storage.size();
                const auto* bytes = this->storage.data();
                if (size < 16 || std::memcmp(bytes, "XIM1", 4) != 0) return;
                uint32_t count{}, stride{}, names{};
                std::memcpy(&count, bytes + 4, 4); std::memcpy(&stride, bytes + 8, 4); std::memcpy(&names, bytes + 12, 4);
                if (stride != sizeof(mob_record) || count > 100000 || uint64_t{16} + uint64_t{count} * stride + names != size) return;
                this->records = {reinterpret_cast<const mob_record*>(bytes + 16), count};
                this->names = {reinterpret_cast<const char*>(bytes + 16 + count * stride), names};
            }
        };
        auto Data(void) -> const database& { static const database value; return value; }
    }

    auto MobById(const uint32_t serverId) -> const mob_record*
    {
        const auto records = Data().records;
        const auto found = std::lower_bound(records.begin(), records.end(), serverId,
            [](const mob_record& mob, const uint32_t id) { return mob.id < id; });
        return found != records.end() && found->id == serverId ? &*found : nullptr;
    }

    auto MobIsPlaceholder(const uint32_t serverId) -> bool
    {
        const auto& rows = resources::phoenix_spawn_relations;
        auto row = std::lower_bound(rows.begin(), rows.end(), serverId, [](const auto& entry, uint32_t id) { return entry.member < id; });
        for (; row != rows.end() && row->member == serverId; ++row) if (row->placeholder) return true;
        return false;
    }

    auto MobSpawnHints(const uint32_t serverId) -> std::vector<std::string>
    {
        std::map<std::string, std::pair<uint32_t, uint32_t>> groups;
        const auto& rows = resources::phoenix_spawn_relations;
        auto row = std::lower_bound(rows.begin(), rows.end(), serverId, [](const auto& entry, uint32_t id) { return entry.member < id; });
        for (; row != rows.end() && row->member == serverId; ++row)
        {
            const auto* target = MobById(row->target);
            const auto name = target != nullptr ? std::string{MobDataName(*target)} : std::string{"Notorious monster"};
            auto& value = groups[row->placeholder ? "PH: " + name : "Lottery"];
            value.first = std::min(100u, value.first + row->chance);
            value.second = std::max(value.second, row->cooldown);
        }
        std::vector<std::string> result;
        for (const auto& [name, values] : groups)
        {
            auto text = name + " (up to " + std::to_string(values.first) + "%)";
            if (values.second != 0) text += "; base cooldown " + std::to_string(values.second / 60) + "m";
            result.push_back(std::move(text));
        }
        return result;
    }

    auto MobForEntity(const uint32_t serverId, const uint16_t zone, const bool monster) -> const mob_record*
    {
        const auto* mob = monster && zone != 0 ? MobById(serverId) : nullptr;
        return mob != nullptr && mob->zone == zone ? mob : nullptr;
    }

    auto MobDataName(const mob_record& mob) -> std::string_view
    {
        const auto names = Data().names;
        if (mob.name_offset >= names.size()) return {};
        const auto tail = names.substr(mob.name_offset);
        const auto length = tail.find('\0');
        return length != std::string_view::npos ? tail.substr(0, length) : std::string_view{};
    }

    auto MobLevel(const mob_record& mob) -> std::string
    {
        if (mob.minimum_level == 0 || mob.maximum_level == 0) return "Unknown level";
        const auto level = std::to_string(mob.minimum_level);
        return mob.minimum_level == mob.maximum_level ? "Lv. " + level : "Lv. " + level + "-" + std::to_string(mob.maximum_level);
    }

    auto MobDetection(const mob_record& mob) -> std::string
    {
        std::string result;
        const auto add = [&](const char* text) { if (!result.empty()) result += ", "; result += text; };
        if ((mob.detects & 0x001) != 0) add("Sight");
        if ((mob.detects & 0x002) != 0) add("Sound");
        if ((mob.detects & 0x004) != 0) add("Low HP");
        if ((mob.detects & 0x020) != 0) add("Magic");
        if ((mob.detects & 0x040) != 0) add("Abilities");
        if ((mob.detects & 0x100) != 0) add("Scent");
        if ((mob.flags & 64) != 0) add("True detection");
        return result.empty() ? "None recorded" : result;
    }

    auto MobImmunities(const mob_record& mob) -> std::string
    {
        constexpr const char* names[]{"Addle", "Gravity", "Bind", "Stun", "Silence", "Paralyze", "Blind", "Slow", "Poison",
            "Elegy", "Requiem", "Light Sleep", "Dark Sleep", "Aspir", "Terror", "Dispel", "Petrify", "Plague", "Curse"};
        std::string result;
        for (size_t bit = 0; bit < std::size(names); ++bit)
            if ((mob.immunity & (1u << bit)) != 0) { if (!result.empty()) result += ", "; result += names[bit]; }
        return result.empty() ? "None recorded" : result;
    }
}
