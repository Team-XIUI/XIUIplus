#pragma once

#include "runtime_api.hpp"
#include "native_chat_palette.hpp"

namespace xiui
{
    class runtime_host;

    class native_chat_source
    {
    public:
        ~native_chat_source(void);
        auto Initialize(runtime_host& runtime) -> bool;
        auto Shutdown(void) -> void;
        auto Palette(void) const -> chat_palette;
        auto Deliver(chat_event event) -> void;
        auto IsAvailable(void) const -> bool { return this->enabled_; }

    private:
        runtime_host* runtime_{};
        uintptr_t module_{};
        uint32_t module_size_{};
        uintptr_t window_hook_{};
        uintptr_t filter_hook_{};
        native_chat_palette palette_{};
        bool minhook_owned_{};
        bool enabled_{};
        auto Find(const char* pattern) const -> uintptr_t;
    };
}
