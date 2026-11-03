#pragma once

#include "design_system.hpp"
#include "resources/menu_texture.hpp"
#include "resources/menu_sprite.hpp"
#include "meter_skin.hpp"
#include "nine_slice.hpp"
#include "selection_style.hpp"

namespace xiui::ui
{
    enum class native_ui_image : size_t { button, compass, fire, earth, water, wind, ice, lightning, light, dark, directions, marker, auto_translate_green, auto_translate_red,
        seeking_party, gauge, gauge_fill, selection_frame, selection_arrow, count };

    class native_ui_assets
    {
    public:
        ~native_ui_assets(void) { this->Shutdown(); }
        auto Load(IResourceManager* resources) -> void;
        auto SetDevice(IDirect3DDevice8* device) -> void;
        auto Shutdown(void) -> void;
        auto Texture(native_ui_image image) const -> IDirect3DTexture8* { return this->textures_[static_cast<size_t>(image)]; }
        auto CompassAvailable(void) const -> bool { return this->compass_ && this->compass_->size() == 6 && this->Texture(native_ui_image::compass) != nullptr &&
            this->Texture(native_ui_image::directions) != nullptr && this->Texture(native_ui_image::marker) != nullptr; }
        auto DrawControl(const design_system& design, const rect& bounds, bool selected, std::optional<D3DCOLOR> tint = std::nullopt) const -> void;
        auto ControlPalette(void) const -> std::optional<std::array<D3DCOLOR, 2>>;
        auto MeterSkin(void) const -> const std::optional<meter_skin>& { return this->meter_skin_; }
        auto DrawAutoTranslateCap(const design_system& design, bool closing, float x, float y, float scale, float opacity) const -> bool;
        auto DrawSeekingParty(const design_system& design, const rect& bounds) const -> bool;
        auto DrawSelectionFrame(const design_system& design, const rect& bounds, D3DCOLOR color, float backgroundOpacity) const -> void;
        auto DrawSelectionArrow(const design_system& design, float tipX, float centerY, D3DCOLOR color) const -> void;
        auto DrawCompassPart(const design_system& design, size_t group, float x, float y, float scale, float rotation = 0, float flatten = 1,
            float opacity = 1, D3DCOLOR tint = 0xFFFFFFFF) const -> bool;

    private:
        std::array<std::optional<resources::menu_texture>, static_cast<size_t>(native_ui_image::count)> images_{};
        std::array<IDirect3DTexture8*, static_cast<size_t>(native_ui_image::count)> textures_{};
        IDirect3DDevice8* device_{};
        std::optional<resources::menu_sprite> compass_{};
        std::optional<resources::menu_sprite> buttons_{};
        std::array<std::optional<resources::menu_sprite>, 2> meters_{};
        std::optional<meter_skin> meter_skin_{};
        std::optional<D3DCOLOR> selection_color_{};
        std::array<std::optional<resources::menu_sprite>, 2> auto_translate_caps_{};
        std::optional<nine_slice_skin> selection_frame_{};
        std::optional<nine_slice_skin> selection_background_{};
        auto ReleaseTextures(void) -> void;
        auto ExtractMeterImages(void) -> void;
        auto CreateMeterSkin(void) const -> std::optional<meter_skin>;
    };
}
