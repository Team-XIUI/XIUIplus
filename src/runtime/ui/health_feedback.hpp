#pragma once

#include <d3d8.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace xiui::ui
{
    struct health_feedback_style
    {
        bool enabled{true}, damage{true}, healing{true}, flashes{true};
        bool warning_colors{true};
        float delay{0.2f}, decay{0.6f}, flash_duration{0.22f}, flash_strength{0.25f};
        D3DCOLOR damage_color{0xDFF7FBFF}, heal_color{0xBF79E5AC};
    };

    struct health_feedback_frame
    {
        float start{}, end{}, opacity{}, flash{};
        bool healing{};
    };

    struct health_feedback_state
    {
        float observed{}, extent{};
        uint64_t last_sample{}, changed_at{};
        bool initialized{}, gaining{};

        auto Update(float ratio, uint64_t now, const health_feedback_style& style) -> health_feedback_frame
        {
            ratio = std::isfinite(ratio) ? std::clamp(ratio, 0.0f, 1.0f) : 0;
            if (!this->initialized || !style.enabled || now < this->last_sample || now - this->last_sample > 1500)
            {
                *this = {ratio, ratio, now, now, true, false};
                return {};
            }
            const auto progress = [&](uint64_t stamp) {
                const float age = now >= stamp ? (now - stamp) * .001f : 0;
                return std::clamp(1 - (age - style.delay) / std::max(.01f, style.decay), 0.0f, 1.0f);
            };
            if (std::abs(ratio - this->observed) > .00001f)
            {
                const float currentExtent = this->observed + (this->extent - this->observed) * progress(this->changed_at);
                const bool gain = ratio > this->observed;
                this->extent = gain ? std::min(this->observed, this->gaining ? currentExtent : this->observed)
                    : std::max(this->observed, this->gaining ? this->observed : currentExtent);
                this->gaining = gain;
                this->changed_at = now;
                this->observed = ratio;
            }
            this->last_sample = now;
            const float amount = progress(this->changed_at);
            const float edge = ratio + (this->extent - ratio) * amount;
            const float age = (now - this->changed_at) * .001f;
            const bool active = std::abs(this->extent - ratio) > .00001f;
            return {std::min(edge, ratio), std::max(edge, ratio),
                active && (this->gaining ? style.healing : style.damage) ? amount : 0,
                active && style.flashes ? std::clamp(1 - age / std::max(.01f, style.flash_duration), 0.0f, 1.0f) * style.flash_strength : 0,
                this->gaining};
        }
    };
}
