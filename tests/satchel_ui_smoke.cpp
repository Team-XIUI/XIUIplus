#include "modules/satchel/satchel_module.hpp"
#include "modules/satchel/satchel_source.hpp"
#include "modules/satchel/satchel_cache.hpp"
#include "modules/satchel/satchel_actions.hpp"
#include "hud_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
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
        static auto ContextMenu(void) -> void
        {
            satchel_source source; satchel_cache cache; satchel_actions actions;
            satchel_data_test::Set(source, Fixture());
            satchel_module module; module.Initialize(nullptr, &source, &cache, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 80; module.state_.bounds.y = 60;
            module.Open(true); module.Update(1000); module.UpdateSize();
            const auto click = [&](const ui::rect& at) {
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, at.x + 10, at.y + 10, false) &&
                    module.HandleMouse(WM_LBUTTONUP, 0, at.x + 10, at.y + 10, false), "context menu rows own matching press and release");
            };
            const auto item = module.CellBounds(0); const float x = item.x + 12, y = item.y + 12;
            module.HandleMouse(WM_RBUTTONDOWN, 0, x, y, false); module.HandleMouse(WM_RBUTTONUP, 0, x, y, false);
            const auto menu = module.DialogBounds();
            Check(module.dialog_ == satchel_module::dialog::actions && menu.width == 252 && menu.x == x + 4 && menu.y == y + 4,
                "right-click opens a compact item context menu beside the pointer");
            for (size_t i = 0; i < module.picks_.size(); ++i)
            {
                const auto row = module.PickBounds(i);
                Check(row.x >= menu.x && row.y >= menu.y && row.x + row.width <= menu.x + menu.width && row.y + row.height <= menu.y + menu.height,
                    "all context actions and separators fit inside the popup");
            }
            const auto split = module.PickBounds(1), use = module.PickBounds(2);
            module.HandleMouse(WM_LBUTTONDOWN, 0, split.x + 10, split.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, use.x + 10, use.y + 10, false);
            Check(module.dialog_ == satchel_module::dialog::actions && actions.Outcome() == satchel_action_outcome::none,
                "pressing a disabled action cannot activate another row on release");
            const auto identity = module.action_identity_;
            click(module.PickBounds(0));
            Check(module.dialog_ == satchel_module::dialog::destinations && module.ItemMenuOpen() && module.picks_[0].label == "Back",
                "bag destinations use the same anchored menu with a Back row");
            module.HandleKey(VK_ESCAPE, 0);
            Check(module.dialog_ == satchel_module::dialog::actions && module.action_identity_ == identity && module.pick_index_ == 0,
                "keyboard Back restores the parent menu without recapturing the item identity");
            click(module.PickBounds(0)); click(module.PickBounds(0));
            Check(module.dialog_ == satchel_module::dialog::actions, "pointer Back restores item actions");
            module.pick_index_ = 2; module.HandleControllerAction(controller_action::up, false);
            Check(module.pick_index_ == 0, "context menu navigation skips unavailable actions");
            module.HandleControllerAction(controller_action::cancel, false);
            module.BeginControllerBrowse(); module.focus_cell_ = 1; module.region_ = satchel_module::region::grid;
            const auto focused = module.CellBounds(1); module.HandleControllerAction(controller_action::confirm, false);
            Check(module.dialog_ == satchel_module::dialog::actions && module.DialogBounds().x == focused.x + focused.width + 4,
                "controller activation anchors the context menu to the focused item");
            module.HandleControllerAction(controller_action::cancel, false);
            module.state_.transform = {1.5f, 1.25f, -40, -20};
            module.viewport_ = module.state_.transform.Inverse(ui::rect{0, 0, 1280, 900});
            module.OpenActions(module.cells_[0].item, ui::point{module.state_.transform.InverseX(1274), module.state_.transform.InverseY(894)});
            const auto edge = module.DialogBounds();
            Check(edge.x >= 8 && edge.y >= 8 && edge.x + edge.width <= 1272 && edge.y + edge.height <= 892,
                "context menus flip and clamp to the screen with HUD scaling");
            module.HandleMouse(WM_LBUTTONDOWN, 0, 4, 4, false);
            Check(module.dialog_ == satchel_module::dialog::none && module.PointerDown(), "outside press dismisses the menu while retaining its owned release");
            module.HandleMouse(WM_LBUTTONUP, 0, 4, 4, false);
            Check(!module.PointerDown() && !module.InputFocused(), "outside release returns pointer and keyboard ownership to gameplay");
            module.state_.transform = {}; module.viewport_ = {0, 0, 1280, 900};
            module.OpenActions(module.cells_[0].item); click(module.PickBounds(5));
            Check(module.dialog_ == satchel_module::dialog::drop && module.pick_index_ == 0 && actions.Outcome() == satchel_action_outcome::none,
                "Drop still requires a separate Cancel-first confirmation");
            click(module.PickBounds(0));
            Check(module.dialog_ == satchel_module::dialog::none && actions.Outcome() == satchel_action_outcome::none,
                "cancelling the destructive prompt does not submit an item action");
        }
        static auto EmptySlotReorder(void) -> void
        {
            const auto path = std::filesystem::temp_directory_path() / ("xiui-empty-slots-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            satchel_source source; satchel_actions actions; satchel_cache cache; cache.Initialize(path.string());
            auto snapshot = Fixture(); snapshot.character_id = 123;
            satchel_data_test::Set(source, snapshot); satchel_data_test::Set(cache, snapshot);
            const auto setup = [&](satchel_module& module, satchel_cache& layout) {
                module.Initialize(nullptr, &source, &layout, &actions); module.ApplyTextDefaults(ui::theme::CreateDefault());
                module.viewport_ = {0, 0, 1280, 900}; module.state_.bounds.x = 80; module.state_.bounds.y = 60;
                module.columns_ = 5; module.rows_ = 4; module.Open(true); module.Update(1000); module.UpdateSize();
            };
            const auto drag = [](satchel_module& module, const size_t from, const size_t to) {
                const auto a = module.CellBounds(from), b = module.CellBounds(to);
                Check(module.HandleMouse(WM_LBUTTONDOWN, 0, a.x + 10, a.y + 10, false) &&
                    module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, b.x + 10, b.y + 10, false) &&
                    module.HandleMouse(WM_LBUTTONUP, 0, b.x + 10, b.y + 10, false), "custom-order drag owns its complete pointer gesture");
            };
            satchel_module module; setup(module, cache);
            const auto first = module.cells_[0].key, second = module.cells_[1].key, third = module.cells_[2].key;
            cache.WriteOrder(snapshot.identity, 0, {third, second, first}); module.rebuild_ = true; module.Update(1100);
            Check(module.cells_[0].key == third && module.cells_[1].key == second && module.cells_[2].key == first,
                "legacy occupied-only layouts retain their custom order");
            drag(module, 0, 8);
            Check(module.cells_[0].empty && module.cells_[8].key == third && module.cells_[1].key == second && module.cells_[2].key == first &&
                cache.ReadOrder(snapshot.identity, 0).size() == 20, "empty-slot drops move only the selected item and save explicit empty positions");
            snapshot.bags[0].items[2].slot = 9; ++snapshot.generation; satchel_data_test::Set(source, snapshot); module.Update(1200);
            Check(module.cells_[0].empty && module.cells_[8].key == third && module.cells_[8].item.slot == 9,
                "inventory refresh and native slot compaction preserve the chosen empty destination");
            cache.Stop(); satchel_cache restoredCache; restoredCache.Initialize(path.string());
            satchel_module restored; setup(restored, restoredCache);
            Check(restored.cells_[0].empty && restored.cells_[8].key == third && restored.cells_[1].key == second && restored.cells_[2].key == first,
                "empty positions and moved items survive a cache reload");
            satchel_item added; added.id = 200; added.bag = 0; added.slot = 4; added.count = 1;
            snapshot.bags[0].items.push_back(added); ++snapshot.generation; satchel_data_test::Set(source, snapshot); restored.Update(1300);
            Check(restored.cells_[0].item.id == 200 && restored.cells_[8].key == third,
                "new items fill available gaps without displacing saved items");
            drag(restored, 8, 1); restored.rebuild_ = true; restored.Update(1400);
            Check(restored.cells_[1].key == third && std::count_if(restored.cells_.begin(), restored.cells_.end(), [](const auto& cell) { return !cell.empty; }) == 4,
                "occupied destinations still reorder without losing or duplicating items in a layout with gaps");
            const auto order = restoredCache.ReadOrder(snapshot.identity, 0);
            const auto start = restored.CellBounds(1), target = restored.CellBounds(9);
            restored.HandleMouse(WM_LBUTTONDOWN, 0, start.x + 10, start.y + 10, false);
            restored.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            restored.SuspendInput();
            Check(restored.cells_[1].key == third && restored.cells_[9].empty && restoredCache.ReadOrder(snapshot.identity, 0) == order &&
                actions.Outcome() == satchel_action_outcome::none, "cancelled previews preserve layout and never submit native item actions");
            restoredCache.Stop(); std::filesystem::remove(path / (snapshot.identity + ".ini")); std::filesystem::remove(path);
        }
        static auto PositionLock(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->satchel_;
            runtime->inventory_source_.state_.available = true; satchel_data_test::Set(runtime->satchel_source_, Fixture());
            module.Initialize(nullptr, &runtime->satchel_source_, &runtime->satchel_cache_, &runtime->satchel_actions_);
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.viewport_ = {0, 0, 1280, 900};
            module.state_.bounds.x = 80; module.state_.bounds.y = 60; module.Open(true); module.Update(1000); module.UpdateSize();
            Check(runtime->SatchelInputAvailable(true) && module.CanMove(), "Satchel is interactive and movable with global HUD editing off");
            *module.Option(satchel_module::lock_position).toggle = true; module.OnOptionChanged(satchel_module::lock_position);
            Check(!module.CanMove() && !module.ConfigScaleEditable() && module.EditGrip() == edit_grip_kind::none &&
                !module.RegionAvailable(satchel_module::region::move) && !module.RegionAvailable(satchel_module::region::resize),
                "Satchel's independent lock disables pointer and controller layout gestures");
            const auto original = module.state_.bounds;
            for (const auto handle : {module.TitleBounds(), module.ResizeBounds()})
            {
                module.HandleMouse(WM_LBUTTONDOWN, 0, handle.x + 8, handle.y + 8, false);
                module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, handle.x + 48, handle.y + 38, false);
                module.HandleMouse(WM_LBUTTONUP, 0, handle.x + 48, handle.y + 38, false);
                Check(module.state_.bounds.x == original.x && module.state_.bounds.y == original.y && module.state_.bounds.width == original.width &&
                    module.state_.bounds.height == original.height && !module.PointerDown(), "locked title and resize drags leave the window in place and drain release");
            }
            runtime->editing_ = runtime->config_.edit_mode = true;
            const auto movable = runtime->ControllerMovableModules();
            Check(!runtime->ModuleEditControlsVisible(module) && std::find(movable.begin(), movable.end(), &module) == movable.end() &&
                !runtime->CenterModuleLayout(module, 0, {0, 0, 1280, 900, 0, 1}), "global edit controls cannot bypass Satchel's own lock");
            runtime->editing_ = false;
            const auto item = module.CellBounds(0);
            module.HandleMouse(WM_RBUTTONDOWN, 0, item.x + 8, item.y + 8, false);
            Check(runtime->SatchelInputAvailable(true) && module.dialog_ == satchel_module::dialog::actions, "locked Satchel still opens item actions with the HUD locked");
            module.HandleControllerAction(controller_action::cancel, false);
            const auto search = module.SearchBounds();
            module.HandleMouse(WM_LBUTTONDOWN, 0, search.x + 8, search.y + 8, false);
            module.HandleMouse(WM_LBUTTONUP, 0, search.x + 8, search.y + 8, false);
            Check(module.KeyboardFocused(), "position locking leaves search interactive");
            module.editor_.Cancel();
            const auto saved = settings_store::FromIniText(""); module.Save(saved);
            satchel_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(saved);
            Check(!restored.CanMove() && restored.slot_opacity_ == .8f && saved.ReadBool("satchel", "lock_position", false),
                "the independent lock and darker slot background persist through reload");
            *module.Option(satchel_module::lock_position).toggle = false; module.OnOptionChanged(satchel_module::lock_position);
            const auto title = module.TitleBounds();
            Check(runtime->HandleSatchelMouse(WM_LBUTTONDOWN, 0, title.x + 8, title.y + 8, false) &&
                runtime->HandleSatchelMouse(WM_MOUSEMOVE, MK_LBUTTON, title.x + 38, title.y + 28, false) &&
                runtime->HandleSatchelMouse(WM_LBUTTONUP, 0, title.x + 38, title.y + 28, false) &&
                module.state_.bounds.x == original.x + 30 && module.state_.bounds.y == original.y + 20 && !module.PointerDown(),
                "unlocking allows movement through the runtime while global HUD editing remains off");
        }
        static auto ContainerOpacity(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>();
            const auto legacy = settings_store::FromIniText("[satchel]\nopacity=1.000\nslot_background_opacity=0.550\n[treasure]\nopacity=0.949\n");
            runtime->LoadSettings(legacy);
            const auto checkOpacity = [&](const float satchel, const float treasure) {
                const auto saved = runtime->CaptureSettings();
                Check(saved.ReadFloat("satchel", "opacity", 0) == satchel && saved.ReadFloat("treasure", "opacity", 0) == treasure,
                    "Satchel and Treasure retain the expected container opacity");
            };
            checkOpacity(.35f, .35f);
            Check(runtime->CaptureSettings().ReadFloat("satchel", "slot_background_opacity", 0) == .8f, "old slot backgrounds migrate to the darker default");
            auto snapshot = runtime->CaptureSettings();
            OverlayProfilePreferences(snapshot, legacy);
            runtime->LoadSettings(snapshot);
            checkOpacity(.35f, .35f);
            Check(runtime->CaptureSettings().ReadFloat("satchel", "slot_background_opacity", 0) == .8f, "legacy profile overlays retain the slot-background migration");
            const auto custom = settings_store::FromIniText("[satchel]\nopacity=0.600\n[treasure]\nopacity=0.700\n");
            OverlayProfilePreferences(snapshot, custom);
            runtime->LoadSettings(snapshot);
            checkOpacity(.6f, .7f);
            const auto modern = settings_store::FromIniText("[satchel]\nopacity=1.000\ncontainer_opacity_version=1\nslot_background_opacity=0.550\nslot_background_version=1\n[treasure]\nopacity=0.950\ncontainer_opacity_version=1\n");
            OverlayProfilePreferences(snapshot, modern);
            runtime->LoadSettings(snapshot);
            checkOpacity(1, .95f);
            Check(runtime->CaptureSettings().ReadFloat("satchel", "slot_background_opacity", 0) == .55f, "later custom slot opacity survives reload");
        }
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
            class mouse final : public IMouse
            {
            public:
                bool blocked{};
                void AddCallback(const char*, mousecallback_f) override {}
                void RemoveCallback(const char*) override {}
                bool GetBlockInput(void) const override { return this->blocked; }
                void SetBlockInput(bool value) override { this->blocked = value; }
            } input;
            auto runtime = std::make_unique<hud_runtime>();
            runtime->mouse_ = &input;
            runtime->inventory_source_.state_.available = true; satchel_data_test::Set(runtime->satchel_source_, Fixture());
            auto& module = runtime->satchel_; module.Initialize(nullptr, &runtime->satchel_source_, &runtime->satchel_cache_, &runtime->satchel_actions_);
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.state_.bounds.x = 80; module.state_.bounds.y = 60;
            module.Open(true); module.Update(1000); module.UpdateSize();
            Check(runtime->SatchelInputAvailable() && !runtime->KeyboardFocused(), "idle visible Satchel leaves the runtime keyboard gate open");
            const auto click = [&](const ui::rect& at) {
                const auto x = at.x + at.width * 0.5f, y = at.y + at.height * 0.5f;
                Check(runtime->HandleSatchelMouse(WM_LBUTTONDOWN, 0, x, y, input.GetBlockInput()), "Satchel owns its pointer press");
                Check(runtime->HandleSatchelMouse(WM_LBUTTONUP, 0, x, y, input.GetBlockInput()), "Satchel owns its pointer release");
            };
            runtime->SetChatMouseCapture(true);
            Check(input.GetBlockInput(), "hover capture blocks native mouse input before the first click");
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
            runtime->native_ui_.menu_visible = true;
            Check(!runtime->SatchelInputAvailable() && runtime->SatchelInputAvailable(true) &&
                !runtime->HandleSatchelKey(VK_DOWN, 0, false), "native menus retain unclaimed keyboard navigation while Satchel accepts pointer input");
            runtime->SetChatMouseCapture(true);
            click(module.CharacterBounds());
            runtime->UpdateVisibilityContext();
            Check(module.dialog_ == satchel_module::dialog::characters && runtime->KeyboardFocused() &&
                runtime->ControllerCapture() == controller_capture_mode::none,
                "mouse-opened popups persist over native menus without entering controller browsing");
            Check(runtime->HandleSatchelKey(VK_ESCAPE, 0, false) && !runtime->KeyboardFocused(),
                "an explicitly opened popup handles Escape and returns keyboard ownership to the native menu");
            runtime->HandleSatchelKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
            click(module.SearchBounds());
            runtime->UpdateVisibilityContext();
            Check(module.KeyboardFocused() && runtime->KeyboardFocused(), "clicked search retains typing focus over a native menu");
            runtime->HandleSatchelKey(VK_ESCAPE, 0, false);
            runtime->HandleSatchelKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
            runtime->SetChatMouseCapture(false);
            input.blocked = true;
            runtime->SetChatMouseCapture(true);
            const auto profile = module.CharacterBounds();
            Check(runtime->PointerInputBlocked(true) && !runtime->HandleSatchelMouse(WM_LBUTTONDOWN, 0, profile.x + 2, profile.y + 2, true) &&
                module.dialog_ == satchel_module::dialog::none, "mouse capture preserves an input block that existed before XIUI acquired it");
            runtime->SetChatMouseCapture(false);
            Check(input.blocked, "releasing XIUI capture restores the previous external block");
            input.blocked = false;
            runtime->native_ui_.menu_visible = false;
            runtime->mouse_block_state_saved_ = runtime->mouse_captured_ = true;
            Check(!runtime->PointerInputBlocked(true), "configuration's own mouse capture does not block its controls");
            runtime->previous_mouse_blocked_ = true;
            Check(runtime->PointerInputBlocked(true), "configuration capture retains a preexisting external block");
            runtime->mouse_block_state_saved_ = runtime->mouse_captured_ = false;
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
            ContextMenu();
            EmptySlotReorder();
            PositionLock();
            RuntimeOwnership();
        }
    };
}

auto main(const int argc, char** argv) -> int
{
    if (argc == 2 && std::strcmp(argv[1], "--slot-reorder-only") == 0)
    {
        try { xiui::satchel_ui_test::EmptySlotReorder(); std::puts("Satchel empty-slot drops, refresh, persistence and cancellation checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Satchel slot reorder check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--container-opacity-only") == 0)
    {
        try { xiui::satchel_ui_test::ContainerOpacity(); std::puts("Satchel and Treasure container opacity migration and profile checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Container opacity check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--input-ownership-only") == 0)
    {
        try { xiui::satchel_ui_test::ContextMenu(); xiui::satchel_ui_test::PositionLock(); xiui::satchel_ui_test::RuntimeOwnership(); std::puts("Satchel context menu, position lock, pointer and keyboard ownership checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Satchel input check failed: %s\n", error.what()); return 1; }
    }
    try { xiui::satchel_ui_test::Run(); std::puts("Satchel UI checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Satchel UI check failed: %s\n", error.what()); return 1; }
}
