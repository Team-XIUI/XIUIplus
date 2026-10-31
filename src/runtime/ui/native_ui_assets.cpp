#include "native_ui_assets.hpp"
#include "resources/menu_texture_scale.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <string_view>

namespace xiui::ui
{
    namespace
    {
        constexpr std::array<std::string_view, static_cast<size_t>(native_ui_image::gauge) + 1> image_names{
            "buttonto", "compass ", "elfire  ", "elearth ", "elwater ", "elwind  ", "elice   ", "elthunde", "elholy  ", "eldark  ", "news    ", "marker  ", "markgree", "markred ", "gauge   "};

        auto CreateSelectionImage(void) -> resources::menu_texture
        {
            // Four source pixels per screen pixel keep the rounded trim smooth.
            resources::menu_texture image{64, 128, std::vector<uint32_t>(64 * 128)};
            for (uint32_t y = 0; y < 64; ++y)
            for (uint32_t x = 0; x < image.width; ++x)
            {
                const float qx = std::abs((x + 0.5f) / 4 - 8) - 3;
                const float qy = std::abs((y + 0.5f) / 4 - 8) - 3;
                const float distance = std::hypot(std::max(0.0f, qx), std::max(0.0f, qy)) + std::min(0.0f, std::max(qx, qy)) - 4;
                const float outer = std::clamp(0.5f - distance * 4, 0.0f, 1.0f);
                const float inner = std::clamp(0.5f - (distance + 1.25f) * 4, 0.0f, 1.0f);
                const float halo = (1 - outer) * std::clamp(1 - distance, 0.0f, 1.0f) * 0.16f;
                const float alpha = (outer - inner) * 0.95f + halo;
                image.pixels[y * image.width + x] = (static_cast<uint32_t>(std::lround(alpha * 255)) << 24) | 0xFFFFFF;
                image.pixels[(y + 64) * image.width + x] = (static_cast<uint32_t>(std::lround(inner * 255)) << 24) | 0xFFFFFF;
            }
            return image;
        }

        auto CreateSelectionArrowImage(void) -> resources::menu_texture
        {
            resources::menu_texture image{64, 64, std::vector<uint32_t>(64 * 64)};
            const auto segmentDistance = [](const point p, const point start, const point end) {
                const float dx = end.x - start.x, dy = end.y - start.y;
                const float t = std::clamp(((p.x - start.x) * dx + (p.y - start.y) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
                return std::hypot(p.x - start.x - dx * t, p.y - start.y - dy * t);
            };
            for (uint32_t y = 0; y < image.height; ++y)
            for (uint32_t x = 0; x < image.width; ++x)
            {
                const point p{(x + 0.5f) / 4, (y + 0.5f) / 4};
                const float distance = std::min(segmentDistance(p, {4, 3}, {10, 8}), segmentDistance(p, {10, 8}, {4, 13})) - 1.05f;
                const float alpha = std::clamp(0.5f - distance * 4, 0.0f, 1.0f);
                image.pixels[y * image.width + x] = (static_cast<uint32_t>(std::lround(alpha * 255)) << 24) | 0xFFFFFF;
            }
            return image;
        }
    }
    auto native_ui_assets::Load(IResourceManager* resources) -> void
    {
        this->Shutdown();
        this->images_[static_cast<size_t>(native_ui_image::selection_frame)] = CreateSelectionImage();
        this->images_[static_cast<size_t>(native_ui_image::selection_arrow)] = CreateSelectionArrowImage();
        if (resources == nullptr) return;
        char path[2048]{};
        resources->GetFilePath(1, path, sizeof(path));
        std::ifstream file{path, std::ios::binary | std::ios::ate};
        if (!file) return;
        const auto size = file.tellg();
        if (size <= 0 || size > 16 * 1024 * 1024) return;
        std::vector<uint8_t> data(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()))) return;
        for (size_t offset = 0; offset + 16 <= data.size();)
        {
            uint32_t meta{}; std::memcpy(&meta, data.data() + offset + 4, 4);
            const size_t sectionSize = ((meta >> 7) & 0xFFFFF) * size_t{16};
            if (sectionSize < 16 || sectionSize > data.size() - offset) break;
            if ((meta & 0x7F) == 0x20 && sectionSize >= 16 + 0x45)
            {
                const std::string_view name{reinterpret_cast<const char*>(data.data() + offset + 25), 8};
                const auto found = std::find(image_names.begin(), image_names.end(), name);
                if (found != image_names.end()) this->images_[found - image_names.begin()] = resources::DecodeMenuTexture(std::span{data}.subspan(offset + 16, sectionSize - 16));
            }
            else if ((meta & 0x7F) == 0x31 && sectionSize >= 35)
            {
                const std::string_view name{reinterpret_cast<const char*>(data.data() + offset + 16), 16};
                const auto payload = std::span{data}.subspan(offset + 16, sectionSize - 16);
                if (name == "menu    compass ") this->compass_ = resources::DecodeMenuSprite(payload);
                else if (name == "menu    windowps")
                {
                    this->buttons_ = resources::DecodeMenuSprite(payload, 13);
                    if (this->buttons_)
                        std::erase_if(this->buttons_->front(), [](const auto& quad) { return quad.texture != "buttonto"; });
                    this->meters_[0] = resources::DecodeMenuSprite(payload, 38);
                    this->meters_[1] = resources::DecodeMenuSprite(payload, 97);
                }
                else if (name == "font    usgaiji ")
                    for (size_t i = 0; i < this->auto_translate_caps_.size(); ++i)
                        this->auto_translate_caps_[i] = resources::DecodeMenuSprite(payload, 8 + i);
                else if (name == "menu    ncol    ")
                {
                    const auto palette = resources::DecodeMenuSprite(payload, 8);
                    if (palette && !palette->front().empty()) this->selection_color_ = palette->front().front().colors.front();
                }
            }
            offset += sectionSize;
        }
        this->ExtractMeterImages();
    }

    auto native_ui_assets::ExtractMeterImages(void) -> void
    {
        const auto& image = this->images_[static_cast<size_t>(native_ui_image::gauge)];
        if (!image || !this->meters_[0] || !this->meters_[1] ||
            this->meters_[0]->front().size() != 3 || this->meters_[1]->front().size() != 1) return;
        const auto extract = [&](const resources::menu_sprite_quad& quad, const native_ui_image id) {
            if (quad.texture != "gauge   ") return;
            auto region = resources::ExtractMenuTexture(*image, quad.u, quad.v, quad.width, quad.height);
            if (!region) return;
            auto scaled = resources::UpscaleMenuTexture(*region, 4);
            this->images_[static_cast<size_t>(id)] = scaled ? std::move(scaled) : std::move(region);
        };
        extract(this->meters_[1]->front().front(), native_ui_image::gauge_fill);
    }

    auto native_ui_assets::ReleaseTextures(void) -> void
    {
        this->meter_skin_.reset();
        this->selection_frame_.reset();
        this->selection_background_.reset();
        for (auto*& texture : this->textures_) { if (texture != nullptr) texture->Release(); texture = nullptr; }
        this->device_ = nullptr;
    }

    auto native_ui_assets::Shutdown(void) -> void
    {
        this->ReleaseTextures();
        for (auto& image : this->images_) image.reset();
        this->compass_.reset();
        this->buttons_.reset();
        this->meters_ = {};
        this->selection_color_.reset();
        this->auto_translate_caps_ = {};
    }

    auto native_ui_assets::SetDevice(IDirect3DDevice8* device) -> void
    {
        if (this->device_ == device) return;
        this->ReleaseTextures();
        this->device_ = device;
        if (device == nullptr) return;
        for (size_t index = 0; index < this->images_.size(); ++index)
        {
            if (!this->images_[index] || index == static_cast<size_t>(native_ui_image::gauge)) continue;
            const auto& image = *this->images_[index];
            auto*& texture = this->textures_[index];
            if (FAILED(device->CreateTexture(image.width, image.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture))) continue;
            D3DLOCKED_RECT locked{};
            if (FAILED(texture->LockRect(0, &locked, nullptr, 0))) { texture->Release(); texture = nullptr; continue; }
            for (uint32_t y = 0; y < image.height; ++y)
                std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch, image.pixels.data() + y * image.width, image.width * sizeof(uint32_t));
            texture->UnlockRect(0);
        }
        this->meter_skin_ = this->CreateMeterSkin();
        if (auto* texture = this->Texture(native_ui_image::selection_frame))
        {
            nine_slice_skin skin{};
            skin.corner_size = skin.edge_size = 6;
            constexpr float positions[]{0, 24, 40}, sizes[]{24, 16, 24};
            for (size_t row = 0; row < 3; ++row)
            for (size_t column = 0; column < 3; ++column)
                skin.tiles[row * 3 + column] = {texture, {positions[column], positions[row], sizes[column], sizes[row]}, 64, 128};
            this->selection_frame_ = skin;
            for (auto& tile : skin.tiles) tile.source.y += 64;
            this->selection_background_ = skin;
        }
    }

    auto native_ui_assets::CreateMeterSkin(void) const -> std::optional<meter_skin>
    {
        if (!this->meters_[0] || !this->meters_[1] ||
            this->meters_[0]->front().size() != 3 || this->meters_[1]->front().size() != 1) return std::nullopt;
        constexpr auto fillImage = native_ui_image::gauge_fill;
        if (this->Texture(fillImage) == nullptr || !this->images_[static_cast<size_t>(fillImage)]) return std::nullopt;
        const auto patch = [&](const resources::menu_sprite_quad& quad, const native_ui_image id) -> meter_texture {
            const auto& image = this->images_[static_cast<size_t>(id)];
            const float sourceWidth = static_cast<float>(std::bit_ceil(uint32_t{quad.width} + 2));
            const float sourceHeight = static_cast<float>(std::bit_ceil(uint32_t{quad.height} + 2));
            meter_texture result{this->Texture(id), {1.0f / sourceWidth, 1.0f / sourceHeight,
                static_cast<float>(quad.width) / sourceWidth, static_cast<float>(quad.height) / sourceHeight}, {},
                static_cast<float>(image->width), static_cast<float>(image->height)};
            if ((quad.uv_flags & 1) != 0) { result.uv.x += result.uv.width; result.uv.width = -result.uv.width; }
            if ((quad.uv_flags & 2) != 0) { result.uv.y += result.uv.height; result.uv.height = -result.uv.height; }
            std::copy(quad.colors.begin(), quad.colors.end(), result.style.colors.begin());
            result.style.additive = quad.additive;
            result.style.double_modulation = true;
            return result;
        };
        const auto& frame = this->meters_[0]->front();
        const auto& fill = this->meters_[1]->front().front();
        for (const auto& quad : frame) if (quad.texture != "gauge   ") return std::nullopt;
        if (fill.texture != "gauge   ") return std::nullopt;
        meter_skin result{patch(fill, fillImage)};
        const float height = std::max(1.0f, static_cast<float>(frame[0].points[5] - frame[0].points[1]));
        result.cap_ratio = static_cast<float>(frame[0].points[2] - frame[0].points[0]) / height;
        return result;
    }

    auto native_ui_assets::ControlPalette(void) const -> std::optional<std::array<D3DCOLOR, 2>>
    {
        if (!this->buttons_ || this->buttons_->front().size() != 2 || !this->selection_color_) return std::nullopt;
        const auto expand = [](const D3DCOLOR native) {
            D3DCOLOR color = 0xFF000000;
            for (unsigned shift = 0; shift < 24; shift += 8)
                color |= std::min<D3DCOLOR>(255, ((native >> shift) & 255) * 2) << shift;
            return color;
        };
        return std::array{expand(this->buttons_->front().front().colors.front()), expand(*this->selection_color_)};
    }

    auto native_ui_assets::DrawControl(const design_system& design, const rect& bounds, const bool selected, const std::optional<D3DCOLOR> overrideTint) const -> void
    {
        const auto& colors = design.GetTheme().colors;
        auto* texture = this->Texture(native_ui_image::button);
        const uint32_t tint = overrideTint.value_or(selected ? colors.control_selected : colors.control);
        if (texture == nullptr || !this->buttons_ || this->buttons_->front().size() != 2)
        { design.FillRect(bounds, tint); design.BorderRect(bounds, colors.border); return; }
        const auto& image = *this->images_[static_cast<size_t>(native_ui_image::button)];
        const auto reference = this->buttons_->front().front().colors.front();
        const float height = std::min(24.0f, bounds.height);
        const float top = bounds.y + (bounds.height - height) * 0.5f;
        const float cap = std::min(height * 0.5f, bounds.width * 0.5f);
        const auto position = [&](const float x) {
            if (x <= 8) return bounds.x + x * cap / 8;
            if (x >= 80) return bounds.x + bounds.width - (88 - x) * cap / 8;
            return bounds.x + cap + (x - 8) * (bounds.width - cap * 2) / 72;
        };
        std::array<texture_quad, 12> quads{};
        size_t used{};
        for (int layer = 0; layer < 2; ++layer)
        for (const auto& patch : this->buttons_->front())
        {
            texture_style style{};
            style.double_modulation = true;
            style.additive = layer == 1 && patch.additive;
            for (size_t i = 0; i < style.colors.size(); ++i)
            {
                const auto native = patch.colors[i];
                uint32_t color = native & 0xFF000000;
                if (layer != 0)
                    for (unsigned shift = 0; shift < 24; shift += 8)
                    {
                        const auto base = std::max(1u, ((reference >> shift) & 255) * 2);
                        const auto channel = ((native >> shift) & 255) * ((tint >> shift) & 255) / base;
                        // Keep the warm face dark enough for its highlighted label.
                        color |= std::min(255u, selected ? channel * 3 / 5 : channel) << shift;
                    }
                style.colors[i] = color;
            }
            // Keep the eight-pixel curved ends proportional; stretch only the middle.
            const float start = static_cast<float>(patch.points[0]), end = static_cast<float>(patch.points[2]);
            for (float left = start; left < end;)
            {
                const float right = std::min(end, left < 8 ? 8.0f : left < 80 ? 80.0f : end);
                const float u = patch.u + (left - start) / (end - start) * patch.width;
                const std::array<point, 4> points{{{position(left), top}, {position(right), top}, {position(left), top + height}, {position(right), top + height}}};
                quads[used++] = {points, {u / image.width, static_cast<float>(patch.v) / image.height,
                    (right - left) / (end - start) * patch.width / image.width, static_cast<float>(patch.height) / image.height}, style};
                left = right;
            }
        }
        design.DrawTextureQuads(texture, std::span{quads}.first(used));
    }

    auto native_ui_assets::DrawAutoTranslateCap(const design_system& design, const bool closing, const float x, const float y, const float scale, const float opacity) const -> bool
    {
        const auto& definition = this->auto_translate_caps_[closing ? 1 : 0];
        const auto imageIndex = static_cast<size_t>(closing ? native_ui_image::auto_translate_red : native_ui_image::auto_translate_green);
        if (!definition || definition->empty() || !this->images_[imageIndex] || this->textures_[imageIndex] == nullptr) return false;
        const auto& image = *this->images_[imageIndex];
        for (const auto& patch : definition->front())
        {
            if (size_t{patch.u} + patch.width > image.width || size_t{patch.v} + patch.height > image.height) return false;
            std::array<point, 4> points{};
            texture_style style{};
            style.double_modulation = true; style.additive = patch.additive;
            for (size_t i = 0; i < points.size(); ++i)
            {
                points[i] = {x + patch.points[i * 2] * scale, y + patch.points[i * 2 + 1] * scale};
                style.colors[i] = (patch.colors[i] & 0xFFFFFF) | (static_cast<uint32_t>((patch.colors[i] >> 24) * std::clamp(opacity, 0.0f, 1.0f)) << 24);
            }
            design.DrawTextureQuad(this->textures_[imageIndex], points, {static_cast<float>(patch.u) / image.width, static_cast<float>(patch.v) / image.height,
                static_cast<float>(patch.width) / image.width, static_cast<float>(patch.height) / image.height}, style);
        }
        return true;
    }

    auto native_ui_assets::DrawSelectionFrame(const design_system& design, const rect& bounds, const D3DCOLOR color, const float backgroundOpacity) const -> void
    {
        if (!this->selection_frame_ || !this->selection_background_) return;
        if (backgroundOpacity > 0)
        {
            auto background = *this->selection_background_;
            for (auto& tile : background.tiles) tile.style.colors.fill(color);
            DrawNineSlice(design, background, bounds, 1, backgroundOpacity);
        }
        auto skin = *this->selection_frame_;
        for (auto& tile : skin.tiles) tile.style.colors.fill(color);
        DrawNineSlice(design, skin, bounds, 1, 1, false);
    }

    auto native_ui_assets::DrawSelectionArrow(const design_system& design, const float tipX, const float centerY, const D3DCOLOR color) const -> void
    {
        auto* texture = this->Texture(native_ui_image::selection_arrow);
        if (texture == nullptr) return;
        design.DrawTexture(texture, {tipX - 12, centerY - 7, 16, 16}, 0xA0000000);
        design.DrawTexture(texture, {tipX - 12, centerY - 8, 16, 16}, color);
    }

    auto native_ui_assets::DrawCompassPart(const design_system& design, const size_t group, const float x, const float y, const float scale,
        const float rotation, const float flatten, const float opacity, const D3DCOLOR tint) const -> bool
    {
        if (!this->compass_ || this->compass_->size() != 6 || group >= this->compass_->size()) return false;
        const auto& patches = (*this->compass_)[group];
        for (const auto& patch : patches)
        {
            const auto found = std::find(image_names.begin(), image_names.end(), patch.texture);
            if (found == image_names.end()) return false;
            const size_t index = found - image_names.begin();
            if (!this->images_[index] || this->textures_[index] == nullptr) return false;
            const auto& image = *this->images_[index];
            if (size_t{patch.u} + patch.width > image.width || size_t{patch.v} + patch.height > image.height) return false;
        }
        for (const auto& patch : patches)
        {
            const size_t index = std::find(image_names.begin(), image_names.end(), patch.texture) - image_names.begin();
            const auto& image = *this->images_[index];
            std::array<point, 4> points{};
            const float cosine = std::cos(rotation), sine = std::sin(rotation);
            for (size_t vertex = 0; vertex < points.size(); ++vertex)
            {
                const float px = patch.points[vertex * 2] * scale, py = patch.points[vertex * 2 + 1] * scale;
                points[vertex] = {x + px * cosine - py * sine, y + (px * sine + py * cosine) * flatten};
            }
            rect uv{static_cast<float>(patch.u) / image.width, static_cast<float>(patch.v) / image.height,
                static_cast<float>(patch.width) / image.width, static_cast<float>(patch.height) / image.height};
            if ((patch.uv_flags & 1) != 0) { uv.x += uv.width; uv.width = -uv.width; }
            if ((patch.uv_flags & 2) != 0) { uv.y += uv.height; uv.height = -uv.height; }
            texture_style style{};
            style.double_modulation = true; style.additive = patch.additive;
            for (size_t vertex = 0; vertex < points.size(); ++vertex)
            {
                uint32_t color{};
                for (int shift = 0; shift <= 24; shift += 8)
                {
                    const auto component = ((patch.colors[vertex] >> shift) & 255) * ((tint >> shift) & 255) / 255;
                    color |= static_cast<uint32_t>(component * (shift == 24 ? opacity : 1)) << shift;
                }
                style.colors[vertex] = color;
            }
            design.DrawTextureQuad(this->textures_[index], points, uv, style);
        }
        return true;
    }
}
