#include "native_ui_sounds.hpp"

#include "Ashita.h"
#include "Memory.h"

#include <cstdio>
#include <cstring>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    template<typename T>
    auto Read(const uintptr_t address, T& value) noexcept -> bool
    {
        if (address == 0) return false;
        __try { std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T)); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    auto PlayNative(const uintptr_t function) noexcept -> bool
    {
        if (function == 0) return false;
        __try { reinterpret_cast<void(__cdecl*)(void)>(function)(); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }
}

namespace xiui::ui
{
    auto native_ui_sounds::Initialize(void) -> void
    {
        this->clips_.Initialize(reinterpret_cast<HMODULE>(&__ImageBase));
        this->sounds_ = {}; this->hovered_ = this->suppress_hover_until_ = 0; this->initial_hover_ = true;
        const auto base = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        const auto size = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        if (base == 0 || size < sizeof(uintptr_t)) return;
        // Native resource wrappers retain the client's sound assets and volume settings.
        constexpr const char* tags[]{"30303031", "30303032", "30303133"};
        for (size_t index = 0; index < std::size(tags); ++index)
        {
            char pattern[256]{};
            std::snprintf(pattern, sizeof(pattern), "51A1????????5685C075??A1????????68%s8D4C24086A3D8B8008000400518B008BC8E8????????8B0085C0A3????????74??8B3085F674??D905????????D80D????????E8????????506A408BCEE8????????5E59C3", tags[index]);
            const auto match = Ashita::Memory::FindPattern(base, size, pattern, 0, 0);
            uintptr_t cache{}, resources{};
            if (match == 0 || Ashita::Memory::FindPattern(base, size, pattern, 0, 1) != 0 ||
                !Read(match + 2, cache) || !Read(match + 12, resources) ||
                cache < base || cache - base > size - sizeof(uintptr_t) ||
                resources < base || resources - base > size - sizeof(uintptr_t)) continue;
            this->sounds_[index] = {match, cache, resources};
        }
    }

    auto native_ui_sounds::Available(void) const -> bool
    {
        for (size_t index = 0; index < 3; ++index) if (this->sounds_[index].function == 0) return false;
        return true;
    }

    auto native_ui_sounds::Play(const native_ui_sound sound) const -> bool
    {
        const auto index = static_cast<size_t>(sound);
        if (index >= this->sounds_.size()) return false;
        const auto& entry = this->sounds_[index];
        if (entry.function == 0 && index >= 3) return this->clips_.Play(index - 3);
        if (entry.function == 0) return false;
        uintptr_t cache{}, resources{};
        if (entry.cache_slot != 0 && (!Read(entry.cache_slot, cache) ||
            (cache == 0 && (!Read(entry.resource_slot, resources) || resources == 0)))) return false;
        return PlayNative(entry.function);
    }

    auto native_ui_sounds::MenuOpened(const uint64_t now, const bool audible) -> void
    {
        this->hovered_ = 0;
        this->initial_hover_ = true;
        this->suppress_hover_until_ = now + 140;
        if (audible) this->Play(native_ui_sound::menu_open);
    }

    auto native_ui_sounds::Hover(const uint64_t target, const uint64_t now, const bool enabled) -> void
    {
        const bool changed = target != this->hovered_;
        this->hovered_ = target;
        if (this->initial_hover_) { this->initial_hover_ = false; return; }
        if (changed && target != 0 && enabled && now >= this->suppress_hover_until_) this->Play(native_ui_sound::hover);
    }

    auto native_ui_sounds::Select(const uint64_t now) -> void
    {
        this->suppress_hover_until_ = now + 140;
        this->Play(native_ui_sound::select);
    }
}
