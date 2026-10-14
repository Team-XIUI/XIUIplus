#include "game_icon_cache.hpp"
#include "resources/item_icon.hpp"
#include "resources/recast_icons.hpp"

#include <algorithm>

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
        if (device != nullptr)
        {
            if (this->jobs_ == nullptr) this->jobs_ = this->LoadEmbeddedAtlas(201, 512, 256);
            if (this->notifications_ == nullptr) this->notifications_ = this->LoadEmbeddedAtlas(203, 256, 64);
            if (this->recasts_ == nullptr) this->recasts_ = this->LoadEmbeddedAtlas(205, resources::recast_icon_width, resources::recast_icon_height);
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

    auto game_icon_cache::Status(const uint32_t id) -> IDirect3DTexture8*
    {
        return this->Icon(id, true);
    }

    auto game_icon_cache::LoadEmbeddedAtlas(const uint16_t id, const uint32_t expectedWidth, const uint32_t expectedHeight) const -> IDirect3DTexture8*
    {
        const auto module = reinterpret_cast<HMODULE>(&__ImageBase);
        const auto resource = ::FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
        if (resource == nullptr || ::SizeofResource(module, resource) != 8 + expectedWidth * expectedHeight * 4) return nullptr;
        const auto loaded = ::LoadResource(module, resource);
        const auto* pixels = loaded != nullptr ? static_cast<const uint8_t*>(::LockResource(loaded)) : nullptr;
        if (pixels == nullptr) return nullptr;
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
