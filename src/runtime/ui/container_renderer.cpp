#include "container_renderer.hpp"
#include "resources/menu_texture_scale.hpp"
#include "resources/embedded_resource.hpp"

#include <fstream>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui::ui
{
    namespace
    {
        auto Xiv1FrameScale(const rect& bounds, const container_style& style) -> float
        {
            const float requested = std::max(.01f, style.edge_size / 5);
            return std::min(requested, std::max(.01f, std::min(bounds.width, bounds.height) / 48));
        }
    }

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
        this->LoadAppearanceAssets();
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
        this->xiv1_window_ = {};
        this->xiv1_meter_.reset();
        if (this->xiv1_atlas_ != nullptr) this->xiv1_atlas_->Release();
        if (this->xiv1_fabric_ != nullptr) this->xiv1_fabric_->Release();
        this->xiv1_atlas_ = this->xiv1_fabric_ = nullptr;
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

    auto container_renderer::LoadAppearanceAssets(void) -> void
    {
        const auto bytes = resources::LoadEmbeddedResource(reinterpret_cast<HMODULE>(&__ImageBase), 850, 8 + 256 * 256 * 4);
        if (bytes.size() != 8 + 256 * 256 * 4) return;
        uint32_t width{}, height{};
        std::memcpy(&width, bytes.data(), 4); std::memcpy(&height, bytes.data() + 4, 4);
        if (width != 256 || height != 256) return;
        resources::menu_texture image{width, height, std::vector<uint32_t>(size_t{width} * height)};
        std::memcpy(image.pixels.data(), bytes.data() + 8, image.pixels.size() * 4);
        if (!this->CreateTexture(image, &this->xiv1_atlas_)) return;
        resources::menu_texture fabric{64, 64, std::vector<uint32_t>(64 * 64)};
        for (size_t y = 0; y < 64; ++y)
            std::copy_n(image.pixels.data() + y * 256 + 160, 64, fabric.pixels.data() + y * 64);
        if (!this->CreateTexture(fabric, &this->xiv1_fabric_)) return;

        auto& window = this->xiv1_window_;
        window.corner_size = 24;
        window.edge_size = 8;
        window.background_under_frame = true;
        constexpr std::array<rect, 9> regions{{{1, 1, 24, 24}, {1, 33, 64, 8}, {27, 1, 24, 24},
            {69, 33, 8, 64}, {0, 0, 64, 64}, {81, 33, 8, 64},
            {53, 1, 24, 24}, {1, 45, 64, 8}, {79, 1, 24, 24}}};
        for (size_t i = 0; i < regions.size(); ++i)
        {
            auto& tile = window.tiles[i];
            tile.texture = i == 4 ? this->xiv1_fabric_ : this->xiv1_atlas_;
            tile.source = regions[i];
            tile.texture_width = tile.texture_height = i == 4 ? 64.0f : 256.0f;
            tile.style.repeat_x = tile.style.repeat_y = i == 4;
        }
        const auto texture = [&](const rect& source) {
            return meter_texture{this->xiv1_atlas_, {source.x / 256, source.y / 256, source.width / 256, source.height / 256}, {}, 256, 256};
        };
        meter_skin meter{};
        meter.xiv1 = true;
        meter.cap_ratio = 9.0f / 14;
        meter.cap_gap = 0;
        meter.fill = texture({96, 72, 64, 8});
        meter.background = texture({96, 88, 64, 8});
        meter.rim = texture({0, 112, 64, 14});
        meter.left_cap = texture({72, 112, 9, 14});
        meter.right_cap = texture({88, 112, 9, 14});
        this->xiv1_meter_ = meter;
    }

    auto container_renderer::DrawXiv1TargetHeader(const design_system& design, const rect& bounds, const float opacity) const -> void
    {
        const auto alpha = static_cast<uint32_t>(255 * std::clamp(opacity, 0.0f, 1.0f));
        const auto color = (alpha << 24) | 0xFFFFFF;
        const float curveWidth = bounds.height * 45.0f / 7;
        const rect curve{bounds.x - curveWidth, bounds.y, curveWidth, bounds.height};
        design.DrawTexture(this->xiv1_atlas_, curve, color, {0, 155.0f / 256, 45.0f / 256, -7.0f / 256});
        design.DrawTexture(this->xiv1_atlas_, bounds, color, {45.0f / 256, 155.0f / 256, 140.0f / 256, -7.0f / 256});
    }

    auto container_renderer::DrawXiv1ActionMedallion(const design_system& design, const rect& bounds, const float opacity) const -> void
    {
        const auto alpha = static_cast<uint32_t>(255 * std::clamp(opacity, 0.0f, 1.0f));
        design.DrawTexture(this->xiv1_atlas_, bounds, (alpha << 24) | 0xFFFFFF, {192.0f / 256, 144.0f / 256, 59.0f / 256, 60.0f / 256});
    }

    auto container_renderer::DrawXiv1TitleSeparator(const design_system& design, const rect& bounds, const float opacity) const -> void
    {
        const auto alpha = static_cast<uint32_t>(255 * std::clamp(opacity, 0.0f, 1.0f));
        design.DrawTexture(this->xiv1_atlas_, bounds, (alpha << 24) | 0xFFFFFF, {1.0f / 256, 33.0f / 256, 64.0f / 256, 8.0f / 256});
    }

    auto container_renderer::MeterBackground(void) const -> meter_texture
    {
        if (this->appearance_theme_ == 1 && this->xiv1_meter_) return this->xiv1_meter_->background;
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
        if (this->appearance_theme_ == 1 && this->xiv1_fabric_ != nullptr)
            return this->Draw(design, bounds, style);
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
        if (this->appearance_theme_ == 1 && this->xiv1_fabric_ != nullptr)
        {
            const float scale = Xiv1FrameScale(bounds, style);
            const float inset = std::min({8 * scale, bounds.width * .5f, bounds.height * .5f});
            const auto opacity = this->BackgroundOpacity(style);
            const auto alpha = static_cast<uint32_t>(255 * opacity);
            const auto backing = static_cast<uint32_t>(255 * opacity * design.DarkBackingOpacity());
            if (fade != nullptr)
            {
                const auto left = fade->At(bounds.x), right = fade->At(bounds.x + bounds.width);
                design.GradientRect(bounds, static_cast<uint32_t>(backing * left) << 24, static_cast<uint32_t>(backing * right) << 24);
                design.GradientRect(bounds, (static_cast<uint32_t>(alpha * left) << 24) | 0x0B102B, (static_cast<uint32_t>(alpha * right) << 24) | 0x0B102B);
            }
            else
            {
                design.FillRoundedRect(bounds, backing << 24, std::min(12 * scale, bounds.height * .5f));
                design.FillRoundedRect(bounds, (alpha << 24) | 0x0B102B, std::min(12 * scale, bounds.height * .5f));
            }
            const rect interior{bounds.x + inset, bounds.y + inset, std::max(0.0f, bounds.width - inset * 2), std::max(0.0f, bounds.height - inset * 2)};
            DrawNineSliceTile(design, this->xiv1_window_.tiles[4], interior, 1, opacity, fade);
            for (const auto& fill : fills)
            {
                const float left = std::max(interior.x, fill.bounds.x), top = std::max(interior.y, fill.bounds.y);
                const float right = std::min(interior.x + interior.width, fill.bounds.x + fill.bounds.width);
                const float bottom = std::min(interior.y + interior.height, fill.bounds.y + fill.bounds.height);
                const auto color = (fill.color & 0xFFFFFF) | (static_cast<uint32_t>((fill.color >> 24) * opacity) << 24);
                if (right <= left || bottom <= top) continue;
                if (fade != nullptr) design.GradientRect({left, top, right - left, bottom - top},
                    (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * fade->At(left)) << 24),
                    (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * fade->At(right)) << 24));
                else design.FillRect({left, top, right - left, bottom - top}, color);
            }
            this->DrawFrame(design, bounds, style, fade);
            return ContentBounds(bounds, style);
        }
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
        if (this->appearance_theme_ == 1 && this->xiv1_atlas_ != nullptr)
        {
            if (style.border) DrawNineSlice(design, this->xiv1_window_, bounds, Xiv1FrameScale(bounds, style), style.opacity, false, fade);
            return;
        }
        const auto& window = this->windows_[this->selected_];
        if (!window.source || !style.border) return;
        const auto& skin = window.variants[style.scan_lines ? 1 : 0];
        const float scale = style.edge_size / window.source->patches[1].height;
        DrawNineSlice(design, skin, bounds, scale, style.opacity, false, fade);
    }
} // namespace xiui::ui
