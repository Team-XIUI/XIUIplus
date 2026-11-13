#pragma once

#include "Ashita.h"
#include "design_system.hpp"
#include "icon_themes.hpp"
#include "pet_portraits.hpp"
#include "resources/custom_icon_library.hpp"

#include <unordered_map>

namespace xiui::ui
{
    enum class mob_detection_icon : uint16_t { passive, passive_nm, aggressive, aggressive_nm, links,
        sight, true_sight, sound, scent, magic, ability, low_hp, count };
    struct game_icon_view
    {
        IDirect3DTexture8* texture{};
        rect uv{0, 0, 1, 1};
    };

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
        // True while an item icon is waiting for a later frame's creation budget.
        auto ItemPending(uint32_t id) const -> bool { return id != 0 && this->resources_ != nullptr && this->device_ != nullptr && !this->items_.contains(id); }
        auto Status(uint32_t id) -> IDirect3DTexture8*;
        auto StatusIcon(uint32_t id, int preference = 0) -> game_icon_view;
        auto PhysicalDamageIcon(size_t index) const -> game_icon_view;
        auto MobDetectionIcon(mob_detection_icon icon) const -> game_icon_view;
        auto PetPortrait(std::string_view name, int kind, bool jobFallback = true) const -> game_icon_view;
        auto SetThemes(int statuses, int jobs) -> void { this->status_theme_ = StatusIconTheme(statuses); this->job_theme_ = JobIconTheme(jobs); }
        auto JobAtlas(int theme = -1) const -> IDirect3DTexture8*;
        auto BlueMagicBanner(void) const -> IDirect3DTexture8* { return this->Atlas(600, 900, 450); }
        auto NotificationAtlas(void) const -> IDirect3DTexture8* { return this->Atlas(203, 256, 64); }
        auto RecastAtlas(void) const -> IDirect3DTexture8*;
        auto ActionIcon(std::string_view key) -> game_icon_view;
        auto CustomIcons(void) -> resources::custom_icon_library& { return this->custom_icons_; }
        auto UpdateCustomIcons(void) -> void;
        auto MagicBurstElement(uint8_t element) const -> game_icon_view
        { return element < 8 ? game_icon_view{this->Atlas(1200, 1024, 512), {static_cast<float>(element % 4) / 4, static_cast<float>(element / 4) / 2, 0.25f, 0.5f}} : game_icon_view{}; }

    private:
        struct entry
        {
            IDirect3DTexture8* texture{};
            uint64_t last_use{};
        };

        IResourceManager* resources_{};
        IDirect3DDevice8* device_{};
        uint64_t access_{};
        uint64_t frame_access_{};
        uint32_t frame_created_{};
        std::unordered_map<uint32_t, entry> items_{};
        resources::custom_icon_library custom_icons_{};
        std::unordered_map<std::string, entry> custom_textures_{};
        uint64_t custom_generation_{};
        mutable std::unordered_map<uint16_t, IDirect3DTexture8*> atlases_{};
        int status_theme_{}, job_theme_{};
        auto Icon(uint32_t id, bool status) -> IDirect3DTexture8*;
        auto Atlas(uint16_t id, uint32_t width, uint32_t height) const -> IDirect3DTexture8*;
        auto LoadEmbeddedAtlas(uint16_t id, uint32_t width, uint32_t height) const -> IDirect3DTexture8*;
        auto Clear(void) -> void;
    };
} // namespace xiui::ui
