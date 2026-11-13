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
        // Serves later reads of keys this store lacks from one parse of its file.
        auto IndexFile(void) const -> void;
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
        // Lookups that made Windows parse the file from disk; captures on the frame thread should add none.
        static auto FileLookups(void) -> uint32_t;

    private:
        struct key_less
        {
            auto operator()(const std::string& left, const std::string& right) const -> bool;
        };

        using field_map = std::map<std::string, std::string, key_less>;
        // Consecutive writes usually share a section. Copies start without the hint.
        struct section_hint
        {
            std::string name{};
            field_map* fields{};
            section_hint(void) = default;
            section_hint(const section_hint&) {}
            auto operator=(const section_hint&) -> section_hint& { this->fields = nullptr; return *this; }
        };

        std::string path_{};
        mutable bool cache_sections_{};
        mutable bool cache_file_loaded_{};
        bool atomic_file_{};
        mutable std::map<std::string, field_map, key_less> reads_{};
        mutable std::map<std::string, field_map, key_less> writes_{};
        mutable section_hint write_hint_{};

        auto PendingValue(const char* section, const char* key) const -> const std::string*;
        auto CachedValue(const char* section, const char* key) const -> const std::string*;
        auto CacheFile(void) const -> bool;
        auto CacheText(std::string bytes) const -> bool;
        auto ReadText(const char* section, const char* key, const char* fallback, char* value, DWORD size) const -> void;
        auto WriteText(const char* section, const char* key, const char* value) const -> void;
    };
} // namespace xiui
