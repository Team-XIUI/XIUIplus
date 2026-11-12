#include "satchel_native_order.hpp"
#include "native_pattern.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace xiui
{
    namespace
    {
        // One sort object per container; each holds four 81-entry lists of {u32 key, u16 item, u16 slot}.
        constexpr size_t object_count = 18, list_entries = 81, entry_size = 8, list_size = list_entries * entry_size;
        constexpr uintptr_t container_offset = 0, loaded_offset = 4, mode_offset = 0xC, count_offset = 0x18, list_offset = 0x804;

        auto ReadBytes(const uintptr_t address, void* output, const size_t size) noexcept -> bool
        {
            if (address == 0) return false;
            __try { std::memcpy(output, reinterpret_cast<const void*>(address), size); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        template<typename T>
        auto ReadValue(const uintptr_t address, T& value) noexcept -> bool { return ReadBytes(address, &value, sizeof(T)); }
    }

    auto SatchelApplyNativeOrder(std::vector<satchel_item>& items, const std::span<const satchel_native_entry> order) -> void
    {
        if (order.empty() || items.size() < 2) return;
        std::array<uint16_t, 256> ids{};
        for (const auto& item : items) ids[item.slot] = item.id;
        std::array<size_t, 256> rank; rank.fill(SIZE_MAX);
        for (size_t index = 0; index < order.size(); ++index)
        {
            const auto [id, slot] = order[index];
            if (id != 0 && ids[slot] == id && rank[slot] == SIZE_MAX) rank[slot] = index;
        }
        std::stable_sort(items.begin(), items.end(), [&](const auto& left, const auto& right) {
            return rank[left.slot] != rank[right.slot] ? rank[left.slot] < rank[right.slot] : left.slot < right.slot;
        });
    }

    auto satchel_native_order::Initialize(void) -> void
    {
        this->initialized_ = true;
        const auto module = reinterpret_cast<uintptr_t>(::GetModuleHandleA("FFXiMain.dll"));
        if (module == 0) return;
        const auto& dos = *reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
        const auto& nt = *reinterpret_cast<const IMAGE_NT_HEADERS*>(module + dos.e_lfanew);
        const uintptr_t base = module; const size_t size = nt.OptionalHeader.SizeOfImage;
        // Login registers each container's sort object from consecutive table slots.
        const auto registration = FindUniquePattern(base, size, "8B0D????????68????????68????????6A00E8????????8B0D????????6A0068????????6A01E8");
        // Slot-at-position and manual-move methods pin the mode, count and entry layout used below.
        const auto slotAt = FindUniquePattern(base, size, "A1????????85C0750532C0C204008B410C8B54240481E2FF0000008D04C08D04C003C28A84C10A080000C20400");
        const auto move = FindUniquePattern(base, size, "A1????????83EC0885C053568BF10F84????????8B460C8B4C24148A5C3018");
        uintptr_t table{}, second{}, enabled{}, moveEnabled{};
        if (registration == 0 || slotAt == 0 || move == 0 || !ReadValue(registration + 2, table) || !ReadValue(registration + 25, second) ||
            second != table + 4 || !ReadValue(slotAt + 1, enabled) || !ReadValue(move + 1, moveEnabled) || enabled != moveEnabled) return;
        this->table_ = table; this->enabled_ = enabled;
    }

    auto satchel_native_order::Read(const uint8_t bag) -> std::vector<satchel_native_entry>
    {
        if (!this->initialized_) this->Initialize();
        uint32_t enabled{};
        if (this->table_ == 0 || !ReadValue(this->enabled_, enabled) || enabled == 0) return {};
        for (size_t index = 0; index < object_count; ++index)
        {
            uintptr_t object{}; uint32_t container{};
            if (!ReadValue(this->table_ + index * sizeof(uint32_t), object) || object == 0 ||
                !ReadValue(object + container_offset, container) || container != bag) continue;
            uint8_t loaded{}, count{}; uint32_t mode{};
            if (!ReadValue(object + loaded_offset, loaded) || loaded == 0 || !ReadValue(object + mode_offset, mode) || mode > 3 ||
                !ReadValue(object + count_offset + mode, count) || count > list_entries) return {};
            std::array<uint8_t, list_size> list{};
            if (!ReadBytes(object + list_offset + mode * list_size, list.data(), count * entry_size)) return {};
            std::vector<satchel_native_entry> result;
            for (size_t entry = 0; entry < count; ++entry)
            {
                uint16_t id{}, slot{};
                std::memcpy(&id, list.data() + entry * entry_size + 4, sizeof(id));
                std::memcpy(&slot, list.data() + entry * entry_size + 6, sizeof(slot));
                if (id != 0 && id != 0xFFFF && slot != 0 && slot <= 80) result.emplace_back(id, static_cast<uint8_t>(slot));
            }
            return result;
        }
        return {};
    }
}
