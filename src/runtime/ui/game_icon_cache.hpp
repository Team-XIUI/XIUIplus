#pragma once

#include "Ashita.h"
#include "design_system.hpp"
#include "icon_themes.hpp"
#include "pet_portraits.hpp"

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
        auto Status(uint32_t id) -> IDirect3DTexture8*;
        auto StatusIcon(uint32_t id, int preference = 0) -> game_icon_view;
        auto PhysicalDamageIcon(size_t index) const -> game_icon_view;
        auto MobDetectionIcon(mob_detection_icon icon) const -> game_icon_view;
        auto PetPortrait(std::string_view name, int kind, bool jobFallback = true) const -> game_icon_view;
        auto SetThemes(int statuses, int jobs) -> void { this->status_theme_ = StatusIconTheme(statuses); this->job_theme_ = JobIconTheme(jobs); }
        auto JobAtlas(void) const -> IDirect3DTexture8*
        { return this->job_theme_ > 0 && this->job_themes_[this->job_theme_ - 1] != nullptr ? this->job_themes_[this->job_theme_ - 1] : this->jobs_; }
        auto BlueMagicBanner(void) const -> IDirect3DTexture8* { return this->blue_magic_; }
        auto NotificationAtlas(void) const -> IDirect3DTexture8* { return this->notifications_; }
        auto RecastAtlas(void) const -> IDirect3DTexture8* { return this->recasts_; }
        auto ActionIcon(std::string_view key) const -> game_icon_view;
        auto MagicBurstElement(uint8_t element) const -> game_icon_view
        { return element < 8 ? game_icon_view{this->magic_burst_, {static_cast<float>(element % 4) / 4, static_cast<float>(element / 4) / 2, 0.25f, 0.5f}} : game_icon_view{}; }

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
        IDirect3DTexture8* actions_{};
        IDirect3DTexture8* magic_burst_{};
        IDirect3DTexture8* blue_magic_{};
        IDirect3DTexture8* pet_portraits_{};
        IDirect3DTexture8* physical_damage_{};
        IDirect3DTexture8* mob_detection_{};
        std::array<IDirect3DTexture8*, 3> status_themes_{}, job_themes_{};
        bool atlases_loaded_{};
        int status_theme_{}, job_theme_{};
        auto Icon(uint32_t id, bool status) -> IDirect3DTexture8*;
        auto LoadEmbeddedAtlas(uint16_t id, uint32_t width, uint32_t height) const -> IDirect3DTexture8*;
        auto Clear(void) -> void;
    };
} // namespace xiui::ui
