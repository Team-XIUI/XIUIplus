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
        if (this->jobs_ != nullptr) this->jobs_->Release();
        this->jobs_ = nullptr;
        if (this->notifications_ != nullptr) this->notifications_->Release();
        this->notifications_ = nullptr;
        if (this->recasts_ != nullptr) this->recasts_->Release();
        this->recasts_ = nullptr;
        if (this->actions_ != nullptr) this->actions_->Release();
        this->actions_ = nullptr;
        if (this->magic_burst_ != nullptr) this->magic_burst_->Release();
        this->magic_burst_ = nullptr;
        if (this->blue_magic_ != nullptr) this->blue_magic_->Release();
        this->blue_magic_ = nullptr;
        if (this->pet_portraits_ != nullptr) this->pet_portraits_->Release();
        this->pet_portraits_ = nullptr;
        if (this->physical_damage_ != nullptr) this->physical_damage_->Release();
        this->physical_damage_ = nullptr;
        if (this->mob_detection_ != nullptr) this->mob_detection_->Release();
        this->mob_detection_ = nullptr;
        for (auto* themes : {&this->status_themes_, &this->job_themes_})
            for (auto*& texture : *themes) { if (texture != nullptr) texture->Release(); texture = nullptr; }
        this->atlases_loaded_ = false;
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
        if (device != nullptr && !this->atlases_loaded_)
        {
            this->atlases_loaded_ = true;
            if (this->jobs_ == nullptr) this->jobs_ = this->LoadEmbeddedAtlas(201, 512, 256);
            if (this->notifications_ == nullptr) this->notifications_ = this->LoadEmbeddedAtlas(203, 256, 64);
            if (this->recasts_ == nullptr) this->recasts_ = this->LoadEmbeddedAtlas(205, resources::recast_icon_width, resources::recast_icon_height);
            this->actions_ = this->LoadEmbeddedAtlas(1100, resources::action_icon_width, resources::action_icon_height);
            this->magic_burst_ = this->LoadEmbeddedAtlas(1200, 1024, 512);
            this->blue_magic_ = this->LoadEmbeddedAtlas(600, 900, 450);
            this->pet_portraits_ = this->LoadEmbeddedAtlas(900, 1024, 512);
            this->physical_damage_ = this->LoadEmbeddedAtlas(704, 256, 64);
            this->mob_detection_ = this->LoadEmbeddedAtlas(706, 256, 256);
            for (size_t index = 0; index < this->status_themes_.size(); ++index)
            {
                this->status_themes_[index] = this->LoadEmbeddedAtlas(static_cast<uint16_t>(800 + index), 1024, 1024);
                this->job_themes_[index] = this->LoadEmbeddedAtlas(static_cast<uint16_t>(803 + index), 512, 256);
            }
        }
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
        if (this->actions_ == nullptr || key.empty()) return {};
        std::string normalized{key};
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(c)); });
        if (normalized.ends_with(".png")) normalized.resize(normalized.size() - 4);
        if (const auto prefix = normalized.find("assets/hotbar/"); prefix != std::string::npos) normalized.erase(0, prefix + 14);
        const auto& icons = resources::action_icons;
        const auto found = std::lower_bound(icons.begin(), icons.end(), std::string_view{normalized}, [](const auto& icon, std::string_view value) { return icon.key < value; });
        if (found == icons.end() || found->key != normalized) return {};
        return {this->actions_, {static_cast<float>((found->index % 56) * 36 + 2) / resources::action_icon_width,
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
        if (theme > 0 && this->status_themes_[theme - 1] != nullptr)
            return {this->status_themes_[theme - 1], {(static_cast<float>(id % 32) * 32 + 0.5f) / 1024,
                (static_cast<float>(id / 32) * 32 + 0.5f) / 1024, 31.0f / 1024, 31.0f / 1024}};
        return {this->Status(id)};
    }

    auto game_icon_cache::PhysicalDamageIcon(const size_t index) const -> game_icon_view
    {
        return index < 4 ? game_icon_view{this->physical_damage_,
            {(static_cast<float>(index) * 64 + 16.5f) / 256, 16.5f / 64, 31.0f / 256, 31.0f / 64}} : game_icon_view{};
    }

    auto game_icon_cache::MobDetectionIcon(const mob_detection_icon icon) const -> game_icon_view
    {
        const auto index = static_cast<size_t>(icon);
        return index < static_cast<size_t>(mob_detection_icon::count) ? game_icon_view{this->mob_detection_,
            {(static_cast<float>(index % 4) * 64 + 16.5f) / 256, (static_cast<float>(index / 4) * 64 + 16.5f) / 256,
                31.0f / 256, 31.0f / 256}} : game_icon_view{};
    }

    auto game_icon_cache::PetPortrait(const std::string_view name, const int kind, const bool jobFallback) const -> game_icon_view
    {
        const auto index = PetPortraitIndex(name, kind);
        if (index >= 0 && this->pet_portraits_ != nullptr)
            return {this->pet_portraits_, {(static_cast<float>(index % 8) * 128 + .5f) / 1024,
                (static_cast<float>(index / 8) * 128 + .5f) / 512, 127.0f / 1024, 127.0f / 512}};
        const auto job = jobFallback ? PetPortraitFallbackJob(kind) : 0;
        if (job == 0) return {};
        const auto cell = job - 1;
        return {this->JobAtlas(), {(static_cast<float>(cell % 8) * 64 + .5f) / 512,
            (static_cast<float>(cell / 8) * 64 + .5f) / 256, 63.0f / 512, 63.0f / 256}};
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
