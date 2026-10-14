#include "ui/tp_meter.hpp"

#include <cstdio>

namespace
{
    auto Check(const bool result, const char* message) -> bool
    {
        if (!result) std::fprintf(stderr, "TP meter check failed: %s\n", message);
        return result;
    }
    auto Near(const float a, const float b) -> bool { return std::abs(a - b) < 0.0001f; }
}

int main(void)
{
    constexpr D3DCOLOR top = 0x9A36586B, bottom = 0xC878C4EE;
    struct sample { float tp; std::array<float, 3> fills; };
    const sample samples[]{
        {0, {0, 0, 0}}, {500, {0.5f, 0, 0}}, {999, {0.999f, 0, 0}}, {1000, {1, 0, 0}},
        {1500, {1, 0.5f, 0}}, {2000, {1, 1, 0}}, {2500, {1, 1, 0.5f}}, {3000, {1, 1, 1}}, {4000, {1, 1, 1}}};
    for (const auto& sample : samples)
    {
        const auto layers = xiui::ui::TPMeterLayers(sample.tp / 3000, top, bottom, 0, false);
        for (size_t index = 0; index < layers.size(); ++index)
            if (!Check(Near(layers[index].ratio, sample.fills[index]) && (layers[index].top >> 24) == (top >> 24) &&
                (layers[index].bottom >> 24) == (bottom >> 24), "each 1,000 TP fills one complete overlay and preserves gradient alpha")) return 1;
        if (!Check(layers[0].top == top && layers[0].bottom == bottom && layers[1].bottom != bottom && layers[2].bottom != layers[1].bottom &&
            layers[1].top != layers[1].bottom && layers[2].top != layers[2].bottom, "base colors stay intact and overlays have distinct styled gradients")) return 1;
    }
    const auto low = xiui::ui::TPMeterLayers(999.0f / 3000, top, bottom, 0.5, true);
    const auto quiet = xiui::ui::TPMeterLayers(0.5f, top, bottom, 0, true);
    const auto bright = xiui::ui::TPMeterLayers(0.5f, top, bottom, 0.5, true);
    const auto disabled = xiui::ui::TPMeterLayers(0.5f, top, bottom, 0.5, false);
    if (!Check(low[0].bottom == bottom && quiet[0].bottom == bottom && bright[0].bottom != bottom && disabled[0].bottom == bottom &&
        bright[1].ratio == quiet[1].ratio && (bright[0].bottom >> 24) == (bottom >> 24), "ready blink affects only color, keeps alpha and stops below 1,000 TP or when disabled")) return 1;
    const auto spent = xiui::ui::TPMeterLayers(0, top, bottom, 0.5, true);
    if (!Check(spent[0].ratio == 0 && spent[1].ratio == 0 && spent[2].ratio == 0 && spent[0].bottom == bottom,
        "spending TP clears all stages without retaining a ready blink")) return 1;
    std::puts("TP 1,000/2,000/3,000 boundaries, overlay retention, custom gradients/alpha and subtle ready pulse checks passed.");
    return 0;
}
