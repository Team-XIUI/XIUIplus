#include "hud_runtime.hpp"
#include "input/controller_movement_layout.hpp"

#include <cmath>
#include <cstdio>

namespace
{
    auto Check(const bool value, const char* message) -> bool
    {
        if (!value) std::fprintf(stderr, "Grid check failed: %s\n", message);
        return value;
    }
    auto Near(const float a, const float b) -> bool { return std::abs(a - b) < 0.02f; }

    auto Snapping(void) -> bool
    {
        const xiui::ui::rect viewport{0, 0, 1000, 800};
        const std::array targets{xiui::ui::rect{300, 200, 200, 120}};
        xiui::ui::layout_grid_settings settings;
        auto result = xiui::ui::SnapLayoutBounds({196, 144, 100, 50}, targets, viewport, settings);
        if (!Check(Near(result.bounds.x, 200) && Near(result.bounds.y, 150) && result.vertical == 300 && result.horizontal == 200,
            "neighboring edges catch independently on both axes")) return false;
        result = xiui::ui::SnapLayoutBounds({347, 232, 100, 60}, targets, viewport, settings);
        if (!Check(Near(result.bounds.x, 350) && Near(result.bounds.y, 230) && result.vertical == 400 && result.horizontal == 260,
            "centers align across elements with different dimensions")) return false;
        result = xiui::ui::SnapLayoutBounds({180, 130, 100, 50}, targets, viewport, settings);
        if (!Check(Near(result.bounds.x, 180) && Near(result.bounds.y, 130) && !result.vertical && !result.horizontal,
            "movement stays free outside the snap distance")) return false;
        settings.snap_elements = false; settings.snap_grid = true;
        result = xiui::ui::SnapLayoutBounds({103, 158, 100, 60}, {}, viewport, settings);
        if (!Check(Near(result.bounds.x, 100) && Near(result.bounds.y, 160), "grid spacing supplies the actual snap coordinates")) return false;
        settings.visible = false; settings.amount = 1;
        result = xiui::ui::SnapLayoutBounds({103, 158, 100, 60}, {}, viewport, settings);
        if (!Check(Near(result.bounds.x, 103) && Near(result.bounds.y, 158), "one-pixel snap amount leaves more precise free movement")) return false;
        settings.amount = 20;
        result = xiui::ui::SnapLayoutBounds({111, 149, 100, 60}, {}, viewport, settings);
        if (!Check(Near(result.bounds.x, 120) && Near(result.bounds.y, 140), "hidden grid still snaps at the chosen amount")) return false;
        settings.snap_grid = false;
        result = xiui::ui::SnapLayoutBounds({111, 149, 100, 60}, targets, viewport, settings);
        if (!Check(Near(result.bounds.x, 111) && Near(result.bounds.y, 149), "both snapping toggles off give free movement")) return false;
        settings.snap_grid = true;
        result = xiui::ui::SnapLayoutBounds({899, 749, 100, 60}, {}, viewport, settings);
        return Check(result.bounds.x >= 0 && result.bounds.y >= 0 && result.bounds.x + result.bounds.width <= 1000 &&
            result.bounds.y + result.bounds.height <= 800, "snapping cannot pull a fitting element past the viewport edge");
    }

    auto Preferences(void) -> bool
    {
        xiui::general_module general;
        const auto grid = general.GridStart();
        for (size_t index = 0; index < xiui::general_module::grid_option_count; ++index)
        {
            size_t owners{};
            for (size_t section = 0; section < general.ConfigSections().size(); ++section)
                owners += general.ConfigSectionContains(section, grid + index);
            if (!Check(owners == 1 && general.ConfigSectionContains(xiui::general_module::grid_section, grid + index), "grid controls belong only to General Grid")) return false;
        }
        const auto amount = general.Option(grid + 3), spacing = general.Option(grid + 4);
        if (!Check(amount.minimum == 1 && amount.maximum == 20 && amount.step == 1 && spacing.minimum == 4 && spacing.maximum == 128,
            "snap amount and visual spacing have separate slider ranges")) return false;
        *general.Option(grid).toggle = false; *general.Option(grid + 1).toggle = true; *general.Option(grid + 2).toggle = false;
        *amount.slider = 17; *spacing.slider = 37; *general.Option(grid + 5).slider = 0.6f;
        auto store = xiui::settings_store::FromIniText(""); general.Save(store);
        xiui::general_module restored; restored.Load(store);
        const auto& saved = restored.Grid();
        if (!Check(!saved.visible && saved.snap_grid && !saved.snap_elements && saved.amount == 17 && saved.spacing == 37 && Near(saved.opacity, 0.6f),
            "grid preferences persist through ordinary settings and profiles")) return false;
        restored.Load(xiui::settings_store::FromIniText("[layout.grid]\namount=nan\nspacing=inf\nopacity=inf\n"));
        if (!Check(restored.Grid().amount == 6 && restored.Grid().spacing == 20 && restored.Grid().opacity == 0.25f, "invalid numeric preferences use finite defaults")) return false;
        restored.ResetDefaults(xiui::ui::theme::CreateDefault());
        return Check(restored.Grid().visible && !restored.Grid().snap_grid && restored.Grid().snap_elements, "reset restores independent grid defaults");
    }
}

namespace xiui
{
    struct layout_grid_test
    {
        static auto PartyEdge(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.party_.Load(settings_store::FromIniText("[party]\nstatus_placement=0\n[timing.party.swing]\ndisplay=1\n[timing.party.resting]\ndisplay=1\n"));
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            auto& bounds = runtime.party_.State().bounds;
            bounds.x = 4000;
            ClampMovementPosition(bounds, runtime.MovementBounds(runtime.party_), viewport);
            const auto placed = runtime.MovementBounds(runtime.party_);
            if (!Check(Near(placed.x + placed.width, 1920), "party placement reaches the right edge without reserving edit padding or external timers")) return false;
            const auto visual = runtime.party_.VisualBounds(true);
            if (!Check(visual.x + visual.width > placed.x + placed.width, "party edit bounds still include the outside artwork and countdowns")) return false;
            runtime.ClampLayout(viewport);
            const auto after = runtime.MovementBounds(runtime.party_);
            return Check(Near(after.x + after.width, 1920), "normal layout clamping preserves an edge-aligned party position");
        }

        static auto Toolbar(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_.bounds = {40, 40, 1100, 600};
            runtime.config_.module = &runtime.player_;
            runtime.player_.State().bounds = {1300, 740, 430, 88};
            runtime.player_.State().content_bounds = {-10, -5, 450, 105};
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            const auto click = [&](const size_t index) {
                const auto toolbar = runtime.MovementToolbar(viewport);
                if (!toolbar) return false;
                const auto button = toolbar->Button(index);
                runtime.mouse_x_ = static_cast<int>(button.x + button.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(button.y + button.height * 0.5f);
                if (!Check(runtime.ConfigSoundTarget(viewport) == runtime.MovementToolbarTarget(viewport) &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand, "centering buttons share cursor and sound targets")) return false;
                if (!runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport)) return false;
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto before = runtime.MovementBounds(runtime.player_);
            const auto dock = runtime.MovementToolbar(viewport);
            if (!Check(dock && Near(dock->bounds.y, 4) && Near(dock->bounds.x + dock->bounds.width * 0.5f, 960) && dock->bounds.height <= 28,
                "the compact movement strip is anchored at the top center of the screen")) return false;
            if (!Check(click(0), "Center X owns the full press/release from the docked strip")) return false;
            auto visual = runtime.MovementBounds(runtime.player_);
            if (!Check(Near(visual.x + visual.width * 0.5f, 960) && Near(visual.y, before.y), "Center X centers the visual bounds without changing Y")) return false;
            const auto x = visual.x;
            if (!Check(click(1), "Center Y captures its complete click from the docked strip")) return false;
            visual = runtime.MovementBounds(runtime.player_);
            if (!Check(std::abs(visual.y + visual.height * 0.5f - 540) <= 0.5f && Near(visual.x, x), "Center Y centers the visual bounds without changing X")) return false;
            const auto after = runtime.MovementToolbar(viewport);
            if (!Check(after && Near(after->bounds.x, dock->bounds.x) && Near(after->bounds.y, dock->bounds.y),
                "centering or moving the module leaves its screen toolbar in place")) return false;
            runtime.config_.module = &runtime.inventory_[1]; runtime.inventory_[1].State().enabled = false;
            if (!Check(!runtime.MovementToolbar(viewport), "disabled inventory trackers have no movement toolbar")) return false;
            const auto edge = ui::movement_toolbar::AtTop({0, 0, 1920, 1080});
            return Check(edge.bounds.x >= 0 && edge.bounds.y >= 0 && edge.bounds.x + edge.bounds.width <= 1920 && edge.bounds.y + edge.bounds.height <= 1080,
                "toolbar stays visible when a module touches the screen edge");
        }

        static auto Dragging(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_.bounds = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            const auto send = [&](const uint32_t message, const int x, const int y) {
                runtime.mouse_x_ = x; runtime.mouse_y_ = y;
                return runtime.ProcessPointerMessage(message, message == WM_LBUTTONUP ? 0 : MK_LBUTTON, viewport);
            };
            for (auto* moving : runtime.modules_.All())
            {
                if (!moving->HasEditableBounds() || !moving->PreviewVisible()) continue;
                for (auto* module : runtime.modules_.All())
                { module->State().bounds = {-4000, -4000, std::max(200.0f, module->State().minimum_width), std::max(80.0f, module->State().minimum_height)}; module->State().content_bounds = {}; }
                moving->State().bounds.x = 1253; moving->State().bounds.y = 703;
                auto* target = moving == &runtime.gil_ ? static_cast<hud_module*>(&runtime.equipment_) : &runtime.gil_;
                target->State().bounds.x = 1500; target->State().bounds.y = 900;
                const auto original = moving->State().bounds;
                send(WM_LBUTTONDOWN, 1263, 713);
                if (!Check(runtime.interaction_module_ == moving && Near(moving->State().bounds.x, original.x) && Near(moving->State().bounds.y, original.y),
                    "a stationary preview click focuses without snapping or moving it")) return false;
                const auto visual = runtime.MovementBounds(*moving), peer = runtime.MovementBounds(*target);
                const int mouseX = static_cast<int>(original.x + peer.x - visual.x - visual.width - 3 + 10);
                const int mouseY = static_cast<int>(original.y + peer.y - visual.y - visual.height - 3 + 10);
                send(WM_MOUSEMOVE, mouseX, mouseY);
                const auto snapped = runtime.MovementBounds(*moving);
                if (!Check(Near(snapped.x + snapped.width, peer.x) && Near(snapped.y + snapped.height, peer.y),
                    "shared drag handling snaps each visible HUD module by its real layout bounds"))
                { std::fprintf(stderr, "Module: %s\n", moving->Id()); return false; }
                send(WM_LBUTTONUP, mouseX, mouseY);
                if (!Check(!runtime.layout_snap_.vertical && !runtime.layout_snap_.horizontal && runtime.interaction_ == hud_runtime::interaction::none,
                    "release clears alignment guides and ends drag capture")) return false;
            }
            for (auto* module : runtime.modules_.All())
            { module->State().bounds.x = -4000; module->State().bounds.y = -4000; module->State().content_bounds = {}; }
            auto& hidden = runtime.inventory_[1]; hidden.State().enabled = false; hidden.State().bounds = {1500, 900, 200, 80};
            auto& player = runtime.player_.State().bounds; player = {1253, 703, 430, 88};
            send(WM_LBUTTONDOWN, 1263, 713);
            send(WM_MOUSEMOVE, 1077, 819);
            if (!Check(Near(player.x, 1067) && Near(player.y, 809) && !runtime.layout_snap_.vertical && !runtime.layout_snap_.horizontal,
                "disabled inventory trackers cannot attract a dragged element")) return false;
            send(WM_LBUTTONUP, 1077, 819);
            return true;
        }
    };
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view{argv[1]} == "--toolbar-only")
    {
        if (!xiui::layout_grid_test::Toolbar()) return 1;
        std::puts("Docked movement strip, centering actions and pointer capture checks passed.");
        return 0;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--party-edge-only")
    {
        if (!xiui::layout_grid_test::PartyEdge()) return 1;
        std::puts("Party edge placement and layout clamping checks passed.");
        return 0;
    }
    if (!Snapping() || !Preferences() || !xiui::layout_grid_test::Dragging() || !xiui::layout_grid_test::Toolbar()) return 1;
    std::puts("Grid/element snapping, preview drags, hidden inventory, preferences and centering toolbar capture checks passed.");
    return 0;
}
