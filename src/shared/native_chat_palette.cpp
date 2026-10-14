#include "native_chat_palette.hpp"

#include "Ashita.h"
#include "Memory.h"

#include <cstring>

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui
{
    auto native_chat_palette::Contains(const uintptr_t address, const size_t bytes) const -> bool
    {
        return this->module_ != 0 && address >= this->module_ && bytes <= this->size_ && address - this->module_ <= this->size_ - bytes;
    }

    auto native_chat_palette::Initialize(const uintptr_t module, const uint32_t size) -> void
    {
        *this = {};
        this->module_ = module; this->size_ = size;
        if (module == 0 || size == 0) return;
        const auto table = [this](const char* pattern, const size_t offset) {
            const auto match = Ashita::Memory::FindPattern(this->module_, this->size_, pattern, 0, 0);
            uintptr_t address{};
            return match != 0 && Ashita::Memory::FindPattern(this->module_, this->size_, pattern, 0, 1) == 0 &&
                this->Contains(match + offset, sizeof(address)) && ::Read(match + offset, address) && this->Contains(address, 4) ? address : 0;
        };
        this->modes_ = table("A1????????33C96685C07418668B542404663BC27411668B048D????????416685C075ED33C0C3", 1);
        this->colors_ = table("A0????????33C984C0741A0FBF54240425FF0000003BC274128A04CD????????4184C075EBB880808080C3", 1);
        this->fixed_ = table("0FBF4424049933C22BC283F8117D088B0485????????C389442404E9????????", 18);
    }

    auto native_chat_palette::Read(void) const -> chat_palette
    {
        chat_palette result{};
        struct color_entry { uint32_t index, color; };
        for (size_t i = 0; i < 256; ++i)
        {
            color_entry entry{};
            const auto address = this->colors_ + i * sizeof(entry);
            if (!this->Contains(address, sizeof(entry)) || !::Read(address, entry) || entry.index == 0) break;
            // Native lookups stop at the first matching ID; later entries may reuse it.
            if (entry.index < result.colors.size() && result.colors[entry.index] == 0)
                result.colors[entry.index] = ChatColor(entry.color);
        }
        struct mode_entry { uint16_t mode; uint8_t color, category; };
        for (size_t i = 0; i < 256; ++i)
        {
            mode_entry entry{};
            const auto address = this->modes_ + i * sizeof(entry);
            if (!this->Contains(address, sizeof(entry)) || !::Read(address, entry) || entry.mode == 0) break;
            if (entry.mode < result.modes.size() && result.modes[entry.mode] == 0)
                result.modes[entry.mode] = result.colors[entry.color];
        }
        result.available = result.modes[1] != 0;
        for (size_t index = 0; index < 17; ++index)
        {
            uint32_t native{};
            const auto address = this->fixed_ + index * sizeof(native);
            if (this->Contains(address, sizeof(native)) && ::Read(address, native) && native != 0) result.colors[index] = ChatColor(native);
        }
        for (size_t index = 0; index < result.modes.size(); ++index)
            result.colors[256 + index] = result.modes[index] != 0 ? result.modes[index] : 0xFFFFFFFF;
        return result;
    }
}
