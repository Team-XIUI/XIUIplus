#include "wic_image.hpp"

#include <Windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace xiui::resources
{
    auto DecodeWicImage(const std::span<const uint8_t> bytes, const uint32_t maximumDimension,
        const bool square, const uint32_t fitSquare) -> std::optional<menu_texture>
    {
        using Microsoft::WRL::ComPtr;
        if (bytes.empty() || bytes.size() > MAXDWORD || maximumDimension == 0 || maximumDimension > 4096 || fitSquare > 512) return {};
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return {};
        ComPtr<IWICStream> stream;
        if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())))) return {};
        ComPtr<IWICBitmapDecoder> decoder;
        if (FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) return {};
        ComPtr<IWICBitmapFrameDecode> frame;
        if (FAILED(decoder->GetFrame(0, &frame))) return {};
        UINT width{}, height{};
        if (FAILED(frame->GetSize(&width, &height)) || width == 0 || height == 0 || width > maximumDimension || height > maximumDimension || (square && width != height)) return {};
        ComPtr<IWICBitmapSource> source = frame;
        if (fitSquare != 0)
        {
            const auto longest = std::max(width, height);
            ComPtr<IWICFormatConverter> pixels;
            if (FAILED(factory->CreateFormatConverter(&pixels)) || FAILED(pixels->Initialize(source.Get(),
                longest > fitSquare ? GUID_WICPixelFormat32bppPBGRA : GUID_WICPixelFormat32bppBGRA,
                WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
            source = pixels;
            const UINT scaledWidth = std::max(1u, width * fitSquare / longest), scaledHeight = std::max(1u, height * fitSquare / longest);
            ComPtr<IWICBitmapScaler> scaler;
            if (FAILED(factory->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(source.Get(), scaledWidth, scaledHeight,
                longest > fitSquare ? WICBitmapInterpolationModeFant : WICBitmapInterpolationModeNearestNeighbor))) return {};
            source = scaler; width = scaledWidth; height = scaledHeight;
        }
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(factory->CreateFormatConverter(&converter)) || FAILED(converter->Initialize(source.Get(), GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return {};
        menu_texture image{width, height, {}};
        image.pixels.resize(size_t{width} * height);
        if (FAILED(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(image.pixels.size() * 4), reinterpret_cast<BYTE*>(image.pixels.data())))) return {};
        if (fitSquare == 0 || (width == fitSquare && height == fitSquare)) return image;
        menu_texture fitted{fitSquare, fitSquare, {}};
        fitted.pixels.resize(size_t{fitSquare} * fitSquare);
        const uint32_t x = (fitSquare - width) / 2, y = (fitSquare - height) / 2;
        for (uint32_t row = 0; row < height; ++row)
            std::copy_n(image.pixels.data() + size_t{row} * width, width, fitted.pixels.data() + size_t{row + y} * fitSquare + x);
        return fitted;
    }
}
