#pragma once

#include "design_system.hpp"

#include <map>
#include <functional>
#include <string>
#include <string_view>

namespace xiui
{
    class settings_store
    {
    public:
        explicit settings_store(std::string path);
        static auto FromIniText(std::string_view text) -> settings_store;
        static auto CachedFile(std::string path) -> settings_store;
        static auto AtomicFile(std::string path) -> settings_store;
        auto ReadString(const char* section, const char* key, const char* fallback = "") const -> std::string;
        auto WriteString(const char* section, const char* key, const std::string& value) const -> void;
        auto Preferences(std::string path) const -> settings_store;
        auto OverlayPreferences(const settings_store& source) const -> void;
        auto VisitPreferences(const std::function<void(std::string_view, std::string_view, std::string_view)>& visit) const -> void;
        auto ToIniText(void) const -> std::string;

        auto ReadBool(const char* section, const char* key, bool fallback) const -> bool;
        auto ReadInt(const char* section, const char* key, int fallback) const -> int;
        auto ReadFloat(const char* section, const char* key, float fallback) const -> float;
        auto ReadColor(const char* section, const char* key, D3DCOLOR fallback) const -> D3DCOLOR;
        auto ReadArgb(const char* section, const char* key, D3DCOLOR fallback) const -> D3DCOLOR;
        auto ReadRect(const char* section, const ui::rect& fallback) const -> ui::rect;

        auto WriteBool(const char* section, const char* key, bool value) const -> void;
        auto WriteInt(const char* section, const char* key, int value) const -> void;
        auto WriteFloat(const char* section, const char* key, float value) const -> void;
        auto WriteColor(const char* section, const char* key, D3DCOLOR value) const -> void;
        auto WriteArgb(const char* section, const char* key, D3DCOLOR value) const -> void;
        auto WriteRect(const char* section, const ui::rect& value) const -> void;
        auto Flush(void) const -> bool;

    private:
        struct key_less
        {
            auto operator()(const std::string& left, const std::string& right) const -> bool;
        };

        std::string path_{};
        bool cache_sections_{};
        bool atomic_file_{};
        mutable std::map<std::string, std::map<std::string, std::string, key_less>, key_less> reads_{};
        mutable std::map<std::string, std::map<std::string, std::string, key_less>, key_less> writes_{};

        auto PendingValue(const char* section, const char* key) const -> const std::string*;
        auto CachedValue(const char* section, const char* key) const -> const std::string*;
        auto ReadText(const char* section, const char* key, const char* fallback, char* value, DWORD size) const -> void;
        auto WriteText(const char* section, const char* key, const char* value) const -> void;
    };
} // namespace xiui
