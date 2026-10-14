#pragma once

#include "runtime_api.hpp"

#include <cstdint>
#include <mutex>
#include <string>
#include <deque>

namespace xiui
{
    class runtime_host
    {
    public:
        runtime_host(void);
        ~runtime_host(void);

        auto Initialize(IAshitaCore* core, const char* installPath) -> bool;
        auto Shutdown(void) -> void;
        auto PollReload(std::string& message) -> bool;
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Render(const hud_snapshot& snapshot) -> bool;
        auto HandleMouseMessage(uint32_t message, WPARAM wParam, LPARAM lParam, bool blocked) -> bool;

        auto HandlePacket(uint16_t id, const uint8_t* data, uint32_t size) -> void;
        auto HandleChat(chat_event event) -> void;
        auto HandleKey(WPARAM key, LPARAM flags, bool blocked) -> bool;
        auto ChatFocused(void) const -> bool;
        auto HandleCommand(const char* command) -> bool;
        auto ToggleEdit(void) -> bool;
        auto ExitEdit(void) -> void;
        auto SetVisible(bool visible) -> void;
        auto ReloadSettings(void) -> void;
        auto ResetSettings(void) -> void;

        auto IsLoaded(void) const -> bool;
        auto IsEditing(void) const -> bool;
        auto IsVisible(void) const -> bool;
        auto IsGpuShaderActive(void) const -> bool;
        auto NativeHudHiddenMask(void) const -> uint32_t;
        auto MenuLayout(void) const -> native_menu_layout;
        auto HasFaulted(void) const -> bool;
        auto GetBuildId(void) const -> std::string;
        auto GetPublishedPath(void) const -> std::string;

    private:
        enum class load_result : uint8_t
        {
            loaded,
            transient_failure,
            rejected,
        };

        struct file_stamp
        {
            uint64_t write_time{};
            uint64_t size{};

            auto operator==(const file_stamp& other) const -> bool = default;
        };

        mutable std::recursive_mutex mutex_{};
        IAshitaCore* core_{};
        IDirect3DDevice8* device_{};
        HMODULE module_{};
        runtime_api api_{};
        void* instance_{};
        std::string install_root_{};
        std::string published_path_{};
        std::string shadow_directory_{};
        std::string shadow_path_{};
        std::string runtime_build_id_{};
        file_stamp loaded_stamp_{};
        file_stamp rejected_stamp_{};
        file_stamp transient_stamp_{};
        uint64_t next_poll_time_{};
        uint64_t next_retry_time_{};
        uint32_t generation_{};
        uint32_t transient_failures_{};
        bool has_loaded_stamp_{};
        bool has_rejected_stamp_{};
        bool has_transient_stamp_{};
        bool runtime_faulted_{};
        std::deque<chat_event> chat_history_{};
        uint64_t next_chat_sequence_{};
        uint64_t last_native_serial_{};

        auto TryLoadPublished(bool initial, const file_stamp& stamp, std::string& message) -> load_result;
        auto ReadPublishedStamp(file_stamp& stamp) const -> bool;
        auto MakeShadowPath(void) -> std::string;
        static auto ValidateApi(const runtime_api& api) -> bool;
        auto RegisterLoadResult(load_result result, const file_stamp& stamp) -> void;
        static auto LastErrorText(const char* action) -> std::string;
    };
} // namespace xiui
