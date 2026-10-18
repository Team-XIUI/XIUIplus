#include "modules/satchel/satchel_module.hpp"
#include "modules/satchel/satchel_source.hpp"
#include "modules/satchel/satchel_cache.hpp"
#include "modules/satchel/satchel_actions.hpp"
#include "hud_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    { if (!condition) throw std::runtime_error{message}; }

    struct keyboard_restore
    {
        BYTE keys[256]{};
        keyboard_restore(void) { Check(::GetKeyboardState(this->keys) != FALSE, "capture thread-local keyboard state"); }
        ~keyboard_restore(void) { ::SetKeyboardState(this->keys); }
    };
}

namespace xiui
{
    struct satchel_data_test
    {
        static auto Set(satchel_source& source, satchel_snapshot snapshot) -> void { source.snapshot_ = std::move(snapshot); }
        static auto Set(satchel_cache& cache, satchel_snapshot snapshot) -> void { cache.characters_ = {std::move(snapshot)}; }
    };

    struct satchel_ui_test
    {
        static auto Fixture(void) -> satchel_snapshot
        {
            satchel_snapshot snapshot;
            snapshot.identity = "world-1-123"; snapshot.name = "Example"; snapshot.ready = true; snapshot.generation = 1;
            snapshot.bags[0].capacity = 20; snapshot.bags[0].ready = snapshot.bags[0].accessible = true;
            snapshot.bags[5].capacity = 20; snapshot.bags[5].ready = snapshot.bags[5].accessible = true;
            for (uint8_t slot = 1; slot <= 3; ++slot)
            {
                satchel_item item; item.id = 99 + slot; item.bag = 0; item.slot = slot; item.count = 1; item.generation = slot;
                snapshot.bags[0].items.push_back(item);
            }
            return snapshot;
        }

        static auto WindowInteraction(void) -> void
        {
            satchel_source source; satchel_cache cache; satchel_actions actions;
            const auto snapshot = Fixture(); satchel_data_test::Set(source, snapshot);
            auto saved = snapshot; saved.identity = "world-2-456"; saved.name = "Saved"; saved.read_only = true;
            satchel_data_test::Set(cache, saved);
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 80; module.state_.bounds.y = 60;
            module.Open(true); module.Update(1000); module.UpdateSize();
            const auto closeEnough = [](const float a, const float b) { return std::abs(a - b) < 0.1f; };
            const auto click = [&](const float x, const float y) {
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false), "dropdown press is captured");
                Check(module.HandleMouse(WM_LBUTTONUP, 0, x, y, false), "dropdown release is captured");
            };
            const auto openProfiles = [&] {
                const auto profile = module.CharacterBounds(); click(profile.x + profile.width * 0.5f, profile.y + profile.height * 0.5f);
                Check(module.dialog_ == satchel_module::dialog::characters, "profile button opens its dropdown");
            };
            openProfiles();
            const auto profile = module.CharacterBounds(); const auto dropdown = module.DialogBounds();
            Check(closeEnough(dropdown.width, 240) && closeEnough(dropdown.x + dropdown.width, profile.x + profile.width) &&
                closeEnough(dropdown.y, profile.y + profile.height + 6), "profile dropdown anchors directly below its button");
            click(4, 4);
            Check(module.Opened() && module.dialog_ == satchel_module::dialog::none && module.character_.empty(),
                "outside click dismisses the dropdown without selecting a character or closing Satchel");
            openProfiles(); module.HandleControllerAction(controller_action::cancel, false);
            Check(module.Opened() && module.dialog_ == satchel_module::dialog::none && module.character_.empty(),
                "Cancel dismisses the profile dropdown without changing its selection");

            module.OpenPicker(satchel_module::dialog::characters);
            while (module.picks_.size() < 10) module.picks_.push_back({"Saved character", "", 0});
            module.state_.bounds.x = module.viewport_.width - 30; module.state_.bounds.y = module.viewport_.height - 30;
            const auto edgeDropdown = module.DialogBounds(); const auto edgeProfile = module.CharacterBounds();
            Check(edgeDropdown.x >= 8 && edgeDropdown.y >= 8 && edgeDropdown.x + edgeDropdown.width <= module.viewport_.width - 8 &&
                edgeDropdown.y + edgeDropdown.height <= module.viewport_.height - 8 && edgeDropdown.y + edgeDropdown.height < edgeProfile.y,
                "long profile dropdown opens upward and clamps to the viewport near its lower-right edge");
            Check(edgeDropdown.y + 20 < module.state_.bounds.y && module.HandleMouse(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)),
                edgeDropdown.x + 20, edgeDropdown.y + 20, false) && module.pick_page_ == 1 && module.character_.empty(),
                "wheel input pages an upward dropdown outside the window without selecting a character");
            module.HandleControllerAction(controller_action::cancel, false);

            module.state_.bounds.x = 80; module.state_.bounds.y = 60; module.UpdateSize(); module.TakeSettingsChanged();
            const auto beforeMove = module.state_.bounds; const auto title = module.TitleBounds();
            const auto titleX = title.x + title.width * 0.5f, titleY = title.y + title.height * 0.5f;
            Check(module.HandleMouse(WM_LBUTTONDOWN, 0, titleX, titleY, false) && module.PointerDown() && !module.navigation_focus_,
                "mouse title press captures movement without a navigation focus ring");
            Check(module.HandleMouse(WM_MOUSEMOVE, 0, titleX + 60, titleY + 45, false), "title drag retains pointer ownership");
            Check(module.HandleMouse(WM_LBUTTONUP, 0, titleX + 60, titleY + 45, false), "title release commits window movement");
            Check(closeEnough(module.state_.bounds.x, beforeMove.x + 60) && closeEnough(module.state_.bounds.y, beforeMove.y + 45) &&
                closeEnough(module.state_.bounds.width, beforeMove.width) && closeEnough(module.state_.bounds.height, beforeMove.height) &&
                !module.PointerDown() && !module.navigation_focus_ && module.TakeSettingsChanged() && !module.TakeSettingsChanged(),
                "title drag changes only position and marks one committed settings change");
            module.navigation_focus_ = true; const auto movedTitle = module.TitleBounds(); click(movedTitle.x + 10, movedTitle.y + 10);
            Check(!module.navigation_focus_ && !module.TakeSettingsChanged(), "a title click clears navigation focus without changing the window");

            const auto beforeResize = module.state_.bounds;
            const auto resize = module.ResizeBounds(); const auto resizeX = resize.x + resize.width * 0.5f, resizeY = resize.y + resize.height * 0.5f;
            const auto pitch = module.icon_size_ + 8 + module.spacing_;
            Check(module.HandleMouse(WM_LBUTTONDOWN, 0, resizeX, resizeY, false) && module.PointerDown(), "resize grip captures the pointer");
            module.HandleMouse(WM_MOUSEMOVE, 0, resizeX + 7, resizeY + 11, false);
            Check(closeEnough(module.state_.bounds.width, beforeResize.width + 7) && closeEnough(module.state_.bounds.height, beforeResize.height + 11),
                "pointer resizing follows deltas smaller than an item cell");
            Check(module.HandleMouse(WM_LBUTTONUP, 0, resizeX + 8, resizeY + 12, false), "resize release commits the final pixel delta");
            Check(closeEnough(module.state_.bounds.x, beforeResize.x) && closeEnough(module.state_.bounds.y, beforeResize.y) &&
                closeEnough(module.state_.bounds.width, beforeResize.width + 8) && closeEnough(module.state_.bounds.height, beforeResize.height + 12) &&
                !module.PointerDown() && module.TakeSettingsChanged(), "resizing retains the final one-pixel adjustment without snapping");
            const auto pixelSize = module.state_.bounds; module.ToggleMinimized(); auto pixelStore = settings_store::FromIniText(""); module.Save(pixelStore); module.ToggleMinimized();
            satchel_module pixelRestored; pixelRestored.Initialize(nullptr, &source, &cache, &actions); pixelRestored.ApplyTextDefaults(ui::theme::CreateDefault());
            pixelRestored.Load(pixelStore); pixelRestored.UpdateSize(); pixelRestored.ToggleMinimized();
            Check(closeEnough(module.state_.bounds.width, pixelSize.width) && closeEnough(module.state_.bounds.height, pixelSize.height) &&
                closeEnough(pixelRestored.state_.bounds.width, pixelSize.width) && closeEnough(pixelRestored.state_.bounds.height, pixelSize.height),
                "exact expanded pixels survive minimization, saving and runtime replacement");
            const auto largeGrip = module.ResizeBounds(); const auto largeX = largeGrip.x + largeGrip.width * 0.5f, largeY = largeGrip.y + largeGrip.height * 0.5f;
            module.HandleMouse(WM_LBUTTONDOWN, 0, largeX, largeY, false); module.HandleMouse(WM_MOUSEMOVE, 0, largeX + 10000, largeY + 10000, false);
            module.HandleMouse(WM_LBUTTONUP, 0, largeX + 10000, largeY + 10000, false);
            Check(module.Columns() > 0 && module.Rows() > 0 && module.Columns() * pitch - module.spacing_ <= module.GridBounds().width + 0.1f &&
                module.Rows() * pitch - module.spacing_ <= module.GridBounds().height + 0.1f && module.state_.bounds.width >= module.state_.minimum_width && module.state_.bounds.height >= module.state_.minimum_height &&
                module.state_.bounds.x + module.state_.bounds.width <= module.viewport_.width && module.state_.bounds.y + module.state_.bounds.height <= module.viewport_.height,
                "oversized resizing fits whole cells inside pixel dimensions bounded by the viewport");

            module.columns_ = 7; module.rows_ = 5; module.width_ = module.height_ = 0; module.state_.bounds.x = 80; module.state_.bounds.y = 60; module.UpdateSize(); module.TakeSettingsChanged();
            const auto sameBounds = [&](const ui::rect& a, const ui::rect& b) {
                return closeEnough(a.x, b.x) && closeEnough(a.y, b.y) && closeEnough(a.width, b.width) && closeEnough(a.height, b.height);
            };
            module.BeginControllerBrowse();
            Check(module.navigation_focus_, "deliberate controller browsing enables navigation focus");
            for (const auto region : {satchel_module::region::move, satchel_module::region::resize})
            {
                const auto original = module.state_.bounds; const auto columns = module.Columns(), rows = module.Rows();
                module.region_ = region; module.HandleControllerAction(controller_action::confirm, false);
                module.HandleControllerAction(controller_action::right, false); module.HandleControllerAction(controller_action::down, false);
                Check(region == satchel_module::region::move ? closeEnough(module.state_.bounds.x, original.x + 5) && closeEnough(module.state_.bounds.y, original.y + 5) :
                    closeEnough(module.state_.bounds.width, original.width + 5) && closeEnough(module.state_.bounds.height, original.height + 5),
                    "controller directions adjust move and resize gestures by five pixels");
                module.HandleControllerAction(controller_action::cancel, false);
                Check(sameBounds(module.state_.bounds, original) && module.Columns() == columns && module.Rows() == rows &&
                    module.Opened() && !module.TakeSettingsChanged(), "controller Cancel restores window bounds and grid dimensions");
                module.HandleControllerAction(controller_action::confirm, false);
                module.HandleControllerAction(controller_action::right, false); module.HandleControllerAction(controller_action::down, false);
                const auto preview = module.state_.bounds; const auto previewColumns = module.Columns(), previewRows = module.Rows();
                module.HandleControllerAction(controller_action::confirm, false);
                Check(sameBounds(module.state_.bounds, preview) && module.Columns() == previewColumns && module.Rows() == previewRows && module.TakeSettingsChanged(),
                    "controller Confirm commits the adjusted window gesture");
            }
            for (const bool resizeGesture : {false, true})
            {
                const auto original = module.state_.bounds; const auto columns = module.Columns(), rows = module.Rows();
                const auto handle = resizeGesture ? module.ResizeBounds() : module.TitleBounds();
                const auto x = handle.x + handle.width * 0.5f, y = handle.y + handle.height * 0.5f;
                module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false); module.HandleMouse(WM_MOUSEMOVE, 0, x + pitch, y + pitch, false);
                Check(module.PointerDown() && !sameBounds(module.state_.bounds, original), "pointer gesture has an active preview before suspension");
                module.SuspendInput();
                Check(sameBounds(module.state_.bounds, original) && module.Columns() == columns && module.Rows() == rows &&
                    !module.PointerDown() && !module.InputFocused() && !module.navigation_focus_ && !module.TakeSettingsChanged(), "suspension rolls back window gestures and clears pointer ownership");
            }
            Check(actions.Outcome() == satchel_action_outcome::none && module.dialog_ == satchel_module::dialog::none,
                "dropdown, window movement and resizing never submit item actions");
        }

        static auto CompactInteraction(void) -> void
        {
            satchel_source source; satchel_cache cache; satchel_actions actions;
            auto snapshot = Fixture(); snapshot.bags[0].items.clear();
            for (uint8_t bag = 0; bag < snapshot.bags.size(); ++bag)
            {
                if (bag == 3) continue;
                snapshot.bags[bag].capacity = 80; snapshot.bags[bag].ready = snapshot.bags[bag].accessible = true;
            }
            for (uint8_t slot = 1; slot <= 80; ++slot)
            {
                satchel_item item; item.id = 100 + slot; item.bag = 0; item.slot = slot; item.count = 1; item.generation = slot;
                snapshot.bags[0].items.push_back(item);
            }
            satchel_data_test::Set(source, snapshot);
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 80; module.state_.bounds.y = 60; module.columns_ = 5; module.rows_ = 2; module.width_ = module.height_ = 0;
            module.Open(true); module.Update(1000); module.UpdateSize();
            const auto click = [&](const ui::rect& bounds) {
                const auto x = bounds.x + bounds.width * 0.5f, y = bounds.y + bounds.height * 0.5f;
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false) && module.HandleMouse(WM_LBUTTONUP, 0, x, y, false), "compact control owns its pointer click");
            };
            const auto within = [](const ui::rect& inner, const ui::rect& outer) {
                return inner.y >= outer.y && inner.y + inner.height <= outer.y + outer.height;
            };
            Check(module.SidebarScrollbar().Visible() && module.GridScrollbar().Visible() && module.PageCount() == 8,
                "compact bags and item pages expose independent overflow scrollbars");
            const auto sidebar = module.SidebarBounds(), grid = module.GridBounds();
            const auto wheelDown = MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA));
            Check(module.HandleMouse(WM_MOUSEWHEEL, wheelDown, sidebar.x + 20, sidebar.y + 20, false) && module.sidebar_scroll_ > 0 && module.bag_ == 0,
                "sidebar wheel scrolls clipped rows without selecting a bag");
            Check(module.HandleMouse(WM_MOUSEWHEEL, wheelDown, grid.x + 20, grid.y + 20, false) && module.page_ == 1 && module.CurrentCell()->item.slot == 11,
                "grid wheel advances one item page and its focused identity");
            for (const auto area : {satchel_module::scroll_area::bags, satchel_module::scroll_area::items})
            {
                const auto bar = area == satchel_module::scroll_area::bags ? module.SidebarScrollbar() : module.GridScrollbar();
                const auto thumb = bar.Thumb(); const auto x = thumb.x + thumb.width * 0.5f, y = thumb.y + thumb.height * 0.5f;
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false) && module.scrolling_ == area, "scrollbar thumb begins pointer capture");
                module.HandleMouse(WM_MOUSEMOVE, 0, x, bar.track.y + bar.track.height + thumb.height, false);
                module.HandleMouse(WM_LBUTTONUP, 0, x, bar.track.y + bar.track.height + thumb.height, false);
                Check(!module.PointerDown() && module.scrolling_ == satchel_module::scroll_area::none &&
                    (area == satchel_module::scroll_area::bags ? module.sidebar_scroll_ == bar.Maximum() : module.page_ == static_cast<size_t>(bar.Maximum())),
                    "scrollbar drag reaches its last position and releases pointer ownership");
            }
            module.Scroll(satchel_module::scroll_area::bags, 0);
            const auto bagBefore = module.bag_; const ui::rect clippedRow{sidebar.x + 10, sidebar.y + sidebar.height + 2, 10, 4};
            click(clippedRow);
            Check(module.bag_ == bagBefore && module.dialog_ == satchel_module::dialog::none && !module.ItemAt(grid.x + 5, grid.y + grid.height + 5),
                "clipped sidebar rows and item cells have no hidden pointer targets");
            const auto hiddenSlip = module.SlipBounds();
            Check(hiddenSlip.y >= sidebar.y + sidebar.height && !module.HandleMouse(WM_LBUTTONDOWN, 0, hiddenSlip.x + 10, hiddenSlip.y + 10, false),
                "the offscreen storage-slip row cannot open its picker");

            module.BeginControllerBrowse(); module.region_ = satchel_module::region::bags;
            const auto tabs = module.VisibleBagTabs(); module.bag_ = satchel_bag_order[tabs[tabs.size() - 2] - 1];
            module.HandleControllerAction(controller_action::down, false);
            Check(module.bag_ == satchel_bag_order[tabs.back() - 1] && module.sidebar_scroll_ > 0 && within(module.FocusBounds(), module.SidebarBounds()),
                "controller bag navigation scrolls the last focused row fully into view");
            module.HandleControllerAction(controller_action::cycle_window, false); module.HandleControllerAction(controller_action::cycle_window, false);
            Check(module.region_ == satchel_module::region::slips && within(module.SlipBounds(), module.SidebarBounds()),
                "controller region navigation reveals the final storage-slip row");
            module.ChangeBag(0); const auto expanded = module.state_.bounds; const auto firstCell = module.CellBounds(0);
            const auto columns = module.Columns(), rows = module.Rows(); click(module.MinimizeBounds());
            Check(module.minimized_ && module.Opened() && module.state_.bounds.height < expanded.height && module.Columns() == columns && module.Rows() == rows &&
                module.GridBounds().width == 0 && module.SidebarBounds().width == 0 && module.SearchBounds().width == 0 && module.ResizeBounds().width == 0 &&
                !module.SidebarScrollbar().Visible() && !module.GridScrollbar().Visible(), "minimize retains the grid while hiding its body and scrollbars");
            Check(!module.InputFocused() && !module.PointerDown() && !module.HandleKey(VK_UP, 0),
                "mouse minimize releases keyboard and controller capture so movement remains native");
            for (const auto region : {satchel_module::region::grid, satchel_module::region::bags, satchel_module::region::slips, satchel_module::region::search, satchel_module::region::resize})
                Check(!module.RegionAvailable(region), "collapsed content regions cannot receive controller focus");
            for (const auto direction : {-1, 1})
                for (int step = 0; step < static_cast<int>(satchel_module::region::count); ++step)
                { module.CycleRegion(direction); Check(module.RegionAvailable(module.region_), "collapsed region cycling skips hidden controls"); }
            Check(!module.HandleMouse(WM_LBUTTONDOWN, 0, firstCell.x + 10, firstCell.y + 10, false) && !module.ItemAt(firstCell.x + 10, firstCell.y + 10),
                "minimized item coordinates do not retain pointer actions");
            const auto title = module.TitleBounds(); const auto x = title.x + 10, y = title.y + 10;
            module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false); module.HandleMouse(WM_MOUSEMOVE, 0, x + 20, y + 15, false); module.HandleMouse(WM_LBUTTONUP, 0, x + 20, y + 15, false);
            Check(module.minimized_ && module.state_.bounds.x == expanded.x + 20 && module.state_.bounds.y == expanded.y + 15, "the minimized title remains draggable");
            Check(!module.InputFocused() && !module.PointerDown() && !module.HandleKey(VK_UP, 0),
                "moving the minimized title releases movement controls on pointer release");
            auto store = settings_store::FromIniText(""); module.Save(store);
            satchel_module restored; restored.Initialize(nullptr, &source, &cache, &actions); restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(store); restored.UpdateSize();
            Check(restored.Opened() && restored.minimized_ && restored.Columns() == columns && restored.Rows() == rows && restored.state_.bounds.x == module.state_.bounds.x &&
                restored.state_.bounds.y == module.state_.bounds.y && !store.Preferences("").ReadBool("session.satchel", "minimized", false),
                "minimized state and position survive replacement while remaining outside portable preferences");
            module.BeginControllerBrowse();
            module.BeginControllerBrowse(); module.region_ = satchel_module::region::minimize; module.HandleControllerAction(controller_action::confirm, false);
            Check(!module.minimized_ && module.Columns() == columns && module.Rows() == rows && module.state_.bounds.height == expanded.height,
                "controller confirmation restores the retained expanded grid");
            click(module.MinimizeBounds()); click(module.MinimizeBounds());
            Check(!module.minimized_ && module.state_.bounds.height == expanded.height, "the same title button minimizes and restores the browser");
            for (int cycle = 0; cycle < 3; ++cycle)
            {
                click(module.MinimizeBounds()); click(module.MinimizeBounds()); click(module.MinimizeBounds());
                Check(module.minimized_ && !module.InputFocused() && !module.PointerDown() && !module.HandleKey(VK_UP, 0) && !module.HandleKey('W', 0),
                    "repeated mouse minimize and restore cycles leave native movement uncaptured");
                click(module.MinimizeBounds());
            }
            module.ToggleMinimized();
            {
                keyboard_restore restore; BYTE keys[256]{}; keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
                Check(::SetKeyboardState(keys) != FALSE && module.HandleKey('F', 0) && !module.minimized_ && module.KeyboardFocused(),
                    "Ctrl+F restores a minimized browser directly into search");
            }
            module.SuspendInput();
            Check(actions.Outcome() == satchel_action_outcome::none, "scrolling, focus and minimize never submit item actions");
        }

        static auto ForgivingReflow(void) -> void
        {
            satchel_source source; satchel_cache cache; satchel_actions actions;
            auto snapshot = Fixture(); snapshot.bags[0].capacity = 12; snapshot.bags[0].items.clear();
            for (uint8_t slot = 1; slot <= 12; ++slot)
            { satchel_item item; item.id = 100 + slot; item.bag = 0; item.slot = slot; item.count = 1; snapshot.bags[0].items.push_back(item); }
            satchel_data_test::Set(source, snapshot);
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 40; module.state_.bounds.y = 50;
            module.columns_ = 6; module.rows_ = 4; module.width_ = module.height_ = 0; module.Open(true); module.Update(1000); module.UpdateSize();
            const auto cell = module.icon_size_ + 8; const auto nominalWidth = 6 * cell + 5 * module.spacing_, nominalHeight = 4 * cell + 3 * module.spacing_;
            const auto resizeBody = [&](const float width, const float height) {
                const auto grid = module.GridBounds(); module.width_ += width - grid.width; module.height_ += height - grid.height; module.UpdateSize();
            };
            const auto aligned = [&] {
                const auto grid = module.GridBounds(), last = module.CellBounds(module.PageSize() - 1), item = module.CellBounds(11);
                const auto hit = module.ItemAt(item.x + cell * 0.5f, item.y + cell * 0.5f);
                Check(last.width == cell && last.height == cell && last.x + cell <= grid.x + grid.width + 0.1f && last.y + cell <= grid.y + grid.height + 0.1f &&
                    hit && *hit == 11, "compressed spacing retains full cells and aligned item hits");
            };
            Check(!module.GridScrollbar().Visible(), "a fully fitting grid leaves its scrollbar gutter available for items");
            resizeBody(nominalWidth - 1, nominalHeight - 1); aligned();
            for (int pass = 0; pass < 3; ++pass) module.UpdateSize();
            Check(module.Columns() == 6 && module.Rows() == 4, "small reductions preserve the prior grid consistently across repeated updates");
            resizeBody(6 * cell + 5 - 0.25f, 4 * cell + 3 - 0.25f); aligned();
            Check(module.Columns() == 5 && module.Rows() == 3, "shrinking below the one-pixel gap removes one column and row");
            resizeBody(nominalWidth - 0.25f, nominalHeight - 0.25f); module.UpdateSize();
            Check(module.Columns() == 5 && module.Rows() == 3, "widening below normal spacing thresholds preserves the smaller grid");
            resizeBody(nominalWidth, nominalHeight); aligned();
            Check(module.Columns() == 6 && module.Rows() == 4, "normal spacing thresholds add the next column and row");
            module.spacing_ = 0; module.columns_ = 6; module.rows_ = 4; module.width_ = module.height_ = 0; module.UpdateSize();
            Check(module.Columns() == 6 && module.Rows() == 4, "zero spacing fits touching full cells exactly");
            resizeBody(6 * cell - 0.25f, 4 * cell - 0.25f); aligned();
            Check(module.Columns() == 5 && module.Rows() == 3 && actions.Outcome() == satchel_action_outcome::none,
                "zero spacing drops cells as soon as they cannot fit without clipping");
        }

        static auto Ownership(void) -> void
        {
            keyboard_restore restore; BYTE keys[256]{}; Check(::SetKeyboardState(keys) != FALSE, "ownership checks use neutral keyboard modifiers");
            satchel_source source; satchel_cache cache; satchel_actions actions;
            auto snapshot = Fixture(); snapshot.bags[0].capacity = 40; satchel_data_test::Set(source, snapshot);
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 80; module.state_.bounds.y = 60;
            module.columns_ = 5; module.rows_ = 4; module.width_ = module.height_ = 0;
            const auto reset = [&] {
                module.SuspendInput(); module.Open(true); if (module.minimized_) module.ToggleMinimized();
                module.character_.clear(); module.query_.clear(); module.bag_ = 0; module.slip_ = -1; module.page_ = module.focus_cell_ = 0;
                module.now_ = 1000; module.last_click_ = 0; module.Rebuild(); module.UpdateSize(); actions = {};
            };
            const auto click = [&](const ui::rect& at) {
                const auto x = at.x + at.width * 0.5f, y = at.y + at.height * 0.5f;
                module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false); module.HandleMouse(WM_LBUTTONUP, 0, x, y, false);
            };
            const auto rightClick = [&](const ui::rect& at) {
                module.HandleMouse(WM_RBUTTONDOWN, 0, at.x + 10, at.y + 10, false); module.HandleMouse(WM_RBUTTONUP, 0, at.x + 10, at.y + 10, false);
            };
            const auto released = [&] {
                Check(!module.InputFocused() && !module.KeyboardFocused() && !module.PointerDown() && !module.navigation_focus_ && !module.keyboard_ && !module.controller_ &&
                    !module.HandleKey('W', 0), "finished mouse interactions release Satchel ownership and pass native movement keys");
            };
            const ui::rect outside{4, 4, 1, 1};
            for (const auto popup : {satchel_module::dialog::characters, satchel_module::dialog::slips, satchel_module::dialog::actions})
                for (int exit = 0; exit < 3; ++exit)
                {
                    reset();
                    if (popup == satchel_module::dialog::characters) click(module.CharacterBounds());
                    else if (popup == satchel_module::dialog::slips) click(module.SlipBounds());
                    else rightClick(module.CellBounds(0));
                    Check(module.dialog_ == popup && module.InputFocused() && !module.keyboard_ && !module.controller_, "mouse popup owns input only while open");
                    if (exit == 0) click(outside);
                    else if (exit == 1) module.HandleKey(VK_ESCAPE, 0);
                    else click(module.PickBounds(popup == satchel_module::dialog::actions ? 2 : 0));
                    released();
                }
            reset(); rightClick(module.CellBounds(0)); click(module.PickBounds(4));
            Check(module.dialog_ == satchel_module::dialog::price && module.KeyboardFocused(), "numeric item editor has temporary text ownership");
            click(module.PickBounds(1)); released();

            for (int destination = 0; destination < 8; ++destination)
            {
                reset(); click(module.SearchBounds());
                Check(module.KeyboardFocused() && !module.keyboard_ && !module.controller_, "mouse search has editor ownership without persistent browsing");
                const ui::rect targets[]{module.HelpBounds(), module.BagBounds(1), module.CellBounds(3), module.GridScrollbar().Thumb(),
                    module.TitleBounds(), module.MinimizeBounds(), module.CharacterBounds(), outside};
                Check(targets[destination].width > 0, "search click-away target is visible");
                click(targets[destination]);
                if (module.dialog_ != satchel_module::dialog::none)
                { Check(!module.KeyboardFocused() && !module.keyboard_ && !module.controller_, "search transfers only temporary ownership to a mouse popup"); click(outside); }
                released();
            }
            reset(); click(module.SearchBounds()); module.HandleKey(VK_ESCAPE, 0); released();
            reset(); click(module.SearchBounds()); module.HandleKey(VK_RETURN, 0);
            Check(module.InputFocused() && module.keyboard_ && module.navigation_focus_ && !module.KeyboardFocused(), "search Enter deliberately enters visible keyboard browsing");
            module.HandleKey(VK_ESCAPE, 0); released();

            reset(); click(module.CellBounds(0)); released(); click(module.CellBounds(3)); released(); rightClick(module.CellBounds(3)); released();
            module.BeginControllerBrowse(); const auto grid = module.GridBounds();
            module.HandleMouse(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA)), grid.x + 10, grid.y + 10, false); released();
            reset(); const auto first = module.CellBounds(0), second = module.CellBounds(1);
            module.HandleMouse(WM_LBUTTONDOWN, 0, first.x + 10, first.y + 10, false);
            module.HandleMouse(WM_MOUSEMOVE, 0, second.x + 10, second.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, second.x + 10, second.y + 10, false); released();
            for (const bool resize : {false, true})
            {
                reset(); const auto at = resize ? module.ResizeBounds() : module.TitleBounds();
                module.HandleMouse(WM_LBUTTONDOWN, 0, at.x + 8, at.y + 8, false);
                Check(module.InputFocused() && !module.keyboard_ && !module.controller_, "window gesture ownership is temporary");
                module.HandleMouse(WM_MOUSEMOVE, 0, at.x + 15, at.y + 19, false); module.HandleMouse(WM_LBUTTONUP, 0, at.x + 15, at.y + 19, false); released();
            }
            for (const auto region : {satchel_module::region::character, satchel_module::region::search})
            {
                reset(); module.BeginControllerBrowse(); module.region_ = region; module.HandleControllerAction(controller_action::confirm, false);
                Check(module.InputFocused(), "controller editor or popup retains explicit ownership"); module.EndControllerBrowse(); released();
            }
            reset(); module.BeginControllerBrowse(); module.region_ = satchel_module::region::character; module.HandleControllerAction(controller_action::confirm, false);
            const auto popup = module.DialogBounds(); module.HandleMouse(WM_LBUTTONDOWN, 0, popup.x + 2, popup.y + 2, false); module.EndControllerBrowse();
            Check(module.dialog_ == satchel_module::dialog::characters && !module.controller_ && !module.keyboard_, "mouse takeover preserves its popup when controller focus is released");
            module.HandleMouse(WM_LBUTTONUP, 0, popup.x + 2, popup.y + 2, false); module.EndControllerBrowse();
            Check(module.dialog_ == satchel_module::dialog::characters, "repeated controller cleanup preserves a mouse-owned popup"); click(outside); released();
            reset(); click(module.SearchBounds()); module.SuspendInput(); released();
            reset(); rightClick(module.CellBounds(0)); module.SuspendInput(); released();
        }

        static auto RuntimeOwnership(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>();
            runtime->inventory_source_.state_.available = true; satchel_data_test::Set(runtime->satchel_source_, Fixture());
            auto& module = runtime->satchel_; module.Initialize(nullptr, &runtime->satchel_source_, &runtime->satchel_cache_, &runtime->satchel_actions_);
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.state_.bounds.x = 80; module.state_.bounds.y = 60;
            module.Open(true); module.Update(1000); module.UpdateSize();
            Check(runtime->SatchelInputAvailable() && !runtime->KeyboardFocused(), "idle visible Satchel leaves the runtime keyboard gate open");
            const auto click = [&](const ui::rect& at) {
                const auto x = at.x + at.width * 0.5f, y = at.y + at.height * 0.5f;
                runtime->HandleSatchelMouse(WM_LBUTTONDOWN, 0, x, y, false); runtime->HandleSatchelMouse(WM_LBUTTONUP, 0, x, y, false);
            };
            click(module.CharacterBounds()); Check(runtime->KeyboardFocused(), "runtime keyboard gate follows an open profile popup");
            click({4, 4, 1, 1}); Check(!runtime->KeyboardFocused(), "popup dismissal releases the actual runtime movement gate");
            click(module.SearchBounds()); Check(runtime->KeyboardFocused(), "search typing owns the runtime keyboard gate");
            Check(runtime->HandleSatchelKey(VK_ESCAPE, 0, false) && !runtime->KeyboardFocused() && runtime->satchel_key_release_[VK_ESCAPE],
                "search Escape releases movement while retaining only its matching key release");
            Check(runtime->HandleSatchelKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false) && !runtime->satchel_key_release_[VK_ESCAPE] &&
                !runtime->HandleSatchelKey('W', 0, false), "owned Escape release drains without consuming a new movement press");
            const auto title = module.TitleBounds(); runtime->HandleSatchelMouse(WM_LBUTTONDOWN, 0, title.x + 10, title.y + 10, false);
            Check(runtime->KeyboardFocused(), "active pointer window gesture has temporary runtime ownership");
            runtime->SuspendSatchelInput();
            Check(!runtime->KeyboardFocused() && (runtime->satchel_pointer_release_ & MK_LBUTTON) != 0 &&
                runtime->HandleSatchelMouse(WM_LBUTTONUP, 0, -1, -1, false) && runtime->satchel_pointer_release_ == 0,
                "suspension releases movement and drains the owned pointer release once");
            runtime->satchel_pointer_release_ = MK_LBUTTON;
            Check(!runtime->HandleMouseMessage(WM_LBUTTONDOWN, 0, MAKELPARAM(4, 4), false) && runtime->satchel_pointer_release_ == 0 &&
                !runtime->HandleMouseMessage(WM_LBUTTONUP, 0, MAKELPARAM(4, 4), false), "fresh native pointer presses clear a stale Satchel release drain");
        }

        static auto Run(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            satchel_source source; satchel_cache cache; satchel_actions actions;
            auto snapshot = Fixture(); satchel_data_test::Set(source, snapshot);
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(theme);
            module.state_.bounds = {40, 50, 576, 456}; module.Open(true); module.Update(1000);
            Check(module.cells_.size() == 20 && module.CurrentCell(), "settled bag includes empty presentation cells");
            {
                keyboard_restore restore;
                BYTE keys[256]{};
                Check(::SetKeyboardState(keys) != FALSE, "clear thread-local shortcut modifiers");
                Check(!module.HandleKey('F', 0) && !module.InputFocused(), "ordinary typing leaves an unfocused Satchel unclaimed");
                keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
                Check(::SetKeyboardState(keys) != FALSE, "set thread-local search shortcut modifiers");
                Check(module.HandleKey('F', 0x40000000) && !module.KeyboardFocused(), "held search shortcut cannot acquire focus");
                Check(module.HandleKey('F', 0) && module.KeyboardFocused(), "Ctrl+F focuses search in a newly opened window");
                module.editor_.Insert("retained query"); module.query_ = module.editor_.Editor().Text();
                module.HandleKey('F', 0);
                Check(module.editor_.Editor().SelectedText() == "retained query", "Ctrl+F selects the current search for replacement");
                Check(!module.HandleKey('F', static_cast<LPARAM>(0x80000000u)) && module.editor_.Editor().Text() == "retained query",
                    "key releases cannot reactivate search or insert text");
                module.query_.clear(); module.SuspendInput();
            }
            const auto click = [&](const ui::rect& bounds) {
                const auto x = bounds.x + bounds.width * 0.5f, y = bounds.y + bounds.height * 0.5f;
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false), "mouse press belongs to the browser");
                Check(module.HandleMouse(WM_LBUTTONUP, 0, x, y, false), "mouse release belongs to the browser");
            };
            {
                keyboard_restore restore; BYTE keys[256]{};
                Check(::SetKeyboardState(keys) != FALSE, "clear search-cancel modifiers");
                click(module.SearchBounds());
                Check(module.KeyboardFocused() && module.InputFocused(), "clicking search captures text input");
                module.editor_.Insert("saved query"); module.query_ = module.editor_.Editor().Text();
                Check(module.HandleKey(VK_ESCAPE, 0) && module.Opened() && !module.KeyboardFocused() && !module.InputFocused() &&
                    !module.navigation_focus_ && module.query_ == "saved query", "Escape leaves search, retains its query and releases all browsing focus");
                Check(!module.HandleKey(VK_UP, 0) && !module.HandleKey('W', 0), "movement keys return to the game after leaving search");
                keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
                Check(::SetKeyboardState(keys) != FALSE && module.HandleKey('F', 0) && module.HandleKey(VK_ESCAPE, 0) && !module.InputFocused(),
                    "Escape also releases search acquired through Ctrl+F");
                module.query_.clear(); module.Rebuild();
            }
            const auto openDrop = [&] {
                const auto at = module.CellBounds(0);
                module.HandleMouse(WM_RBUTTONDOWN, 0, at.x + 10, at.y + 10, false);
                module.HandleMouse(WM_RBUTTONUP, 0, at.x + 10, at.y + 10, false);
                Check(module.dialog_ == satchel_module::dialog::actions, "mouse context action uses the shared menu");
                click(module.PickBounds(5));
                Check(module.dialog_ == satchel_module::dialog::drop && module.pick_index_ == 0, "drop opens with Cancel selected");
            };
            openDrop();
            module.HandleControllerAction(controller_action::confirm, true);
            Check(module.dialog_ == satchel_module::dialog::drop && actions.Outcome() == satchel_action_outcome::none,
                "held confirmation cannot activate a freshly opened destructive dialog");
            module.HandleControllerAction(controller_action::confirm, false);
            Check(module.dialog_ == satchel_module::dialog::none && actions.Outcome() == satchel_action_outcome::none,
                "initial fresh confirmation takes the default Cancel action");
            openDrop();
            module.HandleControllerAction(controller_action::down, false);
            module.HandleControllerAction(controller_action::confirm, true);
            Check(actions.Outcome() == satchel_action_outcome::none, "held confirmation cannot dispatch Drop after navigation");
            module.HandleControllerAction(controller_action::confirm, false);
            Check(actions.Outcome() == satchel_action_outcome::rejected && module.dialog_ == satchel_module::dialog::none,
                "explicit Drop reaches the validated service, which rejects an absent native core");

            actions = {};
            auto saved = snapshot; saved.identity = "world-2-456"; saved.read_only = true;
            for (auto& item : saved.bags[0].items) item.read_only = true;
            satchel_data_test::Set(cache, saved); module.character_ = saved.identity; module.Rebuild();
            module.OpenActions(module.cells_[0].item); module.Transfer(module.cells_[0].item, 5);
            Check(module.ReadOnly() && module.dialog_ == satchel_module::dialog::none && actions.Outcome() == satchel_action_outcome::none,
                "saved characters expose no item dispatch path");
            module.character_.clear(); module.slip_ = 0; module.OpenActions(snapshot.bags[0].items[0]); module.Transfer(snapshot.bags[0].items[0], 5);
            Check(module.ReadOnly() && actions.Outcome() == satchel_action_outcome::none, "storage-slip views expose no item dispatch path");
            module.slip_ = -1; module.Rebuild(); module.controller_ = true;
            module.focus_cell_ = 0; module.MoveGrid(0, 1);
            Check(module.focus_cell_ == 1 && module.CurrentCell(), "vertical navigation escapes rows containing only empty slots");
            const auto focusedKey = module.CurrentCell()->key;
            std::reverse(snapshot.bags[0].items.begin(), snapshot.bags[0].items.end()); ++snapshot.generation;
            satchel_data_test::Set(source, snapshot); module.Rebuild();
            Check(module.CurrentCell() && module.CurrentCell()->key == focusedKey, "focus follows stable item identity across reordered records");
            for (auto& item : snapshot.bags[0].items) if (item.id == 101) { item.slot = 9; item.count = 8; ++item.generation; }
            ++snapshot.generation; satchel_data_test::Set(source, snapshot); module.Rebuild();
            Check(module.CurrentCell() && module.CurrentCell()->key == focusedKey, "slot compaction and stack quantity changes preserve visual identity");
            std::erase_if(snapshot.bags[0].items, [](const auto& item) { return item.id == 101; });
            ++snapshot.generation; satchel_data_test::Set(source, snapshot); module.Rebuild();
            Check(module.CurrentCell() && module.CurrentCell()->item.id != 101, "removed items cannot leave focus on an empty presentation cell");
            auto variants = Fixture(); variants.bags[0].items[1].id = variants.bags[0].items[0].id;
            variants.bags[0].items[1].extra[2] = 3; satchel_data_test::Set(source, variants);
            Check(module.ItemKey(variants.bags[0].items[0]) != module.ItemKey(variants.bags[0].items[1]), "different augment payloads have distinct visual identities");
            variants.bags[0].items[1].extra = {}; satchel_data_test::Set(source, variants);
            Check(module.ItemKey(variants.bags[0].items[0]) != module.ItemKey(variants.bags[0].items[1]), "identical stacks retain deterministic occurrence identities");
            module.Rebuild(); module.BeginSearch(); module.OpenPicker(satchel_module::dialog::characters);
            module.picks_.push_back({"Third", "world-3-789", 0}); module.pick_index_ = 0;
            module.HandleControllerAction(controller_action::left, false);
            Check(module.pick_index_ < module.picks_.size(), "picker paging handles fewer entries than one full page");
            module.BeginSearch(); module.pointer_down_ = true; module.SuspendInput();
            Check(!module.InputFocused() && !module.KeyboardFocused() && !module.PointerDown() && module.dialog_ == satchel_module::dialog::none,
                "suspension releases modal, typing, controller and pointer ownership");

            module.Open(true); module.text_[0].style.glow.enabled = true; module.text_[0].style.stroke.enabled = true;
            module.text_[1].style.shadow.enabled = true; module.text_[2].style.top_color = 0xFFAABBCC; module.text_[2].style.bottom_color = 0xFF112233;
            module.text_[3].offset_x = 2; module.columns_ = 7; module.rows_ = 4; module.width_ = module.height_ = 0; module.UpdateSize(); module.override_satchel_ = true;
            auto store = settings_store::FromIniText(""); module.Save(store);
            satchel_module restored; restored.Initialize(nullptr, &source, &cache, &actions); restored.ApplyTextDefaults(theme); restored.Load(store);
            Check(restored.Opened() && restored.state_.bounds.x == 40 && restored.state_.bounds.y == 50 && restored.columns_ == 7 && restored.rows_ == 4 && restored.OverridesSatchel(),
                "session open state, saved position and browser settings round-trip");
            Check(restored.text_[0].style.glow.enabled && restored.text_[0].style.stroke.enabled && restored.text_[1].style.shadow.enabled &&
                restored.text_[2].style.top_color == 0xFFAABBCC && restored.text_[2].style.bottom_color == 0xFF112233 && restored.text_[3].offset_x == 2,
                "shared text effects and gradients round-trip");
            Check(!store.Preferences("").ReadBool("session.satchel", "open", false), "open state remains outside portable preferences");
            for (size_t option = 0; option < restored.OptionCount(); ++option)
            {
                size_t owners{}; for (size_t section = 0; section < restored.ConfigSections().size(); ++section) owners += restored.ConfigSectionContains(section, option);
                Check(owners == 1 && restored.Option(option).label, "every browser control has exactly one configuration topic");
            }
            WindowInteraction();
            CompactInteraction();
            ForgivingReflow();
            Ownership();
            RuntimeOwnership();
        }
    };
}

auto main(void) -> int
{
    try { xiui::satchel_ui_test::Run(); std::puts("Satchel UI checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Satchel UI check failed: %s\n", error.what()); return 1; }
}
