#pragma once

#include "menu_texture.hpp"

#include <cmath>

namespace xiui::resources
{
    inline auto UpscaleMenuTexture(const menu_texture& image, const uint32_t factor, const bool repeat = false) -> std::optional<menu_texture>
    {
        if (factor == 0 || factor > 8 || image.width == 0 || image.height == 0 ||
            image.width > 2048 / factor || image.height > 2048 / factor || image.pixels.size() != size_t{image.width} * image.height)
            return std::nullopt;
        struct tap { std::array<uint32_t, 4> indices{}; std::array<float, 4> weights{}; };
        const auto taps = [factor, repeat](const uint32_t length) {
            std::vector<tap> result(length * factor);
            for (uint32_t i = 0; i < result.size(); ++i)
            {
                const float position = (static_cast<float>(i) + 0.5f) / factor - 0.5f;
                const int base = static_cast<int>(std::floor(position));
                const float t = position - base, t2 = t * t, t3 = t2 * t;
                result[i].weights = {-0.5f * t + t2 - 0.5f * t3, 1 - 2.5f * t2 + 1.5f * t3,
                    0.5f * t + 2 * t2 - 1.5f * t3, -0.5f * t2 + 0.5f * t3};
                for (int sample = 0; sample < 4; ++sample)
                {
                    const int index = base + sample - 1, size = static_cast<int>(length);
                    result[i].indices[sample] = repeat ? (index % size + size) % size : std::clamp(index, 0, size - 1);
                }
            }
            return result;
        };
        const auto horizontal = taps(image.width), vertical = taps(image.height);
        using sample = std::array<float, 4>;
        std::vector<sample> rows(size_t{horizontal.size()} * image.height);
        // Filter premultiplied colors so transparent texels cannot introduce colored fringes.
        for (uint32_t y = 0; y < image.height; ++y)
        for (size_t x = 0; x < horizontal.size(); ++x)
        {
            auto& output = rows[size_t{y} * horizontal.size() + x];
            for (size_t i = 0; i < 4; ++i)
            {
                const auto pixel = image.pixels[size_t{y} * image.width + horizontal[x].indices[i]];
                const float alpha = static_cast<float>(pixel >> 24), weight = horizontal[x].weights[i];
                for (unsigned channel = 0; channel < 3; ++channel)
                    output[channel] += static_cast<float>((pixel >> (channel * 8)) & 255) * alpha / 255 * weight;
                output[3] += alpha * weight;
            }
        }
        menu_texture result{image.width * factor, image.height * factor, {}};
        result.pixels.resize(size_t{result.width} * result.height);
        for (uint32_t y = 0; y < result.height; ++y)
        for (uint32_t x = 0; x < result.width; ++x)
        {
            sample color{};
            for (size_t i = 0; i < 4; ++i)
                for (size_t channel = 0; channel < 4; ++channel)
                    color[channel] += rows[size_t{vertical[y].indices[i]} * result.width + x][channel] * vertical[y].weights[i];
            const auto alpha = static_cast<uint32_t>(std::clamp(color[3], 0.0f, 255.0f) + 0.5f);
            uint32_t pixel = alpha << 24;
            if (alpha != 0)
                for (unsigned channel = 0; channel < 3; ++channel)
                    pixel |= static_cast<uint32_t>(std::clamp(color[channel] * 255 / color[3], 0.0f, 255.0f) + 0.5f) << (channel * 8);
            result.pixels[size_t{y} * result.width + x] = pixel;
        }
        return result;
    }
}
