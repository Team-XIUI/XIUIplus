#include "config/config_layout.hpp"
#include "config/contributor_list.hpp"
#include "config/element_selector.hpp"
#include "config/experience_display_editor.hpp"
#include "config/option_layout.hpp"

#include <cstdio>

namespace
{
    auto Check(const bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "config responsive check failed: %s\n", message);
        return condition;
    }

    auto Inside(const xiui::ui::rect& outer, const xiui::ui::rect& inner) -> bool
    {
        return inner.x >= outer.x && inner.y >= outer.y &&
            inner.x + inner.width <= outer.x + outer.width && inner.y + inner.height <= outer.y + outer.height;
    }
}

auto RunConfigResponsiveChecks(void) -> bool
{
    using namespace xiui::ui;
    const config_layout wide{{20, 20, 1100, 600}, 14, true, true};
    const config_layout narrow{{20, 20, 760, 391}, 14, true, true, 0, 0, false, -1, 0, false, 13};
    if (!Check(wide.Columns() == 2 && narrow.Columns() == 1 && wide.MinimumWidth() == narrow.MinimumWidth(),
        "content reflows without changing minimum window width between pages")) return false;
    if (!Check(narrow.MinimumHeight() == 391, "minimum height reserves readable topics above the shared footer")) return false;
    const auto left = wide.OptionRow(0), right = wide.OptionRow(1), next = wide.OptionRow(2);
    if (!Check(right.y == left.y && right.x >= left.x + left.width && next.y == left.y + config_layout::row_height &&
        narrow.OptionRow(1).x == narrow.OptionRow(0).x && narrow.OptionRow(1).y == narrow.OptionRow(0).y + config_layout::row_height,
        "one and two columns use the same row geometry")) return false;
    if (!Check(narrow.RowForSlot(-1) == -1 && wide.RowForSlot(-1) == -1 && wide.RowForSlot(-3) == -2,
        "offscreen controller rows remain above the visible content")) return false;
    for (const auto* layout : {&wide, &narrow})
    {
        const auto lastTab = layout->ModuleTab(13), lastTopic = layout->Submenu(12, 13);
        if (!Check(lastTab.height >= 18 && lastTab.y + lastTab.height < layout->ProfileSelector().y &&
            lastTopic.height >= 18 && lastTopic.y + lastTopic.height <= layout->Footer().y,
            "minimum height keeps navigation readable and separate from footer controls")) return false;
        for (int slot = 0; slot < layout->VisibleSlots(); ++slot)
            if (!Check(Inside(layout->ContentArea(), layout->OptionRow(slot)), "visible controls remain inside their content area")) return false;
    }
    const auto normalSlots = ArrangeConfigOptions({{0, 0, false}, {1, 1, true}, {2, 2, false}}, 99, 2);
    const auto narrowSlots = ArrangeConfigOptions({{0, 0, false}, {1, 1, true}, {2, 2, false}}, 99, 1);
    if (!Check(normalSlots == std::vector<size_t>{0, 99, 1, 2} && narrowSlots == std::vector<size_t>{0, 1, 2},
        "paired dimensions stay adjacent without blank rows in a single column")) return false;

    const std::array<float, 6> tabWidths{140, 140, 140, 140, 140, 140};
    const element_selector_layout tabs{narrow.ElementTabs(), tabWidths};
    const config_layout wrapped{{20, 20, 760, 600}, 14, true, true, 0, tabs.height};
    if (!Check(tabs.height == 100 && wrapped.OptionRow(0).y >= tabs.tabs.back().y + tabs.tabs.back().height,
        "wrapped selectors reserve their actual height before options")) return false;

    const config_layout cardStart{{20, 20, 760, 350}, 14, false, true, 0, 0, true, 6};
    const int end = cardStart.MaximumScroll(8);
    const config_layout cardEnd{{20, 20, 760, 350}, 14, false, true, 0, 0, true, 6, end};
    if (!Check(end == 6 && cardEnd.MaximumScroll(8) == end && cardEnd.AlignScroll(100, 8) == end &&
        Inside(cardEnd.ContentArea(), cardEnd.OptionRow(7 - end)) && cardEnd.OptionRow(7 - end).height == 104,
        "preset cards remain reachable with stable scrolling and full card heights")) return false;

    const contributor_list_layout wideCredits{wide}, narrowCredits{narrow};
    if (!Check(wideCredits.Member(0).y == wideCredits.Member(1).y &&
        narrowCredits.Member(0).x == narrowCredits.Member(1).x && narrowCredits.Member(1).y > narrowCredits.Member(0).y &&
        narrowCredits.MaximumScroll() > wideCredits.MaximumScroll(), "contributor rows reflow and update their scroll range")) return false;
    for (const float width : {760.0f, 1100.0f})
    for (const float height : {383.0f, 600.0f})
    {
        const config_layout base{{20, 20, width, height}, 14, false, true, 40};
        const auto layout = base.WithMinimumContentHeight(experience_display_editor::geometry::MinimumHeight(base.ContentArea().width));
        const experience_display_editor::geometry editor{layout};
        if (!Check(height >= layout.MinimumHeight() && Inside(layout.ContentArea(), editor.shown) && Inside(layout.ContentArea(), editor.available) &&
            Inside(layout.ContentArea(), editor.presets) && (width != 760 || editor.available.y > editor.shown.y + editor.shown.height),
            "readout lists stack at narrow widths and retain all controls in short windows")) return false;
    }
    return true;
}
