#include "game_icon_cache.hpp"
#include "resources/embedded_resource.hpp"
#include "resources/item_icon.hpp"
#include "resources/recast_icons.hpp"
#include "resources/action_icons.hpp"

#include <algorithm>
#include <cctype>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui::ui
{
    game_icon_cache::~game_icon_cache(void) { this->Shutdown(); }

    auto game_icon_cache::Clear(void) -> void
    {
        for (const auto& [id, item] : this->items_)
        {
            (void)id;
            if (item.texture != nullptr)
                item.texture->Release();
        }
        this->items_.clear();
        for (const auto& [id, texture] : this->atlases_)
        {
            (void)id;
            if (texture != nullptr) texture->Release();
        }
        this->atlases_.clear();
        this->access_ = 0;
    }

    auto game_icon_cache::SetResources(IResourceManager* resources) -> void
    {
        if (this->resources_ != resources)
            this->Clear();
        this->resources_ = resources;
    }

    auto game_icon_cache::SetDevice(IDirect3DDevice8* device) -> void
    {
        if (this->device_ != device)
            this->Clear();
        this->device_ = device;
    }

    auto game_icon_cache::Shutdown(void) -> void
    {
        this->Clear();
        this->device_ = nullptr;
        this->resources_ = nullptr;
    }

    auto game_icon_cache::Item(const uint32_t id) -> IDirect3DTexture8*
    {
        return this->Icon(id, false);
    }

    auto game_icon_cache::ActionIcon(const std::string_view key) const -> game_icon_view
    {
        if (key.empty()) return {};
        std::string normalized{key};
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(c)); });
        if (normalized.ends_with(".png")) normalized.resize(normalized.size() - 4);
        if (const auto prefix = normalized.find("assets/hotbar/"); prefix != std::string::npos) normalized.erase(0, prefix + 14);
        const auto& icons = resources::action_icons;
        const auto found = std::lower_bound(icons.begin(), icons.end(), std::string_view{normalized}, [](const auto& icon, std::string_view value) { return icon.key < value; });
        if (found == icons.end() || found->key != normalized) return {};
        return {this->Atlas(1100, resources::action_icon_width, resources::action_icon_height), {static_cast<float>((found->index % 56) * 36 + 2) / resources::action_icon_width,
            static_cast<float>((found->index / 56) * 36 + 2) / resources::action_icon_height,
            32.0f / resources::action_icon_width, 32.0f / resources::action_icon_height}};
    }

    auto game_icon_cache::Status(const uint32_t id) -> IDirect3DTexture8*
    {
        return this->Icon(id, true);
    }

    auto game_icon_cache::StatusIcon(const uint32_t id, const int preference) -> game_icon_view
    {
        const auto theme = StatusArtTheme(id, preference, this->status_theme_);
        if (const auto texture = theme > 0 ? this->Atlas(static_cast<uint16_t>(799 + theme), 1024, 1024) : nullptr; texture != nullptr)
            return {texture, {(static_cast<float>(id % 32) * 32 + 0.5f) / 1024,
                (static_cast<float>(id / 32) * 32 + 0.5f) / 1024, 31.0f / 1024, 31.0f / 1024}};
        return {this->Status(id)};
    }

    auto game_icon_cache::PhysicalDamageIcon(const size_t index) const -> game_icon_view
    {
        return index < 4 ? game_icon_view{this->Atlas(704, 256, 64),
            {(static_cast<float>(index) * 64 + 16.5f) / 256, 16.5f / 64, 31.0f / 256, 31.0f / 64}} : game_icon_view{};
    }

    auto game_icon_cache::MobDetectionIcon(const mob_detection_icon icon) const -> game_icon_view
    {
        const auto index = static_cast<size_t>(icon);
        return index < static_cast<size_t>(mob_detection_icon::count) ? game_icon_view{this->Atlas(706, 256, 256),
            {(static_cast<float>(index % 4) * 64 + 16.5f) / 256, (static_cast<float>(index / 4) * 64 + 16.5f) / 256,
                31.0f / 256, 31.0f / 256}} : game_icon_view{};
    }

    auto game_icon_cache::PetPortrait(const std::string_view name, const int kind, const bool jobFallback) const -> game_icon_view
    {
        const auto index = PetPortraitIndex(name, kind);
        if (const auto texture = index >= 0 ? this->Atlas(900, 1024, 512) : nullptr; texture != nullptr)
            return {texture, {(static_cast<float>(index % 8) * 128 + .5f) / 1024,
                (static_cast<float>(index / 8) * 128 + .5f) / 512, 127.0f / 1024, 127.0f / 512}};
        const auto job = jobFallback ? PetPortraitFallbackJob(kind) : 0;
        if (job == 0) return {};
        const auto cell = job - 1;
        return {this->JobAtlas(), {(static_cast<float>(cell % 8) * 64 + .5f) / 512,
            (static_cast<float>(cell / 8) * 64 + .5f) / 256, 63.0f / 512, 63.0f / 256}};
    }

    auto game_icon_cache::JobAtlas(void) const -> IDirect3DTexture8*
    {
        if (this->job_theme_ > 0)
            if (const auto texture = this->Atlas(static_cast<uint16_t>(802 + this->job_theme_), 512, 256)) return texture;
        return this->Atlas(201, 512, 256);
    }

    auto game_icon_cache::RecastAtlas(void) const -> IDirect3DTexture8*
    { return this->Atlas(205, resources::recast_icon_width, resources::recast_icon_height); }

    auto game_icon_cache::Atlas(const uint16_t id, const uint32_t width, const uint32_t height) const -> IDirect3DTexture8*
    {
        if (this->device_ == nullptr) return nullptr;
        auto [found, inserted] = this->atlases_.try_emplace(id, nullptr);
        if (inserted) found->second = this->LoadEmbeddedAtlas(id, width, height);
        return found->second;
    }

    auto game_icon_cache::LoadEmbeddedAtlas(const uint16_t id, const uint32_t expectedWidth, const uint32_t expectedHeight) const -> IDirect3DTexture8*
    {
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto bytes = resources::LoadEmbeddedResource(module, id, 8 + size_t{expectedWidth} * expectedHeight * 4);
        if (bytes.size() != 8 + size_t{expectedWidth} * expectedHeight * 4) return nullptr;
        const auto* pixels = bytes.data();
        uint32_t width{}, height{};
        std::memcpy(&width, pixels, 4); std::memcpy(&height, pixels + 4, 4);
        IDirect3DTexture8* texture{};
        if (width != expectedWidth || height != expectedHeight || FAILED(this->device_->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture))) return nullptr;
        D3DLOCKED_RECT locked{};
        if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
        { texture->Release(); return nullptr; }
        for (uint32_t y = 0; y < height; ++y)
            std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch, pixels + 8 + y * width * 4, width * 4);
        if (FAILED(texture->UnlockRect(0))) { texture->Release(); return nullptr; }
        return texture;
    }

    auto game_icon_cache::Icon(const uint32_t id, const bool status) -> IDirect3DTexture8*
    {
        if ((!status && id == 0) || (status && id > 1023) || this->resources_ == nullptr || this->device_ == nullptr)
            return nullptr;
        const auto key = id | (status ? 0x10000 : 0);
        if (auto found = this->items_.find(key); found != this->items_.end())
        {
            found->second.last_use = ++this->access_;
            return found->second.texture;
        }

        constexpr size_t capacity = 256;
        if (this->items_.size() >= capacity)
        {
            const auto oldest = std::min_element(this->items_.begin(), this->items_.end(), [](const auto& a, const auto& b) {
                return a.second.last_use < b.second.last_use;
            });
            if (oldest->second.texture != nullptr)
                oldest->second.texture->Release();
            this->items_.erase(oldest);
        }

        IDirect3DTexture8* texture{};
        std::span<const uint8_t> bytes{};
        if (status)
        {
            const auto* icon = this->resources_->GetStatusIconByIndex(static_cast<uint16_t>(id));
            if (icon != nullptr && icon->ImageSize > 0 && icon->ImageSize <= sizeof(icon->Bitmap)) bytes = {icon->Bitmap, icon->ImageSize};
        }
        else
        {
            // Native item bitmaps include their quality borders.
            const auto* item = this->resources_->GetItemById(id);
            if (item != nullptr && item->ImageSize > 0 && item->ImageSize <= sizeof(item->Bitmap)) bytes = {item->Bitmap, item->ImageSize};
        }
        if (!bytes.empty())
        {
            const auto bitmap = resources::DecodeItemIcon(bytes);
            if (bitmap && SUCCEEDED(this->device_->CreateTexture(bitmap->width, bitmap->height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
            {
                D3DLOCKED_RECT locked{};
                if (SUCCEEDED(texture->LockRect(0, &locked, nullptr, 0)))
                {
                    for (uint32_t y = 0; y < bitmap->height; ++y)
                        std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch,
                            bitmap->pixels.data() + y * bitmap->width, bitmap->width * sizeof(uint32_t));
                    if (FAILED(texture->UnlockRect(0)))
                    {
                        texture->Release();
                        texture = nullptr;
                    }
                }
                else
                {
                    texture->Release();
                    texture = nullptr;
                }
            }
        }
        this->items_.emplace(key, entry{texture, ++this->access_});
        return texture;
    }
} // namespace xiui::ui
