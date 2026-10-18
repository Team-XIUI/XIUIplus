#include "modules/actionbars/actionbars_module.hpp"
#include "hud_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace
{
    auto Check(bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }
    struct keyboard_restore
    {
        BYTE keys[256]{};
        keyboard_restore() { Check(GetKeyboardState(this->keys) != FALSE, "capture keyboard state"); }
        ~keyboard_restore() { SetKeyboardState(this->keys); }
    };
}

namespace xiui
{
    struct actionbars_ui_test
    {
        static auto ControllerVisuals(void) -> void
        {
            using namespace crossbar_visuals;
            Check(Button(0, controller_backend::xinput, 2, 4).icon == "controller/playstation/circle" &&
                Button(3, controller_backend::xinput, 3, 0).icon == "controller/nintendo/b",
                "theme artwork follows physical XInput mappings, including remapped direction slots");
            Check(Button(8, controller_backend::xinput, 1, 7).icon == "controller/shared/l1" &&
                Button(6, controller_backend::xinput, 1, 6).icon == "controller/shared/up" &&
                Trigger(true, 12, controller_backend::xinput, 1).icon == "controller/shared/r2",
                "shoulder, D-pad and trigger remaps use their actual artwork");
            Check(Button(-41, controller_backend::direct_input, 2, 0).icon == "controller/shared/down" &&
                Button(40, controller_backend::direct_input, 2, 3).icon == "controller/shared/left" &&
                Button(0, controller_backend::direct_input, 4, 4).label == "B1" &&
                Button(42, controller_backend::direct_input, 4, 4).label == "B43" &&
                Button(-1, controller_backend::xinput, 1, 0).icon.empty(),
                "DirectInput respects POV direction and keeps unidentified or unbound buttons honest");
            Check(Button(-2, controller_backend::xinput, 1, 4).icon == "controller/xbox/y",
                "the disconnected preview retains the Lua controller artwork");

            press_motion press;
            press.Set(true, 100); press.Set(false, 110);
            Check(press.Value(100) == 1 && press.Value(140) > 1 && std::abs(press.Value(180) - 1.12f) < 0.001f &&
                std::abs(press.Value(260) - 1) < 0.001f, "a quick trigger tap still grows and settles without a held state");
            press.Set(true, 300, 150); press.Set(false, 310);
            Check(std::abs(press.Value(440) - 1.12f) < 0.001f && std::abs(press.Value(530) - 1) < 0.001f,
                "quick action taps retain the Lua minimum visible press");

            bank_motion left, right;
            left.Select(0, 100, 20, true); right.Select(1, 100, 20, true);
            left.Select(3, 1000, 20, true);
            Check(left.Value(0, 1000, 20).opacity == 1 && left.Value(3, 1000, 20).opacity == 0 &&
                left.Value(3, 1000, 20).y == 20 && right.Value(1, 1100, 20).opacity == 1,
                "only the changed half slides and fades while the other half remains steady");
            const auto before = left.Value(3, 1100, 20);
            left.Select(4, 1100, 20, true);
            const auto after = left.Value(3, 1100, 20);
            Check(std::abs(before.opacity - after.opacity) < 0.001f && std::abs(before.y - after.y) < 0.001f &&
                left.Value(4, 1600, 20).opacity == 1 && left.Value(4, 1600, 20).y == 0 && left.Value(3, 1600, 20).opacity == 0,
                "rapid bank changes retarget from the current picture and finish on the latest bank");
            left.Select(0, 1601, 20, false);
            Check(left.Value(0, 1601, 20).opacity == 1 && left.Value(4, 1601, 20).opacity == 0,
                "disabling crossbar motion changes banks immediately");
            bank_motion dimmed;
            dimmed.Select(0, 100, 20, true, true);
            dimmed.Select(3, 200, 20, true, false);
            Check(dimmed.Value(0, 200, 20).inactive && !dimmed.Value(3, 200, 20).inactive,
                "an outgoing inactive bank retains its dimming instead of flashing brighter");

            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor);
            module.bars_[action_bar_count].state.enabled = true;
            module.controller_settings_.enabled = module.controller_settings_.replace_native = true;
            const auto cell = module.SlotBounds({action_surface::crossbar, 0, 0});
            controller_event event{controller_action::crossbar_bank, controller_backend::xinput, 0, true, false, false, 1000};
            event.bank = crossbar_bank::left; event.triggers = 1;
            module.HandleCrossbarEvent(event);
            event.action = controller_action::crossbar_slot; event.slot = 1;
            module.HandleCrossbarEvent(event);
            Check(module.active_bank_ == crossbar_bank::left && module.held_slots_[0][0] && module.button_motion_[0][0].Value(1080) > 1 &&
                module.SlotBounds({action_surface::crossbar, 0, 0}).x == cell.x && module.SlotBounds({action_surface::crossbar, 0, 0}).y == cell.y,
                "animation follows the real event while input and slot hit bounds change without delay");
            module.SuspendCrossbarInput();
            Check(!module.held_slots_[0][0] && module.trigger_motion_[0].Value(1100) == 1 &&
                module.button_motion_[0][0].Value(1100) == 1 && !module.bank_motion_[0].initialized,
                "suspension clears every transient crossbar visual");
            module.Load(settings_store::FromIniText("[actionbars]\ncrossbar_animations=0\n"));
            Check(!module.crossbar_animations_, "crossbar motion can be disabled in saved settings");
        }
        static auto Command(const char* name) -> action_slot
        { action_slot result; result.kind = action_kind::command; result.name = name; result.lines = {"/echo test"}; return result; }
        static auto Control(actionbars_module& module, const char* label) -> int
        {
            const auto controls = module.Controls();
            const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& control) { return control.text == label && control.enabled; });
            Check(found != controls.end(), label); return found->id;
        }
        static auto Click(actionbars_module& module, const ui::rect& bounds) -> void
        {
            const auto x = bounds.x + bounds.width / 2, y = bounds.y + bounds.height / 2;
            Check(module.HandleMouse(WM_LBUTTONDOWN, 0, x, y, false), "mouse press captured");
            Check(module.HandleMouse(WM_LBUTTONUP, 0, x, y, false), "matching mouse release captured");
        }
        static auto ControlBounds(actionbars_module& module, const char* label) -> ui::rect
        {
            const auto controls = module.Controls();
            const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& control) { return control.text == label && control.enabled; });
            Check(found != controls.end(), label); return found->bounds;
        }
        static auto Overlaps(const ui::rect& a, const ui::rect& b) -> bool
        { return std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x) > 0.01f && std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y) > 0.01f; }
        static auto Ownership(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 60;
            store.Assign(action_surface::hotbar, 0, 0, Command("First"));
            Check(!module.InputFocused() && !module.HandleKey('W', 0), "idle bars leave movement available");
            Click(module, module.SlotBounds({action_surface::hotbar, 0, 0}));
            Check(!module.InputFocused() && !module.navigation_focus_, "mouse activation never retains browsing or a focus frame");
            module.BeginControllerBrowse();
            module.bars_[0].heading = true; module.Layout(0);
            Click(module, module.PaletteBounds(0));
            Check(module.dialog_ == actionbars_module::dialog::palette_dropdown && !module.browsing_, "pointer palette takeover clears controller browsing first");
            module.EndControllerBrowse();
            Check(module.dialog_ == actionbars_module::dialog::palette_dropdown, "root controller cleanup keeps a newly opened mouse dropdown");
            Click(module, {4, 4, 1, 1});
            Check(!module.InputFocused() && !module.PointerDown(), "outside dismissal releases movement and pointer ownership");
            Check(!module.HandleKey('W', 0), "ordinary movement remains native after dismissal");
            module.OpenEditor(false, 0, 0);
            module.BeginField(3); module.input_.Insert("Changed label");
            module.HandleKey(VK_ESCAPE, 0);
            Check(!module.KeyboardFocused() && module.DialogOpen(), "Escape leaves a field without committing a live slot");
            module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u));
            module.HandleKey(VK_ESCAPE, 0);
            Check(!module.InputFocused() && store.Resolve(action_surface::hotbar, 0, 0).label.empty(), "second Escape closes editor and preserves committed assignment");
            Check(module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u)) && !module.HandleKey('W', 0), "owned Escape release drains once before movement resumes");
            module.OpenEditor(false, 0, 0); module.BeginField(3); module.input_.Insert("Unsaved"); module.pointer_down_ = true;
            module.SuspendInput();
            Check(!module.InputFocused() && !module.PointerDown() && store.Resolve(action_surface::hotbar, 0, 0).label.empty(), "suspension cancels draft and releases every input surface");
            Check(module.HandleMouse(WM_LBUTTONUP, 0, -1, -1, false), "suspended gesture owns its release");
        }
        static auto DirectInteraction(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 100; module.bars_[0].state.bounds.y = 80; module.bars_[0].empty = true;
            store.Assign(action_surface::hotbar, 0, 0, Command("First")); store.Assign(action_surface::hotbar, 0, 1, Command("Second"));
            module.bars_[0].locked = false;
            Click(module, module.SlotBounds({action_surface::hotbar, 0, 0}));
            Check(!module.DialogOpen() && !module.InputFocused() && !executor.Status().empty(), "unlocked assigned slots activate without opening an editor or retaining movement focus");
            module.BeginControllerBrowse(); module.selected_ = {action_surface::hotbar, 0, 0};
            executor.Clear(); module.HandleControllerAction(controller_action::confirm, false);
            Check(!module.DialogOpen() && module.browsing_ && !executor.Status().empty(), "controller Confirm activates an unlocked assigned slot");
            module.HandleControllerAction(controller_action::cancel, false);
            Check(!module.InputFocused(), "controller Cancel returns movement after activation");
            module.BeginControllerBrowse(true); module.selected_.bar = 4; module.selected_bank_ = 4; module.SetPreview(false);
            Check(module.CrossbarGroups() == std::vector<size_t>{4, 1}, "controller browsing keeps the chosen expanded bank visible after assignment closes");
            module.HandleControllerAction(controller_action::cancel, false); module.SetPreview(false);
            Check(module.CrossbarGroups() == std::vector<size_t>{0, 1}, "leaving controller browsing restores base crossbar halves");
            module.bars_[0].locked = true;
            const auto first = module.SlotBounds({action_surface::hotbar, 0, 0}), second = module.SlotBounds({action_surface::hotbar, 0, 1});
            module.HandleMouse(WM_LBUTTONDOWN, 0, first.x + 10, first.y + 10, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, second.x + 10, second.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, second.x + 10, second.y + 10, false);
            Check(store.Resolve(action_surface::hotbar, 0, 0).name == "First" && store.Resolve(action_surface::hotbar, 0, 1).name == "Second" && !module.DialogOpen(), "locked assignments cannot be swapped by dragging");
            const auto before = store.Resolve(action_surface::hotbar, 0, 0);
            Click(module, module.LockBounds(0));
            Check(!module.bars_[0].locked && !module.InputFocused() && !module.navigation_focus_ && store.Resolve(action_surface::hotbar, 0, 0) == before, "lock button changes only assignment locking and releases mouse ownership");
            Click(module, module.LockBounds(0)); Check(module.bars_[0].locked && !module.InputFocused(), "locking again does not retain browsing focus");
            Click(module, module.SlotBounds({action_surface::hotbar, 0, 2}));
            Check(module.dialog_ == actionbars_module::dialog::picker, "an empty slot opens the action library even while locked");
            module.results_ = {Command("Direct assignment")};
            Click(module, ControlBounds(module, "Direct assignment"));
            Check(store.Resolve(action_surface::hotbar, 0, 2).name == "Direct assignment" && !module.DialogOpen() && !module.InputFocused() && !module.PointerDown(), "mouse library selection assigns immediately and returns movement without an intermediate editor");
        }
        static auto LibraryPlacement(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 100; module.bars_[0].state.bounds.y = 800; module.Layout(0);
            module.BeginControllerBrowse(); module.OpenLibrary(false, 0, 0);
            module.results_ = {Command("Controller assignment")};
            module.ActivateControl(Control(module, "Controller assignment"));
            Check(module.placing_ && module.draft_.name == "Controller assignment" && !module.library_drag_ && module.dialog_ == actionbars_module::dialog::picker && module.InputFocused() && store.Resolve(action_surface::hotbar, 0, 0).Empty(), "controller choice starts destination placement while retaining modal ownership");
            module.HandleControllerAction(controller_action::right, false);
            const auto destination = module.selected_;
            Check(destination.slot != 0, "controller can choose a different assignment destination");
            module.HandleControllerAction(controller_action::confirm, true);
            Check(store.Resolve(destination.surface, destination.bar, destination.slot).Empty() && module.placing_, "held Confirm cannot commit a placement");
            module.HandleControllerAction(controller_action::confirm, false);
            Check(!module.placing_ && !module.library_drag_ && store.Resolve(destination.surface, destination.bar, destination.slot).name == "Controller assignment" && store.Resolve(action_surface::hotbar, 0, 0).Empty(), "fresh Confirm assigns only the chosen destination");
            module.SuspendInput();
            module.BeginControllerBrowse(); module.OpenLibrary(false, 0, 3); module.query_ = "cancelled"; module.results_ = {Command("Cancelled assignment")};
            module.ActivateControl(Control(module, "Cancelled assignment"));
            module.HandleControllerAction(controller_action::cancel, false);
            Check(!module.placing_ && !module.library_drag_ && module.dialog_ == actionbars_module::dialog::picker && module.draft_.name == "Cancelled assignment" && module.query_ == "cancelled" && store.Resolve(action_surface::hotbar, 0, 3).Empty(), "Cancel returns placement to the library with its query and selected action preserved");
            module.SuspendInput();
            module.OpenLibrary(false, 0, 4); module.results_ = {Command("Dragged assignment")};
            const auto source = ControlBounds(module, "Dragged assignment"), target = module.SlotBounds({action_surface::hotbar, 0, 5});
            module.HandleMouse(WM_LBUTTONDOWN, 0, source.x + source.width / 2, source.y + source.height / 2, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, target.x + 10, target.y + 10, false);
            Check(store.Resolve(action_surface::hotbar, 0, 5).name == "Dragged assignment" && store.Resolve(action_surface::hotbar, 0, 4).Empty() && !module.library_drag_ && module.dialog_ == actionbars_module::dialog::picker && !module.PointerDown(), "library drag assigns its drop slot and keeps the library available for another assignment");
            module.HandleKey(VK_ESCAPE, 0); module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u));
            Check(!module.InputFocused() && !module.navigation_focus_ && !module.HandleKey('W', 0), "closing the library after a drag releases native movement ownership");
            module.OpenLibrary(false, 0, 6); module.results_ = {Command("Suspended assignment")};
            const auto pending = ControlBounds(module, "Suspended assignment");
            module.HandleMouse(WM_LBUTTONDOWN, 0, pending.x + pending.width / 2, pending.y + pending.height / 2, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            module.SuspendInput();
            Check(!module.placing_ && !module.library_drag_ && !module.InputFocused() && !module.PointerDown() && store.Resolve(action_surface::hotbar, 0, 6).Empty(), "suspension cancels library placement and pending pointer assignment");
            Check(module.HandleMouse(WM_LBUTTONUP, 0, target.x + 10, target.y + 10, false) && !module.HandleKey('W', 0), "suspended library drag drains its release before native movement resumes");
        }
        static auto BarControlsAndCrossbarGeometry(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.viewport_ = {0, 0, 1280, 900};
            module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 60; module.bars_[0].heading = true; module.Layout(0);
            for (const auto [bounds, expected] : {std::pair{module.PaletteBounds(0), 1}, {module.MenuBounds(0), 2}, {module.LockBounds(0), 4}, {module.BarNumberBounds(0), 5}})
            {
                const auto hit = module.HitTest(bounds.x + bounds.width / 2, bounds.y + bounds.height / 2);
                Check(hit && hit->header == expected, "bar number, palette, lock and menu have distinct pointer targets");
            }
            constexpr int columns[]{6, 4, 3, 2, 1, 12};
            module.bars_[0].columns = 12; module.bars_[0].slots = 12; module.Layout(0);
            store.Assign(action_surface::hotbar, 0, 11, Command("Preserved"));
            for (const auto expected : columns)
            {
                const auto number = module.BarNumberBounds(0);
                Check(module.HandleMouse(WM_RBUTTONDOWN, 0, number.x + number.width / 2, number.y + number.height / 2, false), "bar number accepts a shape shortcut");
                Check(module.bars_[0].columns == expected && module.bars_[0].slots == 12 && !module.InputFocused() && store.Resolve(action_surface::hotbar, 0, 11).name == "Preserved", "shape cycling preserves all twelve assignments and native movement");
            }
            auto& crossbar = module.bars_[action_bar_count]; crossbar.state.enabled = true; crossbar.empty = true; crossbar.state.bounds.x = 100; crossbar.state.bounds.y = 450; module.Layout(action_bar_count);
            constexpr size_t halves[]{0, 1, 1, 0, 0, 1};
            const std::array<std::vector<size_t>, 6> groups{{{0, 1}, {0, 1}, {0, 2}, {3, 1}, {4, 1}, {0, 5}}};
            std::array<std::array<ui::rect, 8>, 2> original{};
            for (size_t half = 0; half < 2; ++half) for (size_t slot = 0; slot < 8; ++slot) original[half][slot] = module.SlotBounds({action_surface::crossbar, half, slot});
            for (size_t bank = 0; bank < 6; ++bank)
            {
                module.active_bank_ = static_cast<crossbar_bank>(bank + 1);
                Check(module.CrossbarGroups() == groups[bank], "ordered and double-trigger banks replace their stable trigger half");
                std::vector<ui::rect> occupied;
                for (const auto visible : module.CrossbarGroups())
                    for (size_t slot = 0; slot < 8; ++slot)
                    {
                        const auto rect = module.SlotBounds({action_surface::crossbar, visible, slot}); const auto& base = original[halves[visible]][slot];
                        Check(std::abs(rect.x - base.x) < 0.01f && std::abs(rect.y - base.y) < 0.01f, "crossbar icon coordinates stay fixed across every trigger bank");
                        Check(rect.x >= crossbar.state.bounds.x && rect.y >= crossbar.state.bounds.y && rect.x + rect.width <= crossbar.state.bounds.x + crossbar.state.bounds.width && rect.y + rect.height <= crossbar.state.bounds.y + crossbar.state.bounds.height, "all crossbar icons fit inside the module bounds");
                        for (const auto& previous : occupied) Check(!Overlaps(previous, rect), "the sixteen crossbar icons never overlap");
                        occupied.push_back(rect);
                        const auto hit = module.HitTest(rect.x + rect.width / 2, rect.y + rect.height / 2);
                        Check(hit && hit->header == 0 && hit->slot.surface == action_surface::crossbar && hit->slot.bar == visible && hit->slot.slot == slot, "every visible crossbar icon has its own matching pointer target");
                    }
                Check(occupied.size() == 16, "every bank displays two complete eight-slot trigger halves");
            }
            const auto up = original[0][0], right = original[0][1], down = original[0][2], left = original[0][3];
            Check(up.y + up.height <= left.y && right.y + right.height <= down.y && left.x + left.width <= up.x && up.x + up.width <= right.x, "each controller diamond uses three non-overlapping rows and columns");
        }
        static auto EmptyBarEditBounds(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module; ui::design_system design;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.bars_[0].heading = false; module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 60;
            const module_render_context context{design, nullptr, true, nullptr, {0, 0, 1280, 900}};
            for (const float columns : {12.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.0f})
            {
                module.bars_[0].columns = columns;
                design.BeginContentBounds(); module.RenderBar(0, context);
                const auto content = design.EndContentBounds(); auto& state = module.bars_[0].state;
                state.content_bounds = {content.x - state.bounds.x, content.y - state.bounds.y, content.width, content.height};
                const auto edit = state.EditBounds();
                for (size_t slot = 0; slot < action_bar_slots; ++slot)
                {
                    const auto cell = module.SlotBounds({action_surface::hotbar, 0, slot});
                    Check(edit.x < cell.x && edit.y < cell.y && edit.x + edit.width > cell.x + cell.width && edit.y + edit.height > cell.y + cell.height + 2,
                        "edit outline wraps every empty slot and its shadow without relying on icons or markers");
                }
            }
        }
        static auto Editing(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault()); module.viewport_ = {0, 0, 1280, 900};
            store.Assign(action_surface::hotbar, 0, 0, Command("First")); store.Assign(action_surface::hotbar, 0, 1, Command("Second"));
            module.bars_[0].state.bounds.x = 100; module.bars_[0].state.bounds.y = 80; module.bars_[0].locked = false;
            const auto first = module.SlotBounds({action_surface::hotbar, 0, 0}), second = module.SlotBounds({action_surface::hotbar, 0, 1});
            module.HandleMouse(WM_LBUTTONDOWN, 0, first.x + 10, first.y + 10, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, second.x + 10, second.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, second.x + 10, second.y + 10, false);
            Check(store.Resolve(action_surface::hotbar, 0, 0).name == "Second" && store.Resolve(action_surface::hotbar, 0, 1).name == "First" && !module.InputFocused(), "unlocked drag swaps assignments without sticky input");
            const auto macroId = store.CreateMacro("Shared", {"/echo first", "/wait 1", "/echo second"});
            action_slot macro; macro.kind = action_kind::macro; macro.macro_id = macroId;
            store.Assign(action_surface::hotbar, 0, 2, macro); store.Assign(action_surface::hotbar, 0, 3, macro);
            module.OpenEditor(false, 0, 2);
            Check(module.draft_.macro_id == macroId && module.draft_.lines.size() == 3, "shared macro editor keeps its stable reference while expanding lines");
            module.draft_.lines[0] = "/echo updated"; module.SaveDraft();
            Check(store.Macros().size() == 1 && store.ExpandMacro(store.Resolve(action_surface::hotbar, 0, 3)).lines[0] == "/echo updated", "editing a shared macro updates all referencing slots without duplicating it");
            module.OpenPalettes(); const auto original = store.AllPalettes().size();
            module.ActivateControl(Control(module, "New palette")); module.input_.Insert("Cancelled");
            module.HandleKey(VK_ESCAPE, 0); module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u));
            module.HandleKey(VK_ESCAPE, 0);
            Check(store.AllPalettes().size() == original, "cancelled palette creation preserves storage");
            module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u)); module.CloseDialog();
            for (int index = 0; index < 14; ++index) store.Create("Palette " + std::to_string(index));
            module.OpenPalettes();
            Check(module.Scrollbar().Visible(), "large palette lists expose a scrollbar");
            module.SetScroll(1000); Check(module.palette_scroll_ == static_cast<size_t>(module.Scrollbar().Maximum()), "palette scrolling clamps to the final page");
            module.palette_id_ = store.Palettes(action_surface::hotbar)[1]->id;
            module.ActivateControl(Control(module, "Delete..."));
            Check(module.dialog_ == actionbars_module::dialog::delete_palette && module.focused_control_ == Control(module, "Cancel"), "palette deletion starts on Cancel");
            const auto count = store.AllPalettes().size(); module.HandleControllerAction(controller_action::confirm, true);
            Check(store.AllPalettes().size() == count, "held Confirm never accepts deletion");
            module.HandleControllerAction(controller_action::confirm, false);
            Check(store.AllPalettes().size() == count && module.dialog_ == actionbars_module::dialog::palettes, "fresh initial Confirm cancels deletion");
            module.CloseDialog();
            for (size_t view = 0; view <= action_bar_count; ++view)
            {
                module.SelectConfigView(view);
                for (size_t option = 0; option < module.OptionCount(); ++option)
                {
                    size_t owners{};
                    for (size_t topic = 0; topic < module.ConfigSections().size(); ++topic) owners += module.ConfigSectionContains(topic, option);
                    if (option == actionbars_module::bar_selector)
                        Check(owners == (view < action_bar_count ? module.ConfigSections().size() : size_t{0}), "hotbar selector stays available across normal-bar topics");
                    else Check(owners <= 1, "visible options belong to one topic and intentionally hidden controls have no owners");
                    if (owners != 0) Check(module.Option(option).label, "every visible configuration control has a label");
                }
            }
        }
        static auto KeyboardAndCrossbar(void) -> void
        {
            keyboard_restore restore; BYTE keys[256]{}; Check(SetKeyboardState(keys) != FALSE, "clear keyboard modifiers");
            action_store store; action_executor executor; actionbars_module module; module.Initialize(nullptr, &store, &executor);
            store.Assign(action_surface::hotbar, 0, 0, Command("Bound action"));
            module.bars_[0].keys[0] = {'1', 1};
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80; Check(SetKeyboardState(keys) != FALSE, "set Ctrl modifier");
            Check(module.ReplaceNativeMacros(), "native macro replacement starts enabled"); module.allow_native_keys_ = false;
            Check(!module.HandleKey('1', 0), "saved Ctrl-number bindings preserve native macros while replacement is off");
            module.allow_native_keys_ = true; Check(module.HandleKey('1', 0), "explicit replacement activates the configured binding route");
            module.SuspendInput(); Check(module.HandleKey('1', static_cast<LPARAM>(0x80000000u)) && !module.HandleKey('1', static_cast<LPARAM>(0x80000000u)), "held binding release drains exactly once across suspension");
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0; Check(SetKeyboardState(keys) != FALSE, "clear Ctrl modifier");
            module.bars_[0].keys[0] = {'Q', 0}; module.available_[0] = false;
            Check(!module.HandleKey('Q', 0), "context-hidden bars never intercept their bindings");
            module.available_[0] = true;
            module.controller_settings_.enabled = module.controller_settings_.replace_native = true; module.bars_[action_bar_count].state.enabled = true;
            controller_event bank; bank.action = controller_action::crossbar_bank; bank.bank = crossbar_bank::left; bank.triggers = 1;
            module.HandleCrossbarEvent(bank);
            controller_event press; press.action = controller_action::crossbar_slot; press.bank = crossbar_bank::right; press.slot = 1; press.pressed = true;
            module.HandleCrossbarEvent(press); Check(!module.held_slots_[1][0], "stale queued bank presses never activate the current bank");
            press.bank = crossbar_bank::left; module.HandleCrossbarEvent(press); Check(module.held_slots_[0][0], "fresh mapped slot press highlights its original bank");
            press.pressed = false; module.HandleCrossbarEvent(press); Check(!module.held_slots_[0][0], "slot release clears held highlight");
            press.pressed = true; module.HandleCrossbarEvent(press); bank.bank = crossbar_bank::left_right; bank.triggers = 3; module.HandleCrossbarEvent(bank);
            Check(!module.held_slots_[0][0] && module.CrossbarGroups() == std::vector<size_t>{0, 2}, "bank transition clears stale held visuals while retaining stable trigger halves");
            module.SuspendInput(); Check(module.active_bank_ == crossbar_bank::none && !module.InputFocused(), "suspension clears active trigger and browsing state");
        }
        static auto Persistence(void) -> void
        {
            actionbars_module module; module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.bars_[7].state.enabled = true; module.bars_[7].columns = 3; module.bars_[7].slots = 9;
            module.bars_[7].keys[4] = {'Q', 5}; module.text_[1].style.glow.enabled = true; module.text_[1].style.bottom_color = 0xFF123456;
            module.controller_settings_.enabled = module.controller_settings_.replace_native = true;
            module.controller_settings_.xinput = {9, 12, 11, {6, 4, 7, 5, 2, 0, 3, 1}};
            module.controller_settings_.direct_input.left_trigger = 127; module.controller_settings_.direct_input.slots[0] = -41; module.controller_settings_.direct_input.slots[7] = 127;
            auto settings = settings_store::FromIniText(""); module.Save(settings);
            actionbars_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(settings);
            Check(restored.bars_[7].state.enabled && restored.bars_[7].columns == 3 && restored.bars_[7].keys[4] == module.bars_[7].keys[4] && restored.text_[1].style.glow.enabled && restored.text_[1].style.bottom_color == 0xFF123456, "independent bar layout, binding and styled text persist");
            Check(restored.controller_settings_ == module.controller_settings_, "local controller mapping and explicit replacement persist");
            Check(restored.controller_settings_.direct_input.left_trigger == 127 && restored.controller_settings_.direct_input.slots[0] == -41 && restored.controller_settings_.direct_input.slots[7] == 127, "DirectInput reversed POV and high button IDs survive settings roundtrip");
            restored.Load(settings_store::FromIniText("[actionbars]\nprompt_theme=2\n"));
            Check(restored.controller_settings_ == module.controller_settings_ && restored.prompt_theme_ == 2, "portable profile overlays retain local hardware configuration");
        }
        static auto StyledSlots(void) -> actionbar_slot_style
        {
            actionbar_slot_style style;
            style.frame = 3; style.cooldown_style = 2;
            style.binding_anchor = 8; style.count_anchor = 0; style.timer_anchor = 7; style.cost_anchor = 2;
            style.radius = 17.5f; style.border_width = 2.5f; style.icon_inset = 7.25f;
            style.icon_opacity = 0; style.inactive_opacity = 0;
            style.binding_x = -48; style.binding_y = 48; style.count_x = 12.5f; style.count_y = -12.5f;
            style.timer_x = -6.25f; style.timer_y = 6.25f; style.cost_x = 24; style.cost_y = -24;
            style.shadow = style.sheen = style.hover_highlight = style.press_highlight = style.activation_flash = style.cooldown_text = style.mp_cost = false;
            style.fill = 0x00112233; style.empty_fill = 0x00445566; style.border = 0;
            style.icon_tint = 0x00778899; style.unavailable_tint = 0x00AABBCC; style.inactive_tint = 0x00123456;
            style.hover_color = 0x00654321; style.pressed_color = 0x00ABCDEF; style.cooldown_color = 0x00FEDCBA;
            return style;
        }
        static auto ProfileSnapshot(const module_text_profile& profile) -> std::string
        {
            auto settings = settings_store::FromIniText(""); SaveTextProfile(settings, "profile", profile);
            return settings.ToIniText();
        }
        static auto SlotStylePersistence(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            actionbars_module module; module.ApplyTextDefaults(theme);
            module.bars_[0].style = StyledSlots();
            module.bars_[7].style = StyledSlots(); module.bars_[7].style.frame = 1; module.bars_[7].style.radius = 0;
            module.bars_[7].style.icon_opacity = 0.5f; module.bars_[7].style.mp_cost = true;
            module.bars_[action_bar_count].style = StyledSlots();
            auto& crossbar = module.bars_[action_bar_count].style;
            crossbar.frame = 2; crossbar.cooldown_style = 3; crossbar.count_anchor = 6; crossbar.cost_anchor = 8;
            crossbar.icon_opacity = 1; crossbar.inactive_opacity = 0.75f; crossbar.shadow = crossbar.cooldown_text = crossbar.mp_cost = true;
            crossbar.fill = 0xC0445566; crossbar.cooldown_color = 0x88010203;
            for (size_t index = 0; index < module.text_.size(); ++index)
            {
                auto& profile = module.text_[index];
                profile.style.top_color = 0xFF123400 + static_cast<D3DCOLOR>(index);
                profile.style.bottom_color = 0xFF567800 + static_cast<D3DCOLOR>(index);
                profile.style.shadow = {true, 2, 3, 0xA0112233, true};
                profile.style.stroke = {true, 0xBB445566}; profile.style.glow = {true, 0xCC778899};
                profile.style.sheen = {true, 0xDDABCDEF, 42, 0.5f};
                profile.offset_x = static_cast<float>(index); profile.offset_y = -static_cast<float>(index);
            }
            auto settings = settings_store::FromIniText(""); module.Save(settings);
            const auto serialized = settings_store::FromIniText(settings.ToIniText());
            actionbars_module restored; restored.ApplyTextDefaults(theme); restored.Load(serialized);
            for (const size_t index : {size_t{0}, size_t{7}, action_bar_count})
                Check(restored.bars_[index].style == module.bars_[index].style, "every slot design field roundtrips independently for hotbars and crossbar");
            Check(restored.bars_[1].style == actionbar_slot_style{} && restored.bars_[0].style != restored.bars_[7].style &&
                restored.bars_[7].style != restored.bars_[action_bar_count].style, "saving a design does not bleed into another bar");
            Check(restored.bars_[0].style.fill == 0x00112233 && restored.bars_[0].style.border == 0 &&
                restored.bars_[0].style.icon_opacity == 0 && restored.bars_[0].style.inactive_opacity == 0,
                "valid zero alpha and zero opacity remain transparent after serialization");
            for (size_t index = 0; index < module.text_.size(); ++index)
                Check(ProfileSnapshot(restored.text_[index]) == ProfileSnapshot(module.text_[index]),
                    "title, label, binding, timer, tooltip, count and MP cost retain their independent shared text effects");
            Check(serialized.ReadArgb("text.actionbars.count", "top_color", 0) == module.text_[5].style.top_color &&
                serialized.ReadArgb("text.actionbars.cost", "top_color", 0) == module.text_[6].style.top_color,
                "count and MP cost append separate text profiles without renaming earlier roles");
        }
        static auto SlotStyleCompatibility(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            actionbars_module legacy; legacy.ApplyTextDefaults(theme);
            auto oldSettings = settings_store::FromIniText("[actionbars]\npresentation_version=1\n");
            auto binding = legacy.text_[2];
            binding.style.top_color = 0xFF123456; binding.style.bottom_color = 0xFF654321;
            binding.style.glow = {true, 0xAA334455}; binding.style.shadow.enabled = false; binding.style.stroke.enabled = false;
            binding.offset_x = 7; binding.offset_y = -4;
            SaveTextProfile(oldSettings, "text.actionbars.binding", binding);
            legacy.Load(oldSettings);
            for (const auto& bar : legacy.bars_)
                Check(bar.style == actionbar_slot_style{} && bar.style.frame == actionbar_slot_style::window_frame, "missing slot settings default to the current native window theme");
            auto count = binding; count.style.alignment = ui::text_alignment::right;
            Check(ProfileSnapshot(legacy.text_[2]) == ProfileSnapshot(binding) && ProfileSnapshot(legacy.text_[5]) == ProfileSnapshot(count) &&
                ProfileSnapshot(legacy.text_[6]) == ProfileSnapshot(count) && legacy.text_[5].style.alignment == ui::text_alignment::right,
                "missing count and MP cost settings inherit customized binding typography and effects without changing existing roles");

            actionbars_module clamped; clamped.ApplyTextDefaults(theme);
            clamped.Load(settings_store::FromIniText(
                "[actionbars.hotbar.slots]\nframe=99\ncooldown_style=-9\nbinding_anchor=-3\ncount_anchor=99\ntimer_anchor=invalid\ncost_anchor=99\n"
                "radius=nan\nborder_width=99\nicon_inset=-20\nicon_opacity=1.4\ninactive_opacity=-1\n"
                "binding_x=-999\nbinding_y=inf\ncount_x=999\ncount_y=-inf\ntimer_x=invalid\ntimer_y=49\ncost_x=-49\ncost_y=nan\n"
                "fill=00112233\nempty_fill=invalid\nborder=00000000\nmp_cost=false\n"
                "[actionbars.crossbar.slots]\nframe=-9\ncooldown_style=99\nradius=999\nborder_width=-2\nicon_inset=999\nicon_opacity=0\ninactive_opacity=0\n"));
            auto expected = actionbar_slot_style{};
            expected.frame = actionbar_slot_style::window_frame; expected.cooldown_style = 0; expected.binding_anchor = expected.timer_anchor = 0; expected.count_anchor = 8; expected.cost_anchor = 8;
            expected.border_width = 4; expected.icon_inset = 0; expected.icon_opacity = 1; expected.inactive_opacity = 0;
            expected.binding_x = -48; expected.count_x = 48; expected.timer_y = 48; expected.cost_x = -48;
            expected.fill = 0x00112233; expected.border = 0; expected.mp_cost = false;
            Check(clamped.bars_[0].style == expected, "malformed values fall back, finite values clamp, and zero-alpha colors remain valid");
            expected = {}; expected.frame = 0; expected.cooldown_style = 3; expected.radius = 48; expected.border_width = 0; expected.icon_inset = 12;
            expected.icon_opacity = expected.inactive_opacity = 0;
            Check(clamped.bars_[action_bar_count].style == expected, "crossbar values use the same bounded schema with valid zero opacity");
            const auto customized = clamped.bars_[0].style;
            clamped.Load(settings_store::FromIniText("[actionbars.hotbar.slots]\nradius=inf\nbinding_y=nan\n"));
            Check(clamped.bars_[0].style == customized, "partial profiles and nonfinite values preserve the current finite design");

            actionbars_module themed; themed.ApplyTextDefaults(theme);
            themed.Load(settings_store::FromIniText("[actionbars.hotbar.slots]\nframe=0\n[actionbars.crossbar.slots]\nframe=0\n"
                "[actionbars.hotbar2.slots]\nframe=0\nfill=FF123456\n[actionbars.hotbar3.slots]\nframe=1\n"));
            Check(themed.bars_[0].style.frame == actionbar_slot_style::window_frame && themed.bars_[action_bar_count].style.frame == actionbar_slot_style::window_frame,
                "untouched saved hotbar and crossbar designs migrate to the native window theme");
            Check(themed.bars_[1].style.frame == 0 && themed.bars_[1].style.fill == 0xFF123456 && themed.bars_[2].style.frame == 1,
                "custom saved colors and explicit alternate frame styles keep their previous appearance");
            Check(ProfileSnapshot(legacy.text_[2]) == ProfileSnapshot(binding) && themed.bars_[0].style.cooldown_text && themed.bars_[0].style.mp_cost,
                "window-frame migration retains text customizations and slot overlays");
            themed.bars_[0].style.frame = 0;
            auto saved = settings_store::FromIniText(""); themed.Save(saved);
            actionbars_module reloaded; reloaded.ApplyTextDefaults(theme); reloaded.Load(saved);
            Check(saved.ReadInt("actionbars", "slot_frame_version", 0) == 1 && reloaded.bars_[0].style.frame == 0,
                "choosing the original beveled style after migration survives reload");
        }
        static auto SlotFrameProfileOverlay(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.ApplyTextDefaults(ui::theme::CreateDefault()); module.bars_[1].style.frame = 0;
            auto snapshot = runtime->CaptureSettings();
            const auto legacy = settings_store::FromIniText("[actionbars.hotbar.slots]\nframe=0\n[actionbars.crossbar.slots]\nframe=0\n"
                "[ActionBars.Hotbar3.Slots]\nFRAME=0\nfill=FF123456\n");
            OverlayProfilePreferences(snapshot, legacy);
            Check(snapshot.ReadInt("actionbars", "slot_frame_version", 0) == 1 && snapshot.ReadInt("actionbars.hotbar.slots", "frame_version", 1) == 0 &&
                snapshot.ReadInt("actionbars.hotbar2.slots", "frame_version", 0) == 1,
                "profile overlays track the source version only for the slot sections they replace");
            runtime->LoadSettings(snapshot);
            Check(module.bars_[0].style.frame == actionbar_slot_style::window_frame && module.bars_[action_bar_count].style.frame == actionbar_slot_style::window_frame,
                "legacy profile defaults migrate even when CaptureSettings contributed a newer global marker");
            Check(module.bars_[1].style.frame == 0 && module.bars_[2].style.frame == 0 && module.bars_[2].style.fill == 0xFF123456,
                "partial legacy overlays preserve current explicit Beveled choices and imported custom designs");

            snapshot = runtime->CaptureSettings();
            OverlayProfilePreferences(snapshot, settings_store::FromIniText("[actionbars]\nslot_frame_version=1\n[actionbars.hotbar.slots]\nframe=0\n"));
            runtime->LoadSettings(snapshot);
            Check(module.bars_[0].style.frame == 0 && module.bars_[1].style.frame == 0,
                "a versioned profile's deliberate Beveled choice remains explicit without per-slot metadata");
            snapshot = runtime->CaptureSettings();
            OverlayProfilePreferences(snapshot, settings_store::FromIniText("[player]\nenabled=false\n"));
            runtime->LoadSettings(snapshot);
            Check(module.bars_[0].style.frame == 0 && module.bars_[1].style.frame == 0 && module.bars_[2].style.fill == 0xFF123456,
                "unrelated profile overlays never reset slot migration metadata or appearance");
            const auto saved = runtime->CaptureSettings();
            Check(saved.ReadInt("actionbars.hotbar.slots", "frame_version", 0) == 1 && saved.ReadInt("actionbars.crossbar.slots", "frame_version", 0) == 1,
                "saving records each slot design's completed migration state");
        }
        static auto SlotTextDefaults(void) -> void
        {
            const auto theme = ui::theme::CreateDefault();
            actionbars_module defaults; defaults.ApplyTextDefaults(theme);
            for (const size_t index : {size_t{1}, size_t{2}, size_t{3}, size_t{5}})
            {
                const auto& style = defaults.text_[index].style;
                Check(style.top_color == 0xFFFFFFFF && style.bottom_color == 0xFFFFFFFF && style.shadow.enabled && style.stroke.enabled,
                    "slot labels, bindings, recasts and counts default to white text with stroke and shadow");
            }
            Check(defaults.text_[6].style.top_color == 0xFFBFE07D && defaults.text_[6].style.bottom_color == 0xFFBFE07D &&
                defaults.text_[6].style.stroke.enabled && defaults.text_[6].style.shadow.enabled,
                "MP costs default to the MP bar green with stroke and shadow");
            Check(defaults.bars_[0].style.mp_cost && defaults.bars_[action_bar_count].style.mp_cost,
                "MP cost is visible by default on hotbars and crossbar");

            auto oldSettings = settings_store::FromIniText("");
            SaveTextProfile(oldSettings, "text.actionbars.label", module_text_profile{theme.caption_text});
            SaveTextProfile(oldSettings, "text.actionbars.binding", module_text_profile{theme.caption_text});
            SaveTextProfile(oldSettings, "text.actionbars.timer", module_text_profile{theme.meter_text});
            auto custom = module_text_profile{theme.caption_text}; custom.style.top_color = 0xFF123456; custom.style.bottom_color = 0xFF654321;
            custom.style.shadow.enabled = custom.style.stroke.enabled = false; custom.style.glow = {true, 0xCC123456};
            SaveTextProfile(oldSettings, "text.actionbars.cost", custom);
            actionbars_module migrated; migrated.ApplyTextDefaults(theme); migrated.Load(oldSettings);
            for (const size_t index : {size_t{1}, size_t{2}, size_t{3}, size_t{5}})
            {
                const auto& style = migrated.text_[index].style;
                Check(style.top_color == 0xFFFFFFFF && style.bottom_color == 0xFFFFFFFF && style.stroke.enabled && style.shadow.enabled,
                    "only legacy default caption and timer styles migrate to the new legible white defaults");
            }
            Check(ProfileSnapshot(migrated.text_[6]) == ProfileSnapshot(custom), "white default migration preserves customized colors and effects");
            migrated.text_[2] = module_text_profile{theme.caption_text};
            auto saved = settings_store::FromIniText(""); migrated.Save(saved);
            Check(saved.ReadInt("actionbars", "slot_text_version", 0) == 2, "saving records completion of the slot text migration");
            actionbars_module restored; restored.ApplyTextDefaults(theme); restored.Load(settings_store::FromIniText(saved.ToIniText()));
            Check(ProfileSnapshot(restored.text_[2]) == ProfileSnapshot(migrated.text_[2]) && restored.text_[2].style.top_color == theme.caption_text.top_color,
                "a deliberately restored legacy color remains untouched after the versioned migration");

            auto previous = settings_store::FromIniText("[actionbars]\nslot_text_version=1\n[meter.player.mp]\nfill_bottom=FFA0DD68\n");
            auto cost = defaults.text_[6]; cost.style.top_color = cost.style.bottom_color = 0xFFFFFFFF;
            cost.style.scale = 0.933f; cost.style.font = 1; cost.style.shadow.offset_y = 2; cost.style.glow.enabled = true;
            SaveTextProfile(previous, "text.actionbars.cost", cost);
            actionbars_module matched; matched.ApplyTextDefaults(theme); matched.Load(previous);
            for (const size_t index : {size_t{1}, size_t{2}})
            {
                const auto& style = matched.text_[index].style;
                Check(style.scale == cost.style.scale && style.font == cost.style.font && style.top_color == 0xFFFFFFFF && style.bottom_color == 0xFFFFFFFF &&
                    style.stroke.enabled == cost.style.stroke.enabled && style.shadow.offset_y == cost.style.shadow.offset_y && style.glow.enabled == cost.style.glow.enabled,
                    "labels and macro keys copy the MP cost typography before recoloring MP");
            }
            Check(matched.text_[1].style.alignment == ui::text_alignment::center && matched.text_[2].style.alignment == ui::text_alignment::left &&
                matched.text_[6].style.top_color == 0xFFA0DD68 && matched.text_[6].style.bottom_color == 0xFFA0DD68,
                "matching text retains label/key alignment while MP costs use the configured MP bar color");
        }
        static auto SlotStyleActions(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            for (size_t index = 0; index < module.bars_.size(); ++index)
            {
                auto& bar = module.bars_[index];
                bar.state.bounds.x = 20 + static_cast<float>(index) * 11; bar.state.bounds.y = 60 + static_cast<float>(index) * 17;
                bar.state.enabled = index % 2 == 0; bar.size = 24 + static_cast<float>(index) * 2;
                bar.columns = 1 + static_cast<float>(index % 6); bar.slots = 12; bar.gap = static_cast<float>(index);
                bar.locked = index % 2 == 0; bar.heading = index % 3 == 0; bar.labels = index % 3 == 1;
                bar.bindings = index % 3 == 2; bar.resources = index % 2 == 1; bar.cooldowns = index % 2 == 0;
                bar.empty = index % 3 != 0; bar.background = index % 3 != 1; bar.tooltips = index % 3 != 2;
                bar.keys[0] = {static_cast<uint16_t>('A' + index), static_cast<uint8_t>(index % 8)};
                bar.keys[11] = {static_cast<uint16_t>('0' + index), static_cast<uint8_t>((index + 3) % 8)};
                module.Layout(index);
                if (index < action_bar_count) store.Assign(action_surface::hotbar, index, index, Command(("Action " + std::to_string(index)).c_str()));
            }
            for (size_t bank = 0; bank < action_crossbar_banks; ++bank)
                store.Assign(action_surface::crossbar, bank, bank, Command(("Bank " + std::to_string(bank)).c_str()));
            module.bars_[7].style = StyledSlots();
            const auto before = module.bars_; const auto revision = store.Revision();
            auto actionSettings = settings_store::FromIniText(""); store.Save(actionSettings); const auto actions = actionSettings.ToIniText();
            const auto preserved = [&]() {
                for (size_t index = 0; index < module.bars_.size(); ++index)
                {
                    const auto& original = before[index]; const auto& bar = module.bars_[index];
                    Check(bar.keys == original.keys && bar.state.bounds.x == original.state.bounds.x && bar.state.bounds.y == original.state.bounds.y &&
                        bar.state.bounds.width == original.state.bounds.width && bar.state.bounds.height == original.state.bounds.height &&
                        bar.columns == original.columns && bar.slots == original.slots && bar.size == original.size && bar.gap == original.gap &&
                        bar.state.enabled == original.state.enabled && bar.locked == original.locked && bar.heading == original.heading &&
                        bar.labels == original.labels && bar.bindings == original.bindings && bar.resources == original.resources && bar.cooldowns == original.cooldowns &&
                        bar.empty == original.empty && bar.background == original.background && bar.tooltips == original.tooltips,
                        "design actions preserve every bar's bindings, positions, dimensions, enabled state and presentation switches");
                }
                auto saved = settings_store::FromIniText(""); store.Save(saved);
                Check(store.Revision() == revision && saved.ToIniText() == actions, "design actions never modify assignments, action sets or storage selections");
            };
            module.SelectConfigView(7);
            Check(module.Option(actionbars_module::slot_apply_all).kind == module_option_kind::action &&
                module.Option(actionbars_module::slot_reset).kind == module_option_kind::action, "copy and reset use normal shared configuration actions");
            module.OnOptionChanged(actionbars_module::slot_apply_all);
            for (const auto& bar : module.bars_) Check(bar.style == before[7].style, "Apply design to all bars includes all ten hotbars and crossbar");
            Check(module.TakeSettingsChanged(), "copied designs participate in normal settings persistence");
            preserved();
            module.OnOptionChanged(actionbars_module::slot_reset);
            Check(module.bars_[7].style == actionbar_slot_style{} && module.bars_[0].style == before[7].style &&
                module.bars_[action_bar_count].style == before[7].style, "reset restores only the selected hotbar's design");
            preserved();
            module.SelectConfigView(action_bar_count); module.OnOptionChanged(actionbars_module::slot_reset);
            Check(module.bars_[action_bar_count].style == actionbar_slot_style{} && module.bars_[0].style == before[7].style,
                "crossbar design reset leaves hotbar designs intact");
            preserved();

            const auto sections = module.ConfigSections();
            constexpr const char* names[]{"General", "Controller Setup", "Layout", "Display", "Slots", "Effects", "Overlays", "Text"};
            Check(sections.size() == std::size(names), "slot styling has distinct configuration topics");
            for (size_t index = 0; index < sections.size(); ++index)
                Check(std::string{sections[index].name} == names[index], "slot topics follow the common configuration order");
            for (size_t index = actionbars_module::slot_frame; index < actionbars_module::base_options; ++index)
            {
                const auto option = module.Option(index); size_t topics{};
                for (size_t section = 0; section < sections.size(); ++section) if (module.ConfigSectionContains(section, index)) ++topics;
                Check(option.label != nullptr && topics == 1, "slot options retain one topic while conditional visibility changes");
            }
            Check(module.Option(actionbars_module::slot_frame).choice == &module.bars_[action_bar_count].style.frame &&
                module.Option(actionbars_module::slot_cost_x).slider == &module.bars_[action_bar_count].style.cost_x &&
                module.Option(actionbars_module::slot_mp_cost).toggle == &module.bars_[action_bar_count].style.mp_cost,
                "shared choices, sliders and toggles address the selected crossbar design");
            module.bars_[action_bar_count].style.frame = 0;
            for (const size_t index : {actionbars_module::slot_radius, actionbars_module::slot_fill, actionbars_module::slot_empty_fill, actionbars_module::slot_border, actionbars_module::slot_sheen})
                Check(module.Option(index).visible, "selecting a custom beveled frame restores its appearance controls");
            Check(module.Option(actionbars_module::slot_frame).choice_count == 5, "the native frame choice appends to the four existing stable frame values");
            module.SelectConfigView(7);
            Check(module.Option(actionbars_module::slot_frame).choice == &module.bars_[7].style.frame &&
                module.Option(actionbars_module::selected_text).choice_count == module.text_.size(), "hotbar style selection and all seven text roles remain reachable");
        }
        static auto SlotStyleGeometry(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module; ui::design_system design;
            module.Initialize(nullptr, &store, &executor); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.now_ = 1000;
            module.bars_[0].state.bounds.x = 40; module.bars_[0].state.bounds.y = 50;
            module.bars_[0].columns = 6; module.bars_[0].labels = true; module.bars_[0].keys[0] = {'Q', 1};
            module.bars_[action_bar_count].state.enabled = true;
            module.bars_[action_bar_count].state.bounds.x = 40; module.bars_[action_bar_count].state.bounds.y = 400;
            store.Assign(action_surface::hotbar, 0, 0, Command("Styled action"));
            store.Assign(action_surface::crossbar, 0, 0, Command("Styled crossbar"));
            action_availability availability; availability.known = availability.learned = availability.usable = availability.recast_known = true;
            availability.remaining = 25; availability.total = 60; availability.mp = 42; availability.charges = 2; availability.maximum_charges = 3;
            module.inspected_[0] = {availability, module.now_, store.Revision()};
            module.inspected_[256] = {availability, module.now_, store.Revision()};
            const module_render_context context{design, nullptr, true, nullptr, module.viewport_};
            for (const size_t bar : {size_t{0}, action_bar_count})
            {
                module.Layout(bar);
                const actionbars_module::address address{bar == action_bar_count ? action_surface::crossbar : action_surface::hotbar, 0, 0};
                const auto original = module.SlotBounds(address);
                for (int frame = 0; frame <= actionbar_slot_style::window_frame; ++frame)
                {
                    auto& style = module.bars_[bar].style; style = StyledSlots(); style.frame = frame; style.cooldown_style = frame % 4;
                    style.icon_opacity = 1; style.mp_cost = style.cooldown_text = true;
                    style.binding_x = -48; style.count_x = 48; style.timer_y = -48; style.cost_y = 48;
                    design.BeginContentBounds(); module.RenderBar(bar, context); const auto content = design.EndContentBounds();
                    const auto cell = module.SlotBounds(address); const auto hit = module.HitTest(cell.x + cell.width / 2, cell.y + cell.height / 2);
                    Check(std::isfinite(content.x) && std::isfinite(content.y) && std::isfinite(content.width) && std::isfinite(content.height) && content.width > 0 && content.height > 0,
                        "frame and overlay designs keep finite headless content bounds");
                    Check(cell.x == original.x && cell.y == original.y && cell.width == original.width && cell.height == original.height && hit && hit->header == 0 && hit->slot == address,
                        "frame, cooldown and text overlay designs preserve slot hit geometry");
                }
            }
        }
        static auto Keybinds(void) -> void
        {
            keyboard_restore restore;
            BYTE keys[256]{};
            SetKeyboardState(keys);
            action_store store;
            action_executor executor;
            actionbars_module module;
            module.Initialize(nullptr, &store, &executor);
            module.viewport_ = {0, 0, 1280, 900};
            for (auto& bar : module.bars_) bar.keys = {};
            const auto control = [&](const int id) {
                const auto controls = module.Controls();
                const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& item) { return item.id == id; });
                Check(found != controls.end(), "keybind control is present");
                return *found;
            };
            const auto choose = [&](const int id, const int value) {
                module.ActivateControl(id);
                Check(module.setup_choice_input_.Opened(), "keybind selectors use the shared dropdown");
                const auto found = std::find_if(module.setup_choices_.begin(), module.setup_choices_.end(), [&](const auto& item) { return item.first == value; });
                Check(found != module.setup_choices_.end(), "requested keybind choice is available");
                module.SelectSetupChoice(static_cast<int>(found - module.setup_choices_.begin()));
            };
            module.OpenKeybinds(false, 1);
            Check(module.FloatingWindowOpen() && module.WindowIndex() == 4 && control(1100).text == "Hotbar 2",
                "Keybinds opens its own workspace for the selected bar");
            Check(control(700).enabled && control(701).enabled && control(702).enabled && control(1211).text == "Unbound",
                "all twelve slots and shared move/resize/close controls are reachable");
            module.ActivateControl(1204);
            Check(module.binding_capture_ && module.selected_.slot == 4, "a slot enters explicit key capture");
            module.HandleKey(VK_F8, 0);
            Check(!module.binding_capture_ && module.bars_[1].keys[4].key == VK_F8 && module.bars_[0].keys[4].key == 0,
                "capture updates only the selected hotbar slot");
            Check(module.HandleKey(VK_F8, 0xC0000000u), "the captured key release is owned");
            module.ActivateControl(1205);
            module.HandleKey(VK_F8, 0);
            Check(module.binding_capture_ && module.bars_[1].keys[5].key == 0 && module.message_.find("Already bound") != std::string::npos,
                "conflicts preserve both bindings and keep capture available");
            module.HandleKey(VK_F8, 0xC0000000u);
            module.HandleKey(VK_ESCAPE, 0);
            module.HandleKey(VK_ESCAPE, 0xC0000000u);
            Check(!module.binding_capture_ && module.bars_[1].keys[5].key == 0, "Escape cancels capture without replacing a binding");
            module.ActivateControl(1304);
            Check(module.bars_[1].keys[4].key == 0 && !control(1304).enabled, "Clear unbinds that slot and disables itself");
            keys[VK_CONTROL] = keys[VK_SHIFT] = 0x80;
            SetKeyboardState(keys);
            module.ActivateControl(1204);
            module.HandleKey(VK_F9, 0);
            module.HandleKey(VK_F9, 0xC0000000u);
            Check(module.bars_[1].keys[4] == actionbar_binding{VK_F9, 5}, "captured combinations retain Ctrl and Shift");
            keys[VK_CONTROL] = keys[VK_SHIFT] = 0;
            SetKeyboardState(keys);
            module.workspace_bounds_[4].height = 360;
            module.focused_control_ = 1200;
            for (int row = 0; row < 11; ++row) module.HandleControllerAction(controller_action::down, false);
            Check(module.focused_control_ == 1211 && module.palette_scroll_ > 0 && control(1211).enabled,
                "controller navigation scrolls the final slot into view in compact windows");
            choose(1100, static_cast<int>(action_bar_count));
            Check(module.selected_.surface == action_surface::crossbar && control(623).kind == actionbars_module::control_kind::dropdown,
                "the bar selector opens controller slot mappings for Crossbar");
            choose(606, 0);
            const auto oldDirect = module.controller_settings_.direct_input;
            choose(623, 14);
            Check(module.controller_settings_.xinput.slots[0] == 14 && module.controller_settings_.direct_input == oldDirect,
                "crossbar rebinding updates only the selected input backend");
            choose(1101, 5);
            Check(module.selected_.bar == 5 && module.controller_settings_.xinput.slots[0] == 14,
                "trigger-bank previews retain the same physical button mapping");
            module.ActivateControl(1300);
            Check(module.controller_settings_.xinput.slots[0] == -1, "crossbar Clear uses the existing unbound mapping");
            choose(606, 1);
            choose(623, -41);
            Check(module.controller_settings_.direct_input.slots[0] == -41, "DirectInput supports the existing signed POV choices");
            module.ActivateControl(624);
            const auto prior = module.controller_settings_.direct_input.slots[1];
            module.HandleControllerAction(controller_action::down, false);
            module.HandleControllerAction(controller_action::cancel, false);
            Check(!module.setup_choice_input_.Opened() && module.DialogOpen() && module.controller_settings_.direct_input.slots[1] == prior,
                "Cancel dismisses mapping choices without applying their highlight");
            auto settings = settings_store::FromIniText("");
            module.Save(settings);
            actionbars_module loaded;
            loaded.Load(settings);
            Check(loaded.bars_[1].keys[4] == actionbar_binding{VK_F9, 5} && loaded.controller_settings_.direct_input.slots[0] == -41 &&
                loaded.controller_settings_.xinput.slots[0] == -1 && loaded.workspace_bounds_[4].height == 360,
                "keybinds and the independent workspace persist using existing settings paths");
            const auto portable = settings.Preferences("").ToIniText();
            Check(portable.find("controller.crossbar.device") == std::string::npos, "controller mappings remain local in portable profiles");
            module.CloseDialog();
            module.SelectConfigView(2);
            module.OnOptionChanged(actionbars_module::keybinds);
            Check(module.dialog_ == actionbars_module::dialog::keybinds && module.selected_.bar == 2 &&
                module.ConfigSectionContains(1, actionbars_module::keybinds), "the Keybinds topic opens the same workspace");
            auto runtime = std::make_unique<hud_runtime>();
            Check(runtime->HandleActionBarsCommand("crossbar keybinds") && runtime->actionbars_.dialog_ == actionbars_module::dialog::keybinds &&
                runtime->actionbars_.selected_.surface == action_surface::crossbar, "the crossbar keybinds command opens the same list");
        }
        static auto ControllerSetup(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.viewport_ = {0, 0, 1280, 900};
            module.controller_status_.backend = controller_backend::xinput;
            module.controller_settings_.enabled = module.controller_settings_.replace_native = false;
            module.bars_[action_bar_count].state.enabled = false; module.OpenSetup();
            const auto control = [&](const int id) {
                const auto controls = module.Controls();
                const auto found = std::find_if(controls.begin(), controls.end(), [&](const auto& value) { return value.id == id; });
                Check(found != controls.end(), "setup control is present"); return *found;
            };
            Check(control(650).text == "General" && control(651).text == "Mappings" && control(652).text == "Timing", "controller setup exposes three named tabs");
            const auto initial = module.Controls();
            Check(std::none_of(initial.begin(), initial.end(), [](const auto& value) { return value.text == "Next" || value.text == "Back"; }), "setup does not require a sequential Next or Back wizard");
            module.ActivateControl(600);
            Check(module.controller_settings_.enabled && module.controller_settings_.replace_native && module.trigger_controls_ && module.bars_[action_bar_count].state.enabled, "one trigger toggle enables XIUI input, explicit replacement and the crossbar surface");
            module.ActivateControl(600);
            Check(!module.controller_settings_.enabled && !module.controller_settings_.replace_native && !module.trigger_controls_, "the same trigger toggle releases both XIUI routing and native replacement");
            module.ActivateControl(651);
            Check(module.setup_step_ == 1, "Mappings tab opens directly");
            for (int id = 620; id < 631; ++id) Check(control(id).enabled, "all eleven trigger, shoulder and slot mapping controls are reachable");
            module.ActivateControl(620);
            Check(module.setup_choice_input_.Opened() && module.controller_settings_.xinput.left_trigger == -2, "opening a mapping dropdown preserves the saved binding");
            module.HandleControllerAction(controller_action::down, false);
            module.HandleControllerAction(controller_action::cancel, false);
            Check(!module.setup_choice_input_.Opened() && module.controller_settings_.xinput.left_trigger == -2 && module.DialogOpen(), "Cancel dismisses only the dropdown without applying its highlight");
            module.ActivateControl(620);
            const auto mapped = std::find_if(module.setup_choices_.begin(), module.setup_choices_.end(), [](const auto& entry) { return entry.first == 9; });
            const int mappedIndex = static_cast<int>(mapped - module.setup_choices_.begin());
            module.setup_choice_input_.Move(mappedIndex - module.setup_choice_input_.Highlighted());
            const auto mappedRow = module.setup_choice_input_.Row(mappedIndex, module.SetupViewport());
            Click(module, mappedRow);
            Check(module.controller_settings_.xinput.left_trigger == 9 && !module.setup_choice_input_.Opened() && !module.PointerDown(), "mapping selection translates a menu index to the raw binding and releases the pointer");
            module.ActivateControl(620);
            const auto menuViewport = module.SetupViewport();
            auto menuScroll = module.setup_choice_input_.Scrollbar(menuViewport);
            Check(menuScroll.Visible() && !module.setup_choice_input_.IndexAt(menuScroll.track.x + 6, menuScroll.track.y + 6, menuViewport), "a long dropdown reserves a separate scrollbar hit area");
            const auto thumb = menuScroll.Thumb();
            const float scrollX = thumb.x + thumb.width / 2, grabY = thumb.y + thumb.height / 2;
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, scrollX, grabY, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, scrollX, menuScroll.track.y + menuScroll.track.height + 100, false);
            module.HandleMouse(WM_LBUTTONUP, 0, scrollX, menuScroll.track.y + menuScroll.track.height + 100, false);
            Check(module.setup_choice_input_.First() == menuScroll.Maximum() && module.setup_choice_input_.Opened() && !module.setup_choice_input_.Scrolling() && !module.PointerDown() && module.controller_settings_.xinput.left_trigger == 9,
                "scrollbar dragging reaches the final entries without changing the binding or retaining pointer ownership");
            menuScroll = module.setup_choice_input_.Scrollbar(menuViewport);
            Click(module, {menuScroll.track.x, menuScroll.track.y, menuScroll.track.width, 2});
            Check(module.setup_choice_input_.First() < menuScroll.Maximum() && module.setup_choice_input_.Opened(), "scrollbar track clicks page the dropdown without selecting a mapping");
            module.HandleControllerAction(controller_action::down, false);
            Check(module.setup_choice_input_.Highlighted() >= module.setup_choice_input_.First() && module.setup_choice_input_.Highlighted() < module.setup_choice_input_.First() + module.setup_choice_input_.VisibleCount(), "controller navigation scrolls its highlighted option back into view");
            module.HandleControllerAction(controller_action::cancel, false);
            module.controller_settings_.double_tap = false; module.controller_settings_.palette_cycle = true;
            module.controller_settings_.palette_left_shoulder = false; module.controller_settings_.xinput.palette_modifier = 8;
            module.ActivateControl(652);
            Check(control(640).enabled && control(641).enabled, "Timing offers separate XInput press and release thresholds");
            const auto noDoubleTap = module.Controls();
            Check(std::none_of(noDoubleTap.begin(), noDoubleTap.end(), [](const auto& control) { return control.id == 642 || control.id == 643; }), "double-tap timing controls hide while double-tap banks are off");
            module.focused_control_ = 652; module.HandleControllerAction(controller_action::down, false);
            Check(module.focused_control_ != 642 && module.focused_control_ != 643 && control(module.focused_control_).enabled, "controller navigation skips disabled timing controls");
            Check(control(605).text == "Left shoulder", "palette shoulder label follows an explicit LB override rather than the fallback preference");
            module.ActivateControl(605);
            Check(module.setup_choice_input_.Opened() && module.controller_settings_.xinput.palette_modifier == 8, "opening the shoulder dropdown preserves the explicit override");
            module.SelectSetupChoice(1);
            Check(module.controller_settings_.xinput.palette_modifier == -2 && !module.controller_settings_.palette_left_shoulder && control(605).text == "Right shoulder", "changing palette shoulder clears the override and selects the right automatic shoulder");
            module.controller_settings_.double_tap = true;
            const auto timing = control(642);
            Check(timing.kind == actionbars_module::control_kind::slider, "timing uses a shared slider instead of a cycling button");
            const auto track = ui::config_layout::SliderTrack(timing.bounds, timing.label_width);
            const float trackY = track.y + track.height / 2;
            module.HandleMouse(WM_LBUTTONDOWN, MK_LBUTTON, track.x + track.width / 2, trackY, false);
            Check(module.controller_settings_.double_tap_ms == 550 && module.PointerDown(), "clicking the slider track applies its snapped value");
            module.HandleMouse(WM_LBUTTONUP, 0, track.x + track.width + 100, trackY, false);
            Check(module.controller_settings_.double_tap_ms == 1000 && !module.PointerDown() && module.setup_slider_drag_ == 0, "slider release outside the track clamps and ends its drag");
            module.focused_control_ = 642;
            module.HandleControllerAction(controller_action::confirm, false);
            module.HandleControllerAction(controller_action::left, true);
            Check(module.controller_settings_.double_tap_ms == 975, "controller slider adjustment uses the same step");
            module.HandleControllerAction(controller_action::cancel, false);
            Check(module.DialogOpen() && module.setup_slider_adjust_ == 0, "Cancel leaves adjustment before closing Setup");
            Click(module, ui::config_layout::SliderValue(timing.bounds));
            Check(module.setup_value_input_.Focused() && module.KeyboardCapture() && !module.PointerDown(), "the slider number opens shared numeric entry and owns keyboard input");
            module.setup_value_input_.Insert("317"); module.HandleKey(VK_RETURN, 0);
            Check(module.controller_settings_.double_tap_ms == 325 && !module.setup_value_input_.Focused(), "numeric slider entry shares snapping and commit behavior");
            module.SetSetupSlider(640, 1);
            Check(module.controller_settings_.trigger_press == 1 && module.controller_settings_.trigger_release == 0, "lowering Press keeps Release strictly below it");
            module.OpenSetupValue(control(642)); module.setup_value_input_.Insert("700");
            module.SetWindowFocused(false);
            Check(!module.KeyboardCapture() && !module.setup_value_input_.Focused() && module.controller_settings_.double_tap_ms == 700, "clicking away commits a valid number and releases keyboard ownership");
            module.SetWindowFocused(true);
            for (const auto size : {ui::point{620, 416}, ui::point{680, 548}, ui::point{900, 720}})
                for (int tab = 0; tab < 3; ++tab)
                {
                    const ui::rect bounds{20, 30, size.x, size.y};
                    module.setup_step_ = tab;
                    for (const auto& item : module.SetupControls(bounds))
                        Check(item.bounds.width > 0 && item.bounds.height > 0 && item.bounds.x >= bounds.x && item.bounds.y >= bounds.y + 34 &&
                            item.bounds.x + item.bounds.width <= bounds.x + bounds.width && item.bounds.y + item.bounds.height <= bounds.y + bounds.height,
                            "controller setup controls remain inside compact and enlarged windows");
                }
            Click(module, ControlBounds(module, "Done"));
            Check(!module.InputFocused() && !module.PointerDown() && !module.navigation_focus_ && !module.HandleKey('W', 0), "Done closes setup and returns native movement without retained mouse or browsing focus");
        }
        static auto RuntimeOwnership(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 60;
            module.bars_[0].heading = true; module.Layout(0); const auto bounds = module.PaletteBounds(0);
            Check(runtime->ActionBarsInputAvailable() && !runtime->KeyboardFocused(), "idle action bars keep actual runtime movement ownership clear");
            runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, bounds.x + bounds.width / 2, bounds.y + bounds.height / 2, false);
            runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, bounds.x + bounds.width / 2, bounds.y + bounds.height / 2, false);
            Check(runtime->KeyboardFocused(), "open palette dropdown temporarily owns runtime keyboard input");
            Check(runtime->HandleActionBarsKey(VK_ESCAPE, 0, false) && !runtime->KeyboardFocused(), "Escape closes dropdown and releases actual runtime movement gate");
            Check(runtime->HandleActionBarsKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false) && !runtime->HandleActionBarsKey('W', 0, false), "runtime drains owned Escape release and yields fresh movement");
            runtime->controller_surface_ = hud_runtime::controller_surface::party; module.bars_[0].keys[0] = {'Q', 0};
            Check(!runtime->HandleActionBarsKey('Q', 0, false), "an unrelated controller surface retains its keyboard controls");
            runtime->controller_surface_ = hud_runtime::controller_surface::none;
        }
        static auto NativeMacroKeyboardOwnership(void) -> void
        {
            actionbars_module preferences; const auto theme = ui::theme::CreateDefault(); preferences.ApplyTextDefaults(theme);
            Check(preferences.ReplaceNativeMacros() && std::string{preferences.Option(actionbars_module::native_keys).label} == "Replace native macros",
                "native macro replacement defaults on and has a global settings label");
            preferences.allow_native_keys_ = false; preferences.ResetDefaults(theme);
            Check(preferences.ReplaceNativeMacros(), "reset restores the native macro replacement default");
            preferences.Load(settings_store::FromIniText("[actionbars]\nreplace_native_keyboard_macros=false\n"));
            Check(preferences.ReplaceNativeMacros(), "the former explicit-binding permission does not disable the new global default");
            preferences.Load(settings_store::FromIniText("[actionbars]\nreplace_native_macros=false\n"));
            auto saved = settings_store::FromIniText(""); preferences.Save(saved);
            Check(!preferences.ReplaceNativeMacros() && !saved.ReadBool("actionbars", "replace_native_macros", true), "explicit native macro opt-out persists under the new setting key");
            actionbars_module restored; restored.ApplyTextDefaults(theme); restored.Load(saved);
            Check(!restored.ReplaceNativeMacros(), "loading an explicit opt-out restores native macro ownership");

            keyboard_restore restore; BYTE keys[256]{}; keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
            Check(SetKeyboardState(keys) != FALSE, "set Ctrl for native palette replacement");
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            module.bars_[0].keys[0] = {'2', 1};
            runtime->action_store_.Assign(action_surface::hotbar, 0, 0, Command("Cure II"));
            Check((runtime->NativeHudHiddenMask() & native_keyboard_macro_palette) != 0, "the default replacement hides native macro UI before a chord is pressed");
            runtime->visibility_context_.macro_available = runtime->visibility_context_.macro_palette = true;
            Check(module.ReplacesNativeMacroKey('2') && module.ReplacesNativeMacroKey('8') && runtime->ActionBarsNativePaletteOwned(),
                "default replacement owns assigned and unassigned native chords and a verified macro palette");
            for (WPARAM key = '0'; key <= '9'; ++key) Check(module.ReplacesNativeMacroKey(key), "every native Ctrl-number chord is replaced");
            Check(runtime->HandleActionBarsKey('2', 0, false) && module.NativeMacroKeysOwned() &&
                (runtime->NativeHudHiddenMask() & native_keyboard_macro_palette) != 0, "a custom Ctrl chord consumes the key and suppresses the native palette drawing");
            runtime->action_executor_.Clear();
            Check(runtime->HandleActionBarsKey('2', static_cast<LPARAM>(0x40000000u), false) && runtime->action_executor_.Status().empty(),
                "holding a configured chord consumes repeats without executing the action again");
            Check(runtime->HandleActionBarsKey('2', static_cast<LPARAM>(0x80000000u), false) && module.NativeMacroKeysOwned(),
                "digit release drains while palette ownership remains with the held modifier");
            Check(runtime->HandleActionBarsKey('8', 0, false) && module.NativeMacroKeysOwned() && runtime->action_executor_.Status().empty() &&
                (runtime->NativeHudHiddenMask() & native_keyboard_macro_palette) != 0, "an unassigned chord is consumed without executing or restoring native macro UI");
            Check(runtime->HandleActionBarsKey('8', static_cast<LPARAM>(0x40000000u), false) &&
                runtime->HandleActionBarsKey('8', static_cast<LPARAM>(0x80000000u), false) && !runtime->HandleActionBarsKey('8', static_cast<LPARAM>(0x80000000u), false),
                "unassigned native chords own held repeats and drain their release exactly once");
            runtime->HandleActionBarsKey(VK_CONTROL, static_cast<LPARAM>(0x80000000u), false);
            Check(!module.NativeMacroKeysOwned() && (runtime->NativeHudHiddenMask() & native_keyboard_macro_palette) != 0,
                "modifier release clears held state while global native macro replacement remains enabled");
            runtime->native_ui_.menu_visible = true;
            Check(!runtime->ActionBarsNativePaletteOwned(true) && !runtime->HandleActionBarsKey('2', 0, false),
                "an ordinary native command menu still owns Ctrl shortcuts even with a palette marker present");
            runtime->native_ui_.menu_visible = false; runtime->visibility_context_.macro_available = false;
            Check(!runtime->ActionBarsNativePaletteOwned(true), "unavailable native palette detection cannot grant a menu exception");
            module.available_[0] = false;
            runtime->action_executor_.Clear();
            Check(module.ReplacesNativeMacroKey('2') && runtime->HandleActionBarsKey('2', 0, false) && runtime->action_executor_.Status().empty(),
                "context-hidden bars do not execute their actions and still exclude native macro chords");
            runtime->HandleActionBarsKey('2', static_cast<LPARAM>(0x80000000u), false);
            module.available_[0] = true; runtime->action_store_.Assign(action_surface::hotbar, 0, 0, {});
            Check(module.ReplacesNativeMacroKey('2') && runtime->HandleActionBarsKey('2', 0, false) && runtime->action_executor_.Status().empty(),
                "an empty bound slot consumes its native chord without invoking the executor");
            runtime->HandleActionBarsKey('2', static_cast<LPARAM>(0x80000000u), false);
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0; keys[VK_MENU] = keys[VK_LMENU] = 0x80;
            Check(SetKeyboardState(keys) != FALSE && module.ReplacesNativeMacroKey('7') && runtime->HandleActionBarsKey('7', 0, false),
                "Alt-number chords follow the same global replacement policy");
            module.allow_native_keys_ = false; module.OnOptionChanged(actionbars_module::native_keys);
            Check(!module.ReplaceNativeMacros() && !module.NativeMacroKeysOwned() && (runtime->NativeHudHiddenMask() & native_keyboard_macro_palette) == 0 &&
                runtime->HandleActionBarsKey('7', static_cast<LPARAM>(0x80000000u), false) && !runtime->HandleActionBarsKey('7', static_cast<LPARAM>(0x80000000u), false),
                "opting out restores native UI while draining the chord already owned by XIUI");
            Check(!module.ReplacesNativeMacroKey('7') && !runtime->HandleActionBarsKey('7', 0, false), "fresh native macro chords pass through after opt-out");
            keys[VK_MENU] = keys[VK_LMENU] = 0; Check(SetKeyboardState(keys) != FALSE, "clear native macro modifiers");
            module.allow_native_keys_ = true;
            Check(!module.ReplacesNativeMacroKey('2') && !module.HandleKey('2', 0) && !module.HandleKey('W', 0), "ordinary digits and typing remain available without macro modifiers");
            module.OpenLibrary();
            keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80; Check(SetKeyboardState(keys) != FALSE, "restore Ctrl for focused-window guard");
            Check(!module.ReplacesNativeMacroKey('2'), "an explicitly focused XIUI workspace retains its keyboard controls");
            module.SetWindowFocused(false); runtime->action_executor_.Clear();
            Check(module.ReplacesNativeMacroKey('2') && module.HandleKey('2', 0) && runtime->action_executor_.Status().empty() &&
                module.HandleKey('2', static_cast<LPARAM>(0x80000000u)), "an unfocused open workspace still excludes native chords without activating an action");
        }
        static auto EditModeManagement(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 60;
            runtime->editing_ = true; runtime->config_bounds_ = {700, 500, 500, 350}; module.Layout(0);
            Check(runtime->ActionBarsInputAvailable() && !runtime->ActionBarsExecutionAvailable(), "edit mode permits management while action execution stays suspended");
            const auto menu = module.MenuBounds(0);
            Check(runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, menu.x + 8, menu.y + 8, false) &&
                runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, menu.x + 8, menu.y + 8, false) && module.dialog_ == actionbars_module::dialog::bar_menu,
                "the bar menu opens directly while editing");
            runtime->HandleActionBarsKey(VK_ESCAPE, 0, false); runtime->HandleActionBarsKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
            for (const size_t destination : {11, 12})
            {
                runtime->mouse_down_ = true; runtime->mouse_buttons_ = MK_LBUTTON;
                runtime->SelectConfigModule(&module, destination);
                Check(module.DialogOpen() && !runtime->mouse_down_ && runtime->mouse_buttons_ == 0,
                    "configuration opens management and finishes its pointer gesture");
                const auto window = module.WindowBounds();
                runtime->mouse_x_ = static_cast<LONG>(window.x + 30); runtime->mouse_y_ = static_cast<LONG>(window.y + 60);
                Check(runtime->UpdateMouseCapture({0, 0, 1280, 900, 0, 1}, 0) && runtime->mouse_captured_,
                    "action management joins shared mouse capture so the edit-mode cursor remains visible");
                if (destination == 11) { module.BeginField(1); module.input_.Insert("Cure"); }
                module.active_bank_ = crossbar_bank::left; module.triggers_ = 1; module.held_slots_[0][0] = true;
                controller_status unavailable{}; unavailable.suspended = unavailable.enumeration_disabled = unavailable.connection_known = true;
                runtime->UpdateControllerStatus(unavailable); runtime->UpdateControllerStatus(unavailable);
                Check(module.DialogOpen() && module.active_bank_ == crossbar_bank::none && !module.held_slots_[0][0],
                    "unavailable controller status clears held triggers without dismissing mouse or keyboard management");
                if (destination == 11)
                {
                    Check(module.KeyboardFocused() && module.input_.Editor().Text().find("Cure") != std::string::npos, "controller status preserves an active search draft");
                    runtime->HandleActionBarsKey(VK_ESCAPE, 0, false); runtime->HandleActionBarsKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
                }
                runtime->HandleActionBarsKey(VK_ESCAPE, 0, false); runtime->HandleActionBarsKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
                Check(!module.InputFocused() && runtime->editing_, "closing management returns to edit mode");
            }
            const auto slot = module.SlotBounds({action_surface::hotbar, 0, 0});
            Check(!runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, slot.x + 8, slot.y + 8, false) && !module.PointerDown(), "slot left-drag remains available for positioning the bar in edit mode");
            module.bars_[0].keys[0] = {'Q', 0};
            Check(!runtime->HandleActionBarsKey('Q', 0, false), "idle edit mode leaves global action bindings to configuration");
            runtime->config_bounds_ = {menu.x - 20, menu.y - 20, 400, 300};
            Check(!runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, menu.x + 8, menu.y + 8, false) && !module.DialogOpen(), "configuration takes pointer priority over a bar behind it");
        }
        static auto DependentSettings(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            runtime->SelectConfigModule(&module, action_bar_count); runtime->active_config_section_ = 4;
            const auto shown = [&](const size_t index) { const auto slots = runtime->GroupOptionSlots(); return std::find(slots.begin(), slots.end(), index) != slots.end(); };
            auto& style = module.bars_[action_bar_count].style;
            style.frame = actionbar_slot_style::window_frame; style.radius = 17;
            Check(!shown(actionbars_module::slot_radius) && !shown(actionbars_module::slot_fill) && !shown(actionbars_module::slot_shadow) &&
                shown(actionbars_module::slot_frame) && shown(actionbars_module::slot_border_width), "window frames hide custom appearance while retaining their actual frame controls");
            const auto items = runtime->ControllerConfigItems(config_controller_region::options);
            Check(std::none_of(items.begin(), items.end(), [](const auto& item) { return item.index == actionbars_module::slot_radius || item.index == actionbars_module::slot_fill; }),
                "hidden settings cannot receive controller focus");
            style.frame = 0;
            Check(shown(actionbars_module::slot_radius) && shown(actionbars_module::slot_fill) && style.radius == 17,
                "changing frame style restores custom controls and their saved values");
            const auto radius = module.Option(actionbars_module::slot_radius);
            runtime->slider_input_.Open(radius); runtime->slider_input_module_ = &module;
            runtime->slider_input_option_index_ = actionbars_module::slot_radius;
            style.frame = actionbar_slot_style::window_frame; runtime->NormalizeConfigSelection();
            Check(!runtime->slider_input_.Focused() && style.radius == 17, "hiding an edited setting cancels its pending numeric input without changing the value");
            runtime->BuildSettingsSearch();
            Check(std::none_of(runtime->settings_search_entries_.begin(), runtime->settings_search_entries_.end(), [](const auto& entry) {
                return entry.module == "hotbar" && entry.view == action_bar_count && entry.option == actionbars_module::slot_radius;
            }), "settings search excludes options hidden by the captured frame setting");
            module_text_profile profile{}; profile.style.shadow.enabled = false;
            Check(!TextProfileOption(8, profile).visible && !TextProfileOption(9, profile).visible && TextProfileOption(7, profile).visible,
                "text shadow controls hide while their enabling toggle remains visible");
            profile.style.shadow.enabled = true;
            Check(TextProfileOption(8, profile).visible, "enabling an effect reveals its saved customization");
            config_controller_focus focus; focus.id = "removed"; focus.adjusting = true;
            focus.Resolve({{"remaining", {0, 0, 100, 28}, 1, ""}});
            Check(focus.id == "remaining" && !focus.adjusting, "removing a focused setting exits adjustment before choosing another control");
            runtime->SelectConfigModule(&runtime->general_); runtime->active_config_section_ = general_module::input_section;
            const auto diagnostic = runtime->general_.InputStart() + 1;
            Check(shown(diagnostic) && !runtime->general_.Option(diagnostic).available, "unavailable capability diagnostics remain visible");
        }
        static auto WorkspaceCursors(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            module.viewport_ = {0, 0, 1280, 900}; runtime->editing_ = true;
            runtime->config_bounds_ = {850, 20, 350, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 900, 0, 1};
            const auto cursor = [&](const ui::rect& bounds) {
                runtime->mouse_x_ = static_cast<int>(bounds.x + bounds.width / 2);
                runtime->mouse_y_ = static_cast<int>(bounds.y + bounds.height / 2);
                return runtime->ResolveCursorShape(viewport);
            };
            using shape = cursor_renderer::shape;
            module.OpenLibrary();
            Check(cursor(ControlBounds(module, "New macro")) == shape::hand, "the action library uses the hand cursor on buttons");
            Check(cursor(module.WindowTitleBounds()) == shape::move && cursor(module.WindowCloseBounds()) == shape::hand &&
                cursor(module.WindowResizeBounds()) == shape::resize_diagonal, "popout title, close button and resize grip use their matching cursors");
            module.NewDraft(action_kind::command);
            Check(cursor(ControlBounds(module, "Choose action...")) == shape::hand, "the action editor shares button hover cursors");
            module.OpenPalettes();
            Check(cursor(ControlBounds(module, "New palette")) == shape::hand, "Action Sets shares button hover cursors");
            module.OpenImport();
            Check(cursor(ControlBounds(module, "Read and preview")) == shape::hand && cursor(ControlBounds(module, "Cancel")) == shape::hand,
                "Import shares button hover cursors");
            const auto importControls = module.Controls();
            const auto disabledImport = std::find_if(importControls.begin(), importControls.end(), [](const auto& control) { return control.id == 531; });
            Check(disabledImport != importControls.end() && !disabledImport->enabled && cursor(disabledImport->bounds) == shape::arrow,
                "a genuinely unavailable popout action retains the arrow cursor");
            module.OpenSetup();
            Check(cursor(ControlBounds(module, "Done")) == shape::hand && cursor(ControlBounds(module, "General")) == shape::hand,
                "Controller Setup buttons and tabs use the hand cursor");
            module.ActivateControl(652); module.controller_settings_.double_tap = false;
            const auto slider = [&]() {
                const auto controls = module.Controls();
                return *std::find_if(controls.begin(), controls.end(), [](const auto& control) { return control.id == 642; });
            };
            module.controller_settings_.double_tap = true;
            Check(cursor(ui::config_layout::SliderTrack(slider().bounds, slider().label_width)) == shape::resize_horizontal &&
                cursor(ui::config_layout::SliderValue(slider().bounds)) == shape::hand, "popout sliders distinguish dragging from numeric entry");
            module.ActivateControl(651); module.ActivateControl(620);
            Check(cursor(module.setup_choice_input_.Row(module.setup_choice_input_.Highlighted(), viewport)) == shape::hand &&
                cursor(module.setup_choice_input_.Scrollbar(viewport).track) == shape::hand, "dropdown rows and scrollbars share the hand cursor");
            module.SuspendInput();
            Check(!runtime->ActionBarsPointerOwns(static_cast<float>(runtime->mouse_x_), static_cast<float>(runtime->mouse_y_)),
                "closing popouts releases their cursor ownership");
        }
        static auto FloatingWorkspaces(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_); module.ApplyTextDefaults(ui::theme::CreateDefault());
            module.viewport_ = {0, 0, 1280, 900}; module.bars_[0].state.bounds.x = 80; module.bars_[0].state.bounds.y = 780; module.Layout(0);
            runtime->editing_ = true; runtime->config_bounds_ = {850, 20, 350, 600};
            module.OpenLibrary();
            const auto original = module.WindowBounds(); const auto bar = module.bars_[0].state.bounds;
            Check(module.FloatingWindowOpen() && original.width < 1000 && original.height < 800 && !runtime->KeyboardFocused() &&
                !runtime->HandleActionBarsKey('W', 0, false), "a compact mouse-opened spellbook leaves unrelated movement and typing native");
            const auto title = module.WindowTitleBounds();
            Check(runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, title.x + 80, title.y + 10, false), "window title starts a drag");
            runtime->HandleActionBarsMouse(WM_MOUSEMOVE, MK_LBUTTON, title.x + 160, title.y + 60, false);
            runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, title.x + 160, title.y + 60, false);
            auto bounds = module.WindowBounds();
            Check(bounds.x > original.x && bounds.y > original.y && module.bars_[0].state.bounds.x == bar.x && module.bars_[0].state.bounds.y == bar.y,
                "moving the spellbook changes only its own window bounds");
            const auto grip = module.WindowResizeBounds();
            runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, grip.x + grip.width - 3, grip.y + grip.height - 3, false);
            runtime->HandleActionBarsMouse(WM_MOUSEMOVE, MK_LBUTTON, grip.x + grip.width + 77, grip.y + grip.height + 57, false);
            runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, grip.x + grip.width + 77, grip.y + grip.height + 57, false);
            const auto resized = module.WindowBounds();
            Check(resized.width > bounds.width && resized.height > bounds.height && !module.WindowResizing() && !module.PointerDown(),
                "resizing uses an owned gesture and releases it on mouse-up");
            auto settings = settings_store::FromIniText(""); module.Save(settings);
            actionbars_module restored; restored.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_); restored.viewport_ = module.viewport_;
            restored.Load(settings); restored.OpenLibrary();
            Check(restored.WindowBounds().x == resized.x && restored.WindowBounds().width == resized.width, "spellbook bounds persist independently of hotbars");
            module.ActivateControl(Control(module, "Move window")); module.HandleControllerAction(controller_action::right, false);
            module.HandleControllerAction(controller_action::cancel, false);
            Check(module.WindowBounds().x == resized.x && !module.WindowDragging(), "controller Cancel rolls back only the window move");
            module.ActivateControl(Control(module, "Resize window"));
            Check(module.WindowResizeEdgesAt(1000, 100) == 0 && !runtime->ActionBarsPointerOwns(1000, 100),
                "controller resizing does not make the spellbook own the entire screen");
            module.HandleControllerAction(controller_action::cancel, false);
            module.browsing_ = module.navigation_focus_ = true;
            Check(runtime->HandleActionBarsMouse(WM_RBUTTONDOWN, 0, resized.x + 30, resized.y + 60, false) && module.FloatingWindowOpen() && !module.browsing_,
                "right-click mouse takeover preserves a controller-opened spellbook");
            runtime->HandleActionBarsMouse(WM_RBUTTONUP, 0, resized.x + 30, resized.y + 60, false);
            module.SetWindowFocused(true); module.BeginField(1); module.input_.Insert("Cure");
            Check(!runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, 1000, 100, false) && module.FloatingWindowOpen() && !module.WindowFocused() && !module.KeyboardFocused(),
                "clicking Configuration blurs the spellbook without closing it or retaining text focus");
            Check(module.query_ == "Cure", "blurring preserves the search draft");
            runtime->mouse_down_ = true;
            Check(!runtime->HandleActionBarsMouse(WM_MOUSEMOVE, MK_LBUTTON, resized.x + 60, resized.y + 60, false) && module.FloatingWindowOpen() &&
                !runtime->ActionBarsPointerOwns(resized.x + 60, resized.y + 60),
                "an existing Configuration gesture keeps its motion and release");
            runtime->mouse_down_ = false;
            Check(runtime->SetEditing(false, false) && module.FloatingWindowOpen(), "closing Configuration leaves the spellbook open");
            runtime->editing_ = true; module.OpenLibrary(); module.results_ = {Command("Dragged action")};
            auto source = ControlBounds(module, "Dragged action"); auto target = module.SlotBounds({action_surface::hotbar, 0, 0});
            runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, source.x + 20, source.y + 15, false);
            runtime->HandleActionBarsMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, target.x + 10, target.y + 10, false);
            Check(runtime->action_store_.Resolve(action_surface::hotbar, 0, 0).name == "Dragged action" && module.FloatingWindowOpen(),
                "a library drag reaches an exposed hotbar and keeps the spellbook open");
            source = ControlBounds(module, "Dragged action"); target = module.SlotBounds({action_surface::hotbar, 0, 1});
            runtime->config_bounds_ = {target.x - 10, target.y - 10, 200, 100};
            runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, source.x + 20, source.y + 15, false);
            runtime->HandleActionBarsMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, target.x + 10, target.y + 10, false);
            Check(runtime->action_store_.Resolve(action_surface::hotbar, 0, 1).Empty() && module.FloatingWindowOpen() && !module.PointerDown() && !module.discard_pointer_release_,
                "dropping over Configuration cannot assign a hidden hotbar or retain a stale release");
        }
        static auto LibraryCategories(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            module.viewport_ = {0, 0, 1280, 900}; runtime->editing_ = true; runtime->config_bounds_ = {1000, 20, 250, 600};
            module.OpenLibrary();
            const std::pair<const char*, int> categories[]{{"All actions", 0}, {"Spells", 1}, {"Abilities", 2}, {"Weapon skills", 3}, {"Items", 4},
                {"Equipment", 5}, {"Pet commands", 6}, {"Macros", 7}, {"Commands", 8}};
            for (const auto [label, category] : categories)
            {
                module.results_.clear();
                for (size_t index = 0; index < 220; ++index) module.results_.push_back(Command(("Sentinel " + std::to_string(index)).c_str()));
                module.result_scroll_ = 20;
                const auto controls = module.Controls();
                for (size_t index = 0; index < controls.size(); ++index)
                    for (size_t other = index + 1; other < controls.size(); ++other)
                        Check(controls[index].id != controls[other].id, "library controls have unique IDs even with hundreds of results");
                const auto categoryControl = std::find_if(controls.begin(), controls.end(), [&](const auto& value) {
                    return value.kind == actionbars_module::control_kind::list && value.text == label;
                });
                Check(categoryControl != controls.end(), "category has a distinct navigation control");
                const auto target = categoryControl->bounds; const float x = target.x + target.width / 2, y = target.y + target.height / 2;
                Check(runtime->HandleActionBarsMouse(WM_LBUTTONDOWN, 0, x, y, false) && runtime->HandleActionBarsMouse(WM_LBUTTONUP, 0, x, y, false),
                    "category receives its complete runtime mouse gesture");
                Check(module.category_ == category && module.dialog_ == actionbars_module::dialog::picker && module.WindowFocused() && !module.PointerDown() &&
                    !module.AssignmentDragging() && !module.placing_ && module.result_scroll_ == 0 && runtime->action_store_.Resolve(action_surface::hotbar, 0, 0).Empty(),
                    "category selection filters the list without choosing a result, closing the spellbook or changing assignments");
            }
        }
        static auto LibraryLayoutAndSearch(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module; ui::design_system design;
            module.Initialize(nullptr, &store, &executor); module.viewport_ = {0, 0, 1280, 900}; module.OpenLibrary();
            ui::config_text_settings text; module.SetConfigText(&text);
            for (const float size : {8.0f, 14.0f, 18.0f})
                for (const float spacing : {0.0f, 4.0f})
                    for (const ui::point dimensions : {ui::point{520, 420}, ui::point{640, 480}, ui::point{900, 700}})
                    {
                        text.size = size; text.letter_spacing = spacing; module.UpdateLibraryMetrics(design);
                        module.workspace_bounds_[0] = {30, 50, dimensions.x, dimensions.y};
                        module.results_.assign(40, Command("A very long spell or shared macro name"));
                        const auto layout = module.LibraryLayout(); const auto controls = module.Controls();
                        Check(layout.rows == module.PickerRows() && layout.rows > 0 && layout.row_height >= layout.name_height + layout.caption_height + 8,
                            "library row counts reserve the measured name and caption heights");
                        Check(layout.columns == 2 && static_cast<size_t>(std::count_if(controls.begin(), controls.end(), [](const auto& value) { return value.id >= 10000; })) ==
                            std::min(module.results_.size(), layout.rows * layout.columns), "library pages contain two action cards per physical row");
                        Check(layout.list.y + layout.rows * layout.row_pitch <= layout.footer.y + 0.1f && layout.search.y >= layout.bounds.y + 30,
                            "list and search geometry stay below chrome and above the footer");
                        Check(layout.your_actions.y == layout.all_actions.y && layout.all_actions.y == layout.search.y && layout.search.width >= 120 &&
                            layout.your_actions.width >= module.library_metrics_.filter_width && layout.all_actions.width >= module.library_metrics_.filter_width,
                            "filters and usable search share one row without clipping their labels");
                        for (size_t index = 0; index < controls.size(); ++index)
                        {
                            const auto& current = controls[index]; if (current.id >= 700 && current.id <= 702) continue;
                            Check(current.bounds.width > 0 && current.bounds.height > 0 && current.bounds.x >= layout.bounds.x && current.bounds.y >= layout.bounds.y + 30 &&
                                current.bounds.x + current.bounds.width <= layout.bounds.x + layout.bounds.width && current.bounds.y + current.bounds.height <= layout.bounds.y + layout.bounds.height,
                                "visible library controls fit inside the compact window at supported text sizes");
                            if (current.id >= 10000)
                            {
                                const auto offset = static_cast<size_t>(current.id - 10000) - module.result_scroll_ * layout.columns;
                                Check(std::abs(current.bounds.x - layout.list.x - static_cast<float>(offset % layout.columns) * layout.column_pitch) < 0.1f &&
                                    std::abs(current.bounds.y - layout.list.y - static_cast<float>(offset / layout.columns) * layout.row_pitch) < 0.1f,
                                    "action cards keep row-major geometry at every supported text size");
                            }
                            for (size_t other = index + 1; other < controls.size(); ++other)
                            {
                                if (controls[other].id >= 700 && controls[other].id <= 702) continue;
                                Check(!Overlaps(current.bounds, controls[other].bounds), "library search, filters, categories, results and buttons never overlap");
                            }
                        }
                    }
            module.workspace_bounds_[0] = {30, 50, 640, 480};
            const auto filters = module.Controls();
            const auto all = std::find_if(filters.begin(), filters.end(), [](const auto& value) { return value.kind == actionbars_module::control_kind::button && value.text == "All actions"; });
            Check(all != filters.end(), "all-actions filter is distinct from the category with the same name");
            Click(module, all->bounds); Check(!module.only_learned_ && module.FloatingWindowOpen(), "All actions switches the filter without activating an assignment");
            Click(module, ControlBounds(module, "Your actions")); Check(module.only_learned_, "Your actions explicitly selects the learned filter");
            keyboard_restore restore; BYTE keys[256]{}; keys[VK_CONTROL] = keys[VK_LCONTROL] = 0x80;
            Check(SetKeyboardState(keys) != FALSE && module.HandleKey('F', 0) && module.KeyboardFocused(), "Ctrl+F focuses the visible search field");
            module.HandleKey('F', static_cast<LPARAM>(0x80000000u)); module.HandleKey(VK_ESCAPE, 0); module.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u));
            Check(!module.KeyboardFocused() && module.FloatingWindowOpen() && !module.HandleKey('W', 0), "Escape leaves search and releases ordinary input while keeping the book open");
        }
        static auto LibraryGridNavigation(void) -> void
        {
            action_store store; action_executor executor; actionbars_module module;
            module.Initialize(nullptr, &store, &executor); module.viewport_ = {0, 0, 1280, 900}; module.OpenLibrary();
            module.bars_[0].state.bounds.x = 100; module.bars_[0].state.bounds.y = 800; module.Layout(0);
            const auto layout = module.LibraryLayout(); const auto count = layout.rows * layout.columns + 3;
            module.results_.clear();
            for (size_t index = 0; index < count; ++index) module.results_.push_back(Command(("Grid " + std::to_string(index)).c_str()));
            const int maximum = static_cast<int>((count + layout.columns - 1) / layout.columns - layout.rows);
            Check(module.Scrollbar().Maximum() == maximum && maximum == 2, "an odd final action occupies one additional physical scrollbar row");
            module.SetScroll(999);
            auto controls = module.Controls();
            const auto first = std::find_if(controls.begin(), controls.end(), [](const auto& value) { return value.id >= 10000; });
            const auto last = std::find_if(controls.begin(), controls.end(), [&](const auto& value) { return value.id == 10000 + static_cast<int>(count - 1); });
            Check(module.result_scroll_ == static_cast<size_t>(maximum) && first != controls.end() && first->id == 10000 + maximum * static_cast<int>(layout.columns) &&
                last != controls.end() && std::abs(last->bounds.x - layout.list.x) < 0.1f && last->bounds.y + last->bounds.height <= layout.footer.y,
                "scrolling ends on aligned rows and exposes the single left card in the odd final row");

            module.SetScroll(0); module.focused_control_ = 10001; module.focused_result_ = 1;
            module.MoveControl(0, 1);
            Check(module.focused_control_ == 10003 && module.focused_result_ == 3, "Down preserves the second result column");
            module.MoveControl(-1, 0);
            Check(module.focused_control_ == 10002 && module.focused_result_ == 2, "Left selects the other card in the same result row");
            module.MoveControl(1, 0); module.MoveControl(0, -1);
            Check(module.focused_control_ == 10001 && module.focused_result_ == 1, "Right and Up return through the same grid column");
            for (size_t row = 0; row < layout.rows; ++row) module.MoveControl(0, 1);
            Check(module.focused_result_ == layout.rows * layout.columns + 1 && module.result_scroll_ == 1,
                "controller navigation scrolls one physical row while retaining the second column");
            module.MoveControl(0, 1);
            Check(module.focused_control_ < 10000 && !module.placing_, "the missing final right card exits spatially instead of jumping into the left column");

            module.SetScroll(0); module.focused_control_ = 10000 + static_cast<int>(layout.columns * 2); module.focused_result_ = layout.columns * 2;
            module.MoveControl(-1, 0);
            Check(module.focused_control_ >= 200 && module.focused_control_ < 209, "Left from the first result column reaches the category sidebar");
            module.MoveControl(1, 0);
            Check(module.focused_control_ >= 10000 && module.focused_result_ == static_cast<size_t>(module.focused_control_ - 10000),
                "spatial entry into a result synchronizes its stable item focus");
            module.focused_control_ = 10001; module.focused_result_ = 1; module.SetScroll(maximum);
            Check(module.focused_result_ == static_cast<size_t>(maximum) * layout.columns + 1 && module.focused_control_ == 10000 + static_cast<int>(module.focused_result_),
                "pointer scrolling keeps the active result visible in the same column");
            module.focused_control_ = 534; module.focused_result_ = 0;
            module.HandleControllerAction(controller_action::next_tab, false);
            Check(module.focused_result_ == module.result_scroll_ * layout.columns && module.focused_control_ == 10000 + static_cast<int>(module.focused_result_),
                "Tab entering the scrolled result page synchronizes the first visible item");
            module.HandleControllerAction(controller_action::next_tab, false);
            Check(module.focused_result_ == module.result_scroll_ * layout.columns + 1, "Tab moves across both visible result columns");
            module.focused_control_ = 560; module.HandleControllerAction(controller_action::previous_tab, false);
            Check(module.focused_result_ == count - 1 && module.focused_control_ == 10000 + static_cast<int>(count - 1),
                "reverse Tab enters the existing card in an odd final row");

            module.SetScroll(1); controls = module.Controls();
            const auto source = std::find_if(controls.begin(), controls.end(), [](const auto& value) { return value.id == 10003; });
            Check(source != controls.end(), "second-column drag source remains addressable after row scrolling");
            const auto target = module.SlotBounds({action_surface::hotbar, 0, 5});
            module.HandleMouse(WM_LBUTTONDOWN, 0, source->bounds.x + source->bounds.width / 2, source->bounds.y + source->bounds.height / 2, false);
            module.HandleMouse(WM_MOUSEMOVE, MK_LBUTTON, target.x + 10, target.y + 10, false);
            module.HandleMouse(WM_LBUTTONUP, 0, target.x + 10, target.y + 10, false);
            Check(store.Resolve(action_surface::hotbar, 0, 5).name == "Grid 3" && store.Resolve(action_surface::hotbar, 0, 0).Empty() &&
                module.FloatingWindowOpen() && !module.AssignmentDragging() && !module.PointerDown(), "dragging a scrolled second-column card assigns that exact action once");
        }
        static auto Configuration(void) -> void
        {
            auto runtime = std::make_unique<hud_runtime>(); auto& module = runtime->actionbars_;
            module.Initialize(nullptr, &runtime->action_store_, &runtime->action_executor_);
            runtime->SelectConfigModule(&module, 7);
            auto page = runtime->ActiveConfigPage();
            Check(page.SectionCount() == 5 && std::string{page.SectionName(0)} == "Hotbars" && std::string{page.SectionName(1)} == "Crossbar" && page.SectionView(1) == action_bar_count &&
                std::string{page.SectionName(2)} == "Actions & Macros" && std::string{page.SectionName(3)} == "Action Sets" && std::string{page.SectionName(4)} == "Import",
                "action-bar navigation exposes bar settings and three workspace destinations");
            auto selector = module.Option(actionbars_module::bar_selector);
            Check(selector.selector == module_selector::element && selector.choice && selector.choice_count == 10 && *selector.choice == 7,
                "normal bars use shared direct buttons for all ten hotbars");
            *selector.choice = 2; module.OnOptionChanged(actionbars_module::bar_selector);
            Check(module.ConfigView() == 2 && runtime->ActiveConfigPage().SectionView(0) == 2, "the selected normal bar remains the active configuration view");
            runtime->active_config_section_ = 2;
            Check(runtime->ElementSelector().count == 10 && runtime->ElementSelector().selected == 2 &&
                !module.ConfigSectionContains(2, actionbars_module::bank), "hotbar layout shows direct hotbar buttons without trigger-bank tabs");
            Check(std::string{module.ConfigSections()[0].name} == "Actions" && std::string{module.ConfigSections()[1].name} == "Keybinds" &&
                !module.ConfigSectionContains(0, actionbars_module::keybinds), "hotbar Keybinds appears below Actions in the sidebar");
            runtime->SelectConfigTopic(1);
            Check(module.FloatingWindowOpen() && module.dialog_ == actionbars_module::dialog::keybinds && module.selected_.bar == 2,
                "activating the hotbar Keybinds topic opens its slot list directly");
            module.CloseDialog();
            runtime->SelectConfigModule(&module, action_bar_count);
            Check(std::string{module.ConfigSections()[0].name} == "General" && std::string{module.ConfigSections()[1].name} == "Controller Setup" &&
                module.ConfigSectionContains(0, actionbars_module::enabled) && module.ConfigSectionContains(1, actionbars_module::setup) && !module.ConfigSectionContains(0, actionbars_module::setup),
                "crossbar General is first and Controller Setup is second with the matching controls");
            Check(std::string{module.ConfigSections()[2].name} == "Keybinds" && !module.ConfigSectionContains(0, actionbars_module::keybinds) &&
                !module.ConfigSectionContains(1, actionbars_module::keybinds), "crossbar Keybinds appears below Controller Setup in the sidebar");
            runtime->SelectConfigTopic(2);
            Check(module.dialog_ == actionbars_module::dialog::keybinds && module.selected_.surface == action_surface::crossbar,
                "activating the crossbar Keybinds topic opens the controller slot list");
            module.CloseDialog();
            runtime->active_config_section_ = 3;
            Check(runtime->ElementSelector().count == 6, "crossbar layout retains all six trigger-bank tabs");
            Check(runtime->ActiveConfigPage().SectionContains(1, &module) && !module.ConfigSectionContains(0, actionbars_module::bar_selector),
                "crossbar controls keep their surface tab and hide the normal-bar selector");
            Check(!module.Option(actionbars_module::edit).available && !module.Option(actionbars_module::palettes).available && !module.Option(actionbars_module::import).available,
                "workspace destinations are not duplicated as settings rows");
            auto legacy = settings_store::FromIniText("[session.config]\nmodule=hotbar\nview=2\nsection=1\n");
            runtime->LoadConfigSelection(legacy);
            Check(runtime->active_config_section_ == 2 && std::string{module.ConfigSections()[2].name} == "Layout", "old hotbar topic selections retain their destination");
            legacy.WriteInt("session.config", "view", static_cast<int>(action_bar_count));
            legacy.WriteInt("session.config", "section", 2);
            runtime->LoadConfigSelection(legacy);
            Check(runtime->active_config_section_ == 3 && std::string{module.ConfigSections()[3].name} == "Layout", "old crossbar topic selections retain their destination");
            runtime->active_config_section_ = 2;
            runtime->SaveConfigSelection(legacy);
            legacy.WriteInt("session.config", "section", 8);
            runtime->LoadConfigSelection(legacy);
            Check(legacy.ReadString("session.config", "topic_id") == "keybinds", "topic identity persists independently of its ordinal");
            Check(runtime->active_config_section_ == 2 && !module.DialogOpen(), "new Keybinds topic selections round trip without reopening transient workspaces");
        }
        static auto Run(void) -> void { ControllerVisuals(); Ownership(); DirectInteraction(); LibraryPlacement(); BarControlsAndCrossbarGeometry(); EmptyBarEditBounds(); Editing(); KeyboardAndCrossbar(); Persistence(); SlotStylePersistence(); SlotStyleCompatibility(); SlotFrameProfileOverlay(); SlotTextDefaults(); SlotStyleActions(); SlotStyleGeometry(); Keybinds(); ControllerSetup(); RuntimeOwnership(); NativeMacroKeyboardOwnership(); EditModeManagement(); DependentSettings(); WorkspaceCursors(); FloatingWorkspaces(); LibraryCategories(); LibraryLayoutAndSearch(); LibraryGridNavigation(); Configuration(); }
    };
}

auto main(const int argc, char** argv) -> int
{
    if (argc == 2 && std::strcmp(argv[1], "--keybinds-only") == 0)
    {
        try { xiui::actionbars_ui_test::Keybinds(); std::puts("Hotbar and crossbar keybind list checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Keybinds check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--controller-setup-only") == 0)
    {
        try { xiui::actionbars_ui_test::ControllerSetup(); xiui::actionbars_ui_test::Configuration(); std::puts("Controller setup and configuration checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Controller setup check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--workspace-cursors-only") == 0)
    {
        try { xiui::actionbars_ui_test::WorkspaceCursors(); std::puts("Popout cursor checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Popout cursor check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--dependent-settings-only") == 0)
    {
        try { xiui::actionbars_ui_test::DependentSettings(); std::puts("Dependent setting visibility checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Dependent setting visibility check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--configuration-only") == 0)
    {
        try { xiui::actionbars_ui_test::Configuration(); std::puts("Action bar configuration checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Action bar configuration check failed: %s\n", error.what()); return 1; }
    }
    if (argc == 2 && std::strcmp(argv[1], "--controller-visuals-only") == 0)
    {
        try { xiui::actionbars_ui_test::ControllerVisuals(); std::puts("Crossbar controller artwork and animation checks passed."); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "Crossbar visual check failed: %s\n", error.what()); return 1; }
    }
    try { xiui::actionbars_ui_test::Run(); std::puts("Action bars UI checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Action bars UI check failed: %s\n", error.what()); return 1; }
}
