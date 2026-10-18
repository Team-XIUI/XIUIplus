#include "minimap_maps.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <wincodec.h>
#include "src/webp/decode.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace
{
    template<typename T> struct release_com { auto operator()(T* value) const -> void { if (value != nullptr) value->Release(); } };
    template<typename T> using com_ptr = std::unique_ptr<T, release_com<T>>;
}

namespace xiui
{
    auto minimap_maps::DecodeImage(const std::span<const uint8_t> bytes) -> std::optional<resources::menu_texture>
    {
        if (bytes.size() >= 12 && std::memcmp(bytes.data(), "RIFF", 4) == 0 && std::memcmp(bytes.data() + 8, "WEBP", 4) == 0)
        {
            int width{}, height{};
            if (!WebPGetInfo(bytes.data(), bytes.size(), &width, &height) || width <= 0 || width > 1024 || height != width) return {};
            resources::menu_texture image{};
            image.width = static_cast<uint32_t>(width); image.height = static_cast<uint32_t>(height);
            image.pixels.resize(size_t{image.width} * image.height);
            if (WebPDecodeBGRAInto(bytes.data(), bytes.size(), reinterpret_cast<uint8_t*>(image.pixels.data()),
                image.pixels.size() * 4, width * 4) == nullptr) return {};
            return image;
        }
        IWICImagingFactory* rawFactory{};
        if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&rawFactory)))) return {};
        com_ptr<IWICImagingFactory> factory{rawFactory};
        IWICStream* rawStream{};
        if (FAILED(factory->CreateStream(&rawStream))) return {};
        com_ptr<IWICStream> stream{rawStream};
        if (FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())))) return {};
        IWICBitmapDecoder* rawDecoder{};
        if (FAILED(factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, &rawDecoder))) return {};
        com_ptr<IWICBitmapDecoder> decoder{rawDecoder};
        IWICBitmapFrameDecode* rawFrame{};
        if (FAILED(decoder->GetFrame(0, &rawFrame))) return {};
        com_ptr<IWICBitmapFrameDecode> frame{rawFrame};
        xiui::resources::menu_texture image{};
        if (FAILED(frame->GetSize(&image.width, &image.height)) || image.width == 0 || image.width > 1024 || image.height != image.width) return {};
        IWICFormatConverter* rawConverter{};
        if (FAILED(factory->CreateFormatConverter(&rawConverter))) return {};
        com_ptr<IWICFormatConverter> converter{rawConverter};
        if (FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
        image.pixels.resize(size_t{image.width} * image.height);
        if (FAILED(converter->CopyPixels(nullptr, image.width * 4, static_cast<UINT>(image.pixels.size() * 4), reinterpret_cast<BYTE*>(image.pixels.data())))) return {};
        return image;
    }
}

namespace xiui
{
    auto minimap_maps::Initialize(void) -> void
    {
        this->initialized_ = true;
        HMODULE module{};
        if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&minimap_maps::DecodeImage), &module)) return;
        const auto resource = ::FindResourceA(module, MAKEINTRESOURCEA(1000), RT_RCDATA);
        const auto length = resource != nullptr ? ::SizeofResource(module, resource) : 0;
        const auto* bytes = resource != nullptr ? static_cast<const uint8_t*>(::LockResource(::LoadResource(module, resource))) : nullptr;
        if (bytes == nullptr || length < 8 || std::memcmp(bytes, "XMAP", 4) != 0) return;
        uint32_t count{}; std::memcpy(&count, bytes + 4, 4);
        if (count > 2048 || count * 16 > length - 8) return;
        this->records_.reserve(count); this->textures_.reserve(8);
        for (uint32_t i = 0; i < count; ++i)
        {
            const auto* row = bytes + 8 + i * 16;
            record entry{}; uint32_t offset{}, size{};
            std::memcpy(&entry.zone, row, 2); std::memcpy(&entry.floor, row + 2, 2); entry.style = row[4];
            std::memcpy(&offset, row + 8, 4); std::memcpy(&size, row + 12, 4);
            if (offset < 8 + count * 16 || offset > length || size == 0 || size > length - offset || size > 4 * 1024 * 1024 || entry.style > 1)
            { this->records_.clear(); return; }
            entry.bytes = {bytes + offset, size}; this->records_.push_back(entry);
        }
    }

    auto minimap_maps::ReleaseTextures(void) -> void
    {
        for (auto& entry : this->textures_) if (entry.texture != nullptr) entry.texture->Release();
        this->textures_.clear();
    }

    auto minimap_maps::SetDevice(IDirect3DDevice8* device) -> void
    {
        if (device == this->device_) return;
        this->ReleaseTextures(); this->device_ = device;
        std::lock_guard lock{this->mutex_};
        ++this->generation_; this->requested_.reset(); this->completed_.reset();
    }

    auto minimap_maps::Shutdown(void) -> void
    {
        {
            std::lock_guard lock{this->mutex_}; this->stopping_ = true; this->requested_.reset();
        }
        this->wake_.notify_one();
        if (this->worker_.joinable()) this->worker_.join();
        this->ReleaseTextures(); this->device_ = nullptr; this->records_.clear(); this->completed_.reset();
        this->initialized_ = false; this->stopping_ = false; this->selection_.reset();
    }

    auto minimap_maps::Run(void) -> void
    {
        const auto com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        for (;;)
        {
            std::unique_lock lock{this->mutex_};
            this->wake_.wait(lock, [this] { return this->stopping_ || this->requested_.has_value(); });
            if (this->stopping_) break;
            const auto request = *this->requested_; const auto bytes = this->records_[request.first].bytes;
            lock.unlock();
            std::optional<resources::menu_texture> image{};
            try { if (SUCCEEDED(com)) image = DecodeImage(bytes); } catch (...) {}
            lock.lock();
            if (this->requested_ == request)
            {
                this->completed_ = decoded{request.first, request.second, std::move(image)};
                this->requested_.reset();
            }
        }
        if (SUCCEEDED(com)) ::CoUninitialize();
    }

    auto minimap_maps::Acquire(const uint16_t zone, const int floor, const bool enhanced) -> minimap_image
    {
        if (!this->initialized_) this->Initialize();
        if (!this->selection_ || this->selection_->zone != zone || this->selection_->floor != floor || this->selection_->enhanced != enhanced)
        {
            this->selection_ = selection{zone, floor, enhanced, this->records_.size()};
            for (size_t i = 0; i < this->records_.size(); ++i)
            {
                const auto& record = this->records_[i];
                if (record.zone == zone && record.floor == floor && (record.style == 0 || enhanced) &&
                    (this->selection_->record == this->records_.size() || record.style > this->records_[this->selection_->record].style)) this->selection_->record = i;
            }
        }
        const auto index = this->selection_->record;
        if (index == this->records_.size()) return {};
        minimap_image result{nullptr, this->records_[index].style == 1, true, this->device_ != nullptr};
        if (this->device_ == nullptr) return result;
        ++this->tick_;
        for (auto& entry : this->textures_)
            if (entry.record == index) { entry.used = this->tick_; result.texture = entry.texture; result.loading = false; return result; }
        std::lock_guard lock{this->mutex_};
        if (this->completed_ && this->completed_->record == index && this->completed_->generation == this->generation_)
        {
            IDirect3DTexture8* texture{};
            if (this->completed_->image)
            {
                const auto& image = *this->completed_->image;
                if (SUCCEEDED(this->device_->CreateTexture(image.width, image.height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
                {
                    D3DLOCKED_RECT locked{};
                    if (SUCCEEDED(texture->LockRect(0, &locked, nullptr, 0)))
                    {
                        for (uint32_t y = 0; y < image.height; ++y)
                            std::memcpy(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch, image.pixels.data() + y * image.width, image.width * 4);
                        texture->UnlockRect(0);
                    }
                    else { texture->Release(); texture = nullptr; }
                }
            }
            if (this->textures_.size() == 8)
            {
                const auto oldest = std::min_element(this->textures_.begin(), this->textures_.end(), [](const auto& a, const auto& b) { return a.used < b.used; });
                if (oldest->texture != nullptr) oldest->texture->Release(); this->textures_.erase(oldest);
            }
            this->textures_.push_back({index, texture, this->tick_}); this->completed_.reset();
            result.texture = texture; result.loading = false; return result;
        }
        const auto request = std::pair{index, this->generation_};
        if (this->requested_ != request)
        {
            this->requested_ = request; this->completed_.reset();
            if (!this->worker_.joinable()) this->worker_ = std::thread{[this] { this->Run(); }};
            this->wake_.notify_one();
        }
        return result;
    }
}
