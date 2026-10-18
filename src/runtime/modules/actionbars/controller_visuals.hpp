#pragma once

#include "controller_input.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace xiui::crossbar_visuals
{
    inline auto Progress(const uint64_t now, const uint64_t start, const float duration) -> float
    { return std::clamp(static_cast<float>(now >= start ? now - start : 0) / duration, 0.0f, 1.0f); }
    inline auto Cubic(const float value) -> float { const float remaining = 1 - value; return 1 - remaining * remaining * remaining; }
    inline auto Quad(const float value) -> float { return 1 - (1 - value) * (1 - value); }

    struct press_motion
    {
        bool pressed{}, started{};
        uint64_t press_at{}, release_at{}, hold_until{};
        float from{1}, release_from{1.12f};

        auto Value(const uint64_t now) const -> float
        {
            if (!this->started) return 1;
            if (!this->pressed && now >= this->release_at)
                return std::lerp(this->release_from, 1.0f, Quad(Progress(now, this->release_at, 80)));
            const float t = Progress(now, this->press_at, 80) - 1;
            const float back = 1 + 2.70158f * t * t * t + 1.70158f * t * t;
            return std::lerp(this->from, 1.12f, back);
        }
        auto Set(const bool down, const uint64_t now, const uint64_t minimumPress = 80) -> void
        {
            if (this->pressed == down) return;
            if (down)
            {
                this->from = this->Value(now);
                this->press_at = now;
                this->hold_until = now + minimumPress;
                this->started = this->pressed = true;
            }
            else
            {
                this->release_at = std::max(now, this->hold_until);
                this->release_from = this->Value(this->release_at);
                this->pressed = false;
            }
        }
    };

    struct bank_layer { float opacity{}, y{}; bool inactive{}; };
    struct bank_motion
    {
        std::array<bank_layer, 7> from{};
        size_t target{};
        uint64_t changed_at{};
        bool initialized{}, changing{}, target_inactive{};

        auto Value(const size_t bank, const uint64_t now, const float distance) const -> bank_layer
        {
            if (!this->initialized) return {};
            if (!this->changing) return {bank == this->target ? 1.0f : 0.0f, 0, this->target_inactive};
            const float p = Cubic(Progress(now, this->changed_at, 500)), q = Cubic(p);
            if (bank == this->target) return {std::lerp(this->from[bank].opacity, 1.0f, q), std::lerp(this->from[bank].y, 0.0f, q), this->target_inactive};
            return {this->from[bank].opacity * (1 - Quad(std::min(p * 1.5f, 1.0f))), std::lerp(this->from[bank].y, -distance, q), this->from[bank].inactive};
        }
        auto Select(const size_t bank, const uint64_t now, const float distance, const bool animate, const bool inactive = false) -> void
        {
            if (!this->initialized || !animate)
            { *this = {}; this->initialized = true; this->target = bank; this->target_inactive = inactive; return; }
            if (bank == this->target) { this->target_inactive = inactive; return; }
            // Retarget from the current picture while input adopts the new bank immediately.
            for (size_t index = 0; index < this->from.size(); ++index) this->from[index] = this->Value(index, now, distance);
            if (this->from[bank].opacity < 0.001f) this->from[bank].y = distance;
            this->target = bank; this->target_inactive = inactive; this->changed_at = now; this->changing = true;
        }
    };

    struct button_prompt { std::string_view icon{}; std::string label{}; };
    inline auto Direction(const size_t position) -> button_prompt
    {
        constexpr std::string_view icons[]{"controller/shared/up", "controller/shared/right", "controller/shared/down", "controller/shared/left"};
        constexpr const char* labels[]{"Up", "Right", "Down", "Left"};
        return {icons[position], labels[position]};
    }
    inline auto Face(const size_t position, const int theme) -> button_prompt
    {
        constexpr std::string_view xbox[]{"controller/xbox/y", "controller/xbox/b", "controller/xbox/a", "controller/xbox/x"};
        constexpr std::string_view playstation[]{"controller/playstation/triangle", "controller/playstation/circle", "controller/playstation/x", "controller/playstation/square"};
        constexpr std::string_view nintendo[]{"controller/nintendo/x", "controller/nintendo/a", "controller/nintendo/b", "controller/nintendo/y"};
        constexpr const char* positions[]{"T", "R", "B", "L"};
        return {theme == 1 ? xbox[position] : theme == 2 ? playstation[position] : theme == 3 ? nintendo[position] : std::string_view{}, positions[position]};
    }
    inline auto Button(const int binding, const controller_backend backend, const int theme, const size_t slot) -> button_prompt
    {
        if (binding == -1) return {{}, "-"};
        if (binding == -2) return slot < 4 ? Direction(slot) : Face(slot - 4, theme);
        if (backend == controller_backend::xinput)
        {
            constexpr size_t faces[]{1, 3, 0, 2}, directions[]{1, 3, 0, 2};
            if (binding >= 0 && binding < 4) return Face(faces[binding], theme);
            if (binding >= 4 && binding < 8) return Direction(directions[binding - 4]);
            switch (binding)
            {
            case 8: return {"controller/shared/l1", "LB"};
            case 9: return {"controller/shared/l2", "LT"};
            case 10: return {{}, "L3"};
            case 11: return {"controller/shared/r1", "RB"};
            case 12: return {"controller/shared/r2", "RT"};
            case 13: return {{}, "R3"};
            case 14: return {{}, "ST"};
            case 15: return {{}, "BK"};
            default: return {{}, "?"};
            }
        }
        if (binding == 40 || binding == -40 || binding == 41 || binding == -41)
        {
            const bool negative = (slot == 0 || slot == 3) != (binding < 0);
            return Direction(std::abs(binding) == 41 ? (negative ? 0 : 2) : (negative ? 3 : 1));
        }
        if (binding >= 0 && binding < 4 && theme != 4)
        {
            constexpr size_t xbox[]{2, 1, 3, 0}, playstation[]{3, 2, 1, 0}, nintendo[]{2, 1, 3, 0};
            return Face(theme == 1 ? xbox[binding] : theme == 2 ? playstation[binding] : nintendo[binding], theme);
        }
        if (theme != 4)
        {
            if (binding == 4) return {"controller/shared/l1", "L1"};
            if (binding == 5) return {"controller/shared/r1", "R1"};
            if (binding == 6) return {"controller/shared/l2", "L2"};
            if (binding == 7) return {"controller/shared/r2", "R2"};
        }
        return {{}, binding >= 0 && binding <= 127 && (binding < 32 || binding >= 42) ? "B" + std::to_string(binding + 1) : "?"};
    }
    inline auto Trigger(const bool right, const int binding, const controller_backend backend, const int theme) -> button_prompt
    {
        if (binding == -2) return {right ? "controller/shared/r2" : "controller/shared/l2", right ? "RT" : "LT"};
        return Button(binding, backend, theme, right ? 1 : 0);
    }
}
