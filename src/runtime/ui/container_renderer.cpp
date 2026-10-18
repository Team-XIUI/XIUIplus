#include "container_renderer.hpp"
#include "resources/menu_texture_scale.hpp"

#include <fstream>

namespace xiui::ui
{
    container_renderer::~container_renderer(void) { this->Shutdown(); }

    auto container_renderer::CorrectWindowFrame(resources::menu_window& window, const uint8_t type) -> void
    {
        auto& right = window.patches[5];
        const auto& left = window.patches[3];
        // Match the stock Type 7 edge intensity without changing its artwork.
        if (type == 6 && right.additive && !left.additive &&
            std::all_of(right.colors.begin(), right.colors.end(), [](const auto color) { return color == 0xC0808080; }) &&
            std::all_of(left.colors.begin(), left.colors.end(), [](const auto color) { return color == 0x91616161; }))
        {
            right.colors = left.colors;
            right.additive = left.additive;
        }
    }

    auto container_renderer::LoadSkin(IResourceManager* resources) -> void
    {
        this->Shutdown();
        if (resources == nullptr)
            return;
        for (size_t type = 0; type < this->windows_.size(); ++type)
        {
            char path[2048]{};
            resources->GetFilePath(static_cast<uint32_t>(14 + type), path, sizeof(path));
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file) continue;
            const auto size = file.tellg();
            if (size <= 0 || size > 1024 * 1024) continue;
            std::vector<uint8_t> bytes(static_cast<size_t>(size));
            file.seekg(0);
            if (!file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) continue;
            auto& window = this->windows_[type];
            window.source = resources::DecodeMenuWindow(bytes);
            if (!window.source) continue;
            CorrectWindowFrame(*window.source, static_cast<uint8_t>(type));
            std::array<uint64_t, 3> sum{};
            const auto& background = window.source->textures[0];
            window.meter_background = resources::UpscaleMenuTexture(background, 4, true);
            for (const auto pixel : background.pixels)
                for (unsigned channel = 0; channel < 3; ++channel)
                    sum[channel] += (pixel >> (channel * 8)) & 255;
            window.average = 0xFF000000;
            for (unsigned channel = 0; channel < 3; ++channel)
                window.average |= static_cast<uint32_t>(sum[channel] / background.pixels.size()) << (channel * 8);
        }
    }

    auto container_renderer::SetDevice(IDirect3DDevice8* device) -> bool
    {
        if (device == this->device_) return true;
        this->ReleaseTextures();
        this->device_ = device;
        if (device == nullptr) return true;
        for (auto& window : this->windows_)
        {
            if (!window.source) continue;
            const auto& source = *window.source;
            for (size_t i = 0; i < source.textures.size(); ++i)
                if (!this->CreateTexture(source.textures[i], &window.textures[i]))
                {
                    this->ReleaseTextures();
                    return false;
                }
            if (!this->CreateTexture({1, 1, {window.average}}, &window.textures[4]))
            {
                this->ReleaseTextures();
                return false;
            }
            if (window.meter_background) this->CreateTexture(*window.meter_background, &window.textures[5]);
            for (size_t variant = 0; variant < window.variants.size(); ++variant)
            {
                auto& skin = window.variants[variant];
                skin.corner_size = static_cast<float>(source.patches[0].width);
                skin.edge_size = static_cast<float>(source.patches[1].height);
                skin.background_under_frame = true;
                for (size_t i = 0; i < skin.tiles.size(); ++i)
                {
                    const auto& patch = source.patches[i];
                    const auto& image = source.textures[patch.texture];
                    auto& tile = skin.tiles[i];
                    tile.texture = window.textures[variant == 0 && i == 4 ? 4 : patch.texture];
                    tile.texture_width = static_cast<float>(image.width);
                    tile.texture_height = static_cast<float>(image.height);
                    tile.source = {static_cast<float>(patch.u), static_cast<float>(patch.v),
                        static_cast<float>(patch.width), static_cast<float>(patch.height)};
                    std::copy(patch.colors.begin(), patch.colors.end(), tile.style.colors.begin());
                    tile.style.repeat_x = i % 3 == 1;
                    tile.style.repeat_y = i / 3 == 1;
                    tile.style.additive = patch.additive;
                    tile.style.double_modulation = true;
                }
            }
        }
        return true;
    }

    auto container_renderer::ReleaseTextures(void) -> void
    {
        for (auto& window : this->windows_)
        {
            window.variants = {};
            for (auto*& texture : window.textures)
            {
                if (texture != nullptr) texture->Release();
                texture = nullptr;
            }
        }
        this->device_ = nullptr;
    }

    auto container_renderer::MeterBackground(void) const -> meter_texture
    {
        const auto& window = this->windows_[this->selected_];
        const auto& tile = window.variants[1].tiles[4];
        return {window.textures[5] != nullptr ? window.textures[5] : tile.texture, {tile.source.x / tile.texture_width, tile.source.y / tile.texture_height,
            tile.source.width / tile.texture_width, tile.source.height / tile.texture_height},
            tile.style, tile.texture_width, tile.texture_height};
    }

    auto container_renderer::Shutdown(void) -> void
    {
        this->ReleaseTextures();
        for (auto& window : this->windows_) { window.source.reset(); window.meter_background.reset(); }
        this->selected_ = 0;
    }

    auto container_renderer::CreateTexture(const resources::menu_texture& image, IDirect3DTexture8** texture) const -> bool
    {
        IDirect3DTexture8* created{};
        if (FAILED(this->device_->CreateTexture(image.width, image.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &created)))
            return false;
        D3DLOCKED_RECT locked{};
        if (FAILED(created->LockRect(0, &locked, nullptr, 0))) { created->Release(); return false; }
        for (uint32_t y = 0; y < image.height; ++y)
            std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
                image.pixels.data() + y * image.width, image.width * sizeof(uint32_t));
        if (FAILED(created->UnlockRect(0))) { created->Release(); return false; }
        *texture = created;
        return true;
    }

    auto container_renderer::ContentBounds(const rect& bounds, const container_style& style) -> rect
    {
        const float edge = std::max(0.0f, style.edge_size);
        const float xInset = std::min(edge + std::max(0.0f, style.padding), std::max(0.0f, bounds.width) * 0.5f);
        const float yInset = std::min(edge + std::max(0.0f, style.padding), std::max(0.0f, bounds.height) * 0.5f);
        return {bounds.x + xInset, bounds.y + yInset, std::max(0.0f, bounds.width - xInset * 2.0f), std::max(0.0f, bounds.height - yInset * 2.0f)};
    }

    auto container_renderer::DrawTooltip(const design_system& design, const rect& bounds) const -> rect
    {
        auto style = TooltipStyle();
        const auto& window = this->windows_[this->selected_];
        if (window.source)
        {
            const auto& skin = window.variants[0];
            const float available = std::max(0.0f, std::min(bounds.width, bounds.height)) * 0.5f;
            const float inset = style.edge_size + style.padding;
            // Scale the entire frame together so short tooltips retain matching corners and edges.
            style.edge_size = std::min(style.edge_size, available * skin.edge_size / std::max(1.0f, skin.corner_size));
            style.padding = inset - style.edge_size;
        }
        design.FillRect(bounds, 0xFF000000);
        const std::array fills{container_fill{bounds, 0xA0000000}};
        return this->Draw(design, bounds, style, fills);
    }

    auto container_renderer::Draw(const design_system& design, const rect& bounds, const container_style& style, const std::span<const container_fill> fills, const horizontal_fade* fade) const -> rect
    {
        const auto& window = this->windows_[this->selected_];
        if (window.source)
        {
            const float inset = std::max(1.0f, style.edge_size);
            const rect interior{bounds.x + inset, bounds.y + inset, std::max(0.0f, bounds.width - inset * 2), std::max(0.0f, bounds.height - inset * 2)};
            const auto backing = static_cast<uint32_t>(255 * std::clamp(style.opacity, 0.0f, 1.0f) * design.DarkBackingOpacity());
            if (fade != nullptr) design.GradientRect(bounds, static_cast<uint32_t>(backing * fade->At(bounds.x)) << 24,
                static_cast<uint32_t>(backing * fade->At(bounds.x + bounds.width)) << 24);
            else design.FillRect(bounds, backing << 24);
            const auto& skin = window.variants[style.scan_lines ? 1 : 0];
            const float scale = style.edge_size / window.source->patches[1].height;
            DrawNineSliceTile(design, skin.tiles[4], bounds, scale, style.opacity, fade);
            for (const auto& fill : fills)
            {
                const float left = std::max(interior.x, fill.bounds.x), top = std::max(interior.y, fill.bounds.y);
                const float right = std::min(interior.x + interior.width, fill.bounds.x + fill.bounds.width);
                const float bottom = std::min(interior.y + interior.height, fill.bounds.y + fill.bounds.height);
                const auto color = (fill.color & 0xFFFFFF) | (static_cast<uint32_t>((fill.color >> 24) * std::clamp(style.opacity, 0.0f, 1.0f)) << 24);
                if (right > left && bottom > top)
                {
                    if (fade != nullptr) design.GradientRect({left, top, right - left, bottom - top},
                        (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * fade->At(left)) << 24),
                        (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * fade->At(right)) << 24));
                    else design.FillRect({left, top, right - left, bottom - top}, color);
                }
            }
            this->DrawFrame(design, bounds, style, fade);
        }
        return ContentBounds(bounds, style);
    }

    auto container_renderer::DrawFrame(const design_system& design, const rect& bounds, const container_style& style, const horizontal_fade* fade) const -> void
    {
        const auto& window = this->windows_[this->selected_];
        if (!window.source || !style.border) return;
        const auto& skin = window.variants[style.scan_lines ? 1 : 0];
        const float scale = style.edge_size / window.source->patches[1].height;
        DrawNineSlice(design, skin, bounds, scale, style.opacity, false, fade);
    }
} // namespace xiui::ui
