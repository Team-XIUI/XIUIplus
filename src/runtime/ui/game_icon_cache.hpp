#pragma once

#include "Ashita.h"

#include <unordered_map>

namespace xiui::ui
{
    class game_icon_cache
    {
    public:
        game_icon_cache(void) = default;
        ~game_icon_cache(void);
        game_icon_cache(const game_icon_cache&) = delete;
        auto operator=(const game_icon_cache&) -> game_icon_cache& = delete;

        auto SetResources(IResourceManager* resources) -> void;
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        auto Item(uint32_t id) -> IDirect3DTexture8*;
        auto Status(uint32_t id) -> IDirect3DTexture8*;
        auto JobAtlas(void) const -> IDirect3DTexture8* { return this->jobs_; }
        auto NotificationAtlas(void) const -> IDirect3DTexture8* { return this->notifications_; }
        auto RecastAtlas(void) const -> IDirect3DTexture8* { return this->recasts_; }

    private:
        struct entry
        {
            IDirect3DTexture8* texture{};
            uint64_t last_use{};
        };

        IResourceManager* resources_{};
        IDirect3DDevice8* device_{};
        uint64_t access_{};
        std::unordered_map<uint32_t, entry> items_{};
        IDirect3DTexture8* jobs_{};
        IDirect3DTexture8* notifications_{};
        IDirect3DTexture8* recasts_{};
        auto Icon(uint32_t id, bool status) -> IDirect3DTexture8*;
        auto LoadEmbeddedAtlas(uint16_t id, uint32_t width, uint32_t height) const -> IDirect3DTexture8*;
        auto Clear(void) -> void;
    };
} // namespace xiui::ui
