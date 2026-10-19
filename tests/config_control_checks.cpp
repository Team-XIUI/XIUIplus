#include "config_input_fixture.hpp"

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::StartupWiring(void) -> bool
    {
        const auto root = std::filesystem::temp_directory_path() / ("xiui-startup-wiring-" + std::to_string(::GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } files{root};
        auto runtime = std::make_unique<hud_runtime>();
        runtime->Initialize(nullptr, root.string().c_str());
        if (!check(runtime->player_.timing_ == &runtime->timing_source_ && runtime->player_.sounds_ == &runtime->ui_sounds_ &&
            runtime->target_.Settings().world_bar.healing == &runtime->heal_prediction_source_ && runtime->target_.timing_ == &runtime->timing_source_ && runtime->enemy_list_.timing_ == &runtime->timing_source_,
            "real startup retains timing, healing and sound dependencies")) return false;
        for (auto* module : runtime->modules_.All())
        {
            const auto original = module->ConfigView();
            for (size_t view = 0; view < std::max(size_t{1}, module->ConfigViews().size()); ++view)
            {
                module->SelectConfigView(view);
                std::set<std::string> ids;
                for (const auto& topic : module->ConfigSections())
                    if (!check(!topic.Id().empty() && ids.insert(std::string{topic.Id()}).second, "every settings view has unique stable topic identities")) return false;
            }
            module->SelectConfigView(original);
        }
        ui::design_system design;
        const auto style = ui::theme::CreateDefault().body_text;
        const std::string letter = "\xC3\xA9";
        const auto fitted = ui::FitText(design, letter + letter + letter + letter, design.MeasureText((letter + "...").c_str(), style), style);
        if (!check(fitted == letter + "...", "shared text fitting keeps whole UTF-8 characters")) return false;
        runtime->FlushSettings();
        runtime->Shutdown();
        return true;
    }

    auto config_input_test::ControlCount(hud_runtime& runtime) -> size_t
    {
        const auto slots = runtime.GroupOptionSlots();
        return static_cast<size_t>(std::count_if(slots.begin(), slots.end(), [&](const size_t index) { return index < runtime.config_.module->OptionCount(); }));
    }

    auto __cdecl config_input_test::HoverSound(void) -> void { ++hover_sounds; }

    auto __cdecl config_input_test::SelectSound(void) -> void { ++select_sounds; }

    auto __cdecl config_input_test::MenuSound(void) -> void { ++menu_sounds; }

    auto config_input_test::Sounds(void) -> bool
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
        hud_runtime runtime;
        runtime.mouse_ = &input;
        auto& sounds = runtime.ui_sounds_;
        sounds.sounds_[0].function = reinterpret_cast<uintptr_t>(&HoverSound);
        sounds.sounds_[1].function = reinterpret_cast<uintptr_t>(&SelectSound);
        sounds.sounds_[2].function = reinterpret_cast<uintptr_t>(&MenuSound);
        hover_sounds = select_sounds = menu_sounds = 0;
        if (!check(runtime.SetEditing(true) && menu_sounds == 1, "opening config plays native menu feedback")) return false;
        runtime.SetEditing(true);
        if (!check(menu_sounds == 1, "already-open config does not replay its opening sound")) return false;
        runtime.SetEditing(false);
        runtime.RestoreEditing(true);
        if (!check(menu_sounds == 1, "hot-reload edit restoration stays silent")) return false;

        sounds.MenuOpened(1000, false);
        sounds.Hover(1, 1001); sounds.Hover(2, 1050); sounds.Hover(2, 1200);
        if (!check(hover_sounds == 0, "opening and stationary hover do not add incidental sounds")) return false;
        sounds.Hover(3, 1201); sounds.Hover(3, 1300);
        sounds.Hover(4, 1400, false); sounds.Hover(4, 1500);
        if (!check(hover_sounds == 1, "entering a control plays once and dragging stays quiet")) return false;
        sounds.Hover(0, 1600); sounds.Hover(4, 1700);
        sounds.Select(1800); sounds.Hover(5, 1850); sounds.Hover(6, 2000);
        if (!check(hover_sounds == 3 && select_sounds == 1, "re-entry plays cursor feedback and selection suppresses incidental hover")) return false;
        sounds.sounds_[0].cache_slot = 1;
        if (!check(!sounds.Play(ui::native_ui_sound::hover) && hover_sounds == 3, "unreadable native resource state fails silently")) return false;
        sounds.sounds_[0].cache_slot = 0;

        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        runtime.config_.bounds = {40, 40, 1100, 600};
        const auto at = [&](const ui::rect& bounds) { runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3); };
        const auto press = [&] { runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport); };
        const auto release = [&] { runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport); };
        at(runtime.ConfigLayout().ModuleTab(0));
        if (!check(runtime.ConfigSoundTarget(viewport) != 0, "config navigation has a hover identity")) return false;
        const auto before = select_sounds;
        press(); press(); release();
        if (!check(select_sounds == before + 1, "duplicate pointer messages play only one selection")) return false;
        runtime.mouse_x_ = 1100; runtime.mouse_y_ = 630;
        if (!check(runtime.ConfigSoundTarget(viewport) == 0, "blank configuration space is silent")) return false;
        runtime.config_.module = &runtime.player_;
        runtime.config_.section = static_cast<size_t>(bar_config_section::bar_style);
        runtime.NormalizeConfigSelection();
        const auto row = runtime.ConfigLayout().OptionRow(3);
        at(ui::config_layout::SliderTrack(row));
        const auto sliderBefore = select_sounds;
        press(); runtime.mouse_x_ += 50; runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport); release();
        if (!check(select_sounds == sliderBefore + 1, "a complete slider drag plays only its initial click")) return false;
        runtime.config_.active_color = &runtime.player_.Settings().hp_color;
        at(ui::color_picker_layout{runtime.ConfigLayout().ColorEditor(viewport)}.Close());
        if (!check(runtime.ConfigSoundTarget(viewport) != 0, "color-picker controls share sound hit testing")) return false;
        runtime.config_.active_color = nullptr;

        runtime.equipment_.State().bounds = {1200, 300, 206, 226};
        at(runtime.equipment_.SlotBounds(0));
        const auto firstSlot = runtime.HudSoundTarget();
        at(runtime.equipment_.SlotBounds(1));
        if (!check(firstSlot != 0 && runtime.HudSoundTarget() != 0 && firstSlot != runtime.HudSoundTarget(), "equipment squares have separate hover identities")) return false;
        runtime.equipment_.State().enabled = false;
        if (!check(runtime.HudSoundTarget() == 0, "hidden equipment produces no hover feedback")) return false;

        runtime.SetEditing(false);
        auto& ready = runtime.ready_check_;
        const auto now = ::GetTickCount64();
        const std::array<ready_check_member, 2> roster{{{1, 0, 1, "One"}, {2, 1, 2, "Two"}}};
        if (!check(ready.check_.Start(roster, 1, 1, now), "ready-check mock starts without sending messages")) return false;
        ready.State().bounds = {0, 0, 300, 180}; ready.buttons_[0] = {20, 20, 100, 30};
        const auto readyBefore = select_sounds;
        ready.HandleMouse(WM_LBUTTONDOWN, 30, 30, now);
        ready.HandleMouse(WM_LBUTTONDOWN, 30, 30, now);
        ready.HandleMouse(WM_MOUSEMOVE, 40, 30, now);
        ready.HandleMouse(WM_LBUTTONUP, 400, 400, now);
        if (!check(select_sounds == readyBefore + 1 && ready.ButtonAt(30, 30) == 1 && ready.ButtonAt(10, 10) == 0,
            "Ready Check buttons play once per press, with no sound for window padding")) return false;
        runtime.mouse_ = nullptr;
        return true;
    }

    auto config_input_test::SliderValues(void) -> bool
    {
        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-slider-input-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        hud_runtime runtime;
        runtime.settings_path_ = path.string();
        runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.module = &runtime.player_;
        runtime.config_.section = static_cast<size_t>(bar_config_section::layout);
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto slots = runtime.GroupOptionSlots();
        const auto heightSlot = std::find_if(slots.begin(), slots.end(), [&](const auto index) {
            return index < runtime.player_.OptionCount() && runtime.player_.Option(index).slider == &runtime.player_.Settings().hp_style.height;
        });
        if (!check(heightSlot != slots.end(), "the player height control is available on Layout")) return false;
        const auto row = runtime.ConfigLayout().OptionRow(static_cast<int>(heightSlot - slots.begin()));
        const auto field = ui::config_layout::SliderValue(row);
        const auto click = [&](const float x, const float y) {
            runtime.mouse_x_ = static_cast<int>(x); runtime.mouse_y_ = static_cast<int>(y);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        };
        const auto open = [&] { click(field.x + field.width / 2, field.y + field.height / 2); };
        const auto height = runtime.player_.Settings().hp_style.height;
        open();
        if (!check(runtime.config_.slider_input.Focused() && runtime.KeyboardFocused() && !runtime.ChatFocused() &&
            runtime.player_.Settings().hp_style.height == height && runtime.interaction_ == hud_runtime::interaction::none,
            "clicking a slider value captures typing without dragging, changing its value or focusing chat")) return false;
        runtime.config_.slider_input.Insert("23");
        runtime.HandleConfigInputKey(VK_RETURN, 0x40000000);
        if (!check(runtime.config_.slider_input.Focused() && runtime.player_.Settings().hp_style.height == height,
            "a held Enter cannot commit a newly opened value")) return false;
        runtime.HandleConfigInputKey(VK_RETURN, 0);
        if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 23 &&
            runtime.player_.Settings().mp_style.height == 23 && runtime.player_.Settings().tp_style.height == 23,
            "typed values apply through the module callback and release keyboard capture")) return false;
        runtime.FlushSettings();
        player_module loaded;
        loaded.Load(settings_store{path.string()});
        if (!check(loaded.Settings().hp_style.height == 23, "typed slider values persist through the shared settings path")) return false;
        open(); runtime.config_.slider_input.Insert("-");
        runtime.HandleConfigInputKey(VK_RETURN, 0);
        if (!check(runtime.config_.slider_input.Focused() && runtime.config_.slider_input.Invalid() && runtime.player_.Settings().hp_style.height == 23,
            "incomplete numbers stay editable without modifying the saved value")) return false;
        runtime.HandleConfigInputKey(VK_ESCAPE, 0);
        if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 23, "Escape cancels numeric editing")) return false;
        open(); runtime.config_.slider_input.Insert("999"); runtime.HandleConfigInputKey(VK_RETURN, 0);
        if (!check(runtime.player_.Settings().hp_style.height == 80, "typed values respect the slider's maximum")) return false;
        open(); runtime.config_.slider_input.Insert("31"); click(1200, 5);
        if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 31, "clicking outside applies a valid pending value and releases typing")) return false;
        open(); runtime.config_.slider_input.Insert("42"); runtime.config_.module = &runtime.target_;
        runtime.NormalizeConfigSelection();
        if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 31, "changing config pages cancels stale numeric input")) return false;
        runtime.config_.module = &runtime.player_;
        const auto track = ui::config_layout::SliderTrack(row);
        runtime.mouse_x_ = static_cast<int>(track.x); runtime.mouse_y_ = static_cast<int>(track.y);
        runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
        runtime.mouse_x_ = static_cast<int>(std::ceil(track.x + track.width));
        runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
        if (!check(runtime.player_.Settings().hp_style.height == 80 && !runtime.config_.slider_input.Focused(), "slider dragging still uses its final release position")) return false;

        float value = 0.4f;
        ui::slider_value_input input;
        input.Open(module_option::Slider("Value", "", &value, 0.1f, 1, 0.05f));
        input.Insert("0.83");
        if (!check(input.Value() && std::abs(*input.Value() - 0.85f) < 0.00001f && value == 0.4f,
            "fractional entry snaps to the same step grid as dragging without applying while typing")) return false;
        input.Open(module_option::Slider("Offset", "", &value, -200, 200, 1));
        input.Insert("-45");
        if (!check(input.Value() && *input.Value() == -45, "negative offsets support exact entry")) return false;
        input.SelectAll(); input.Insert("1..2");
        if (!check(!input.Value() && input.Invalid(), "malformed numbers cannot reach module settings")) return false;
        input.SelectAll(); input.Insert("1e999");
        if (!check(!input.Value(), "overflowing numeric input is rejected")) return false;
        if (!check(ui::slider_value_input::Format(0.85f, 0.05f) == "0.85" && ui::slider_value_input::Format(31, 1) == "31",
            "value fields retain the precision needed for each slider step")) return false;
        for (const float width : {269.0f, 351.0f, 439.0f})
        {
            const ui::rect bounds{0, 0, width, 56};
            const auto slider = ui::config_layout::SliderTrack(bounds), number = ui::config_layout::SliderValue(bounds);
            if (!check(slider.width > 0 && slider.x + slider.width < number.x && number.x + number.width <= bounds.width &&
                number.y + number.height < ui::config_layout::Description(bounds).y,
                "slider fields and tracks fit narrow columns without covering descriptions")) return false;
        }
        return true;
    }

    auto config_input_test::ColorPicker(void) -> bool
    {
        using picker = ui::color_picker;
        for (uint32_t i = 0; i < 4096; ++i)
        {
            const D3DCOLOR color = i * 2654435761u;
            if (!check(picker::ToColor(picker::FromColor(color)) == color, "HSV round-trips preserve all RGB and alpha bytes")) return false;
        }
        const ui::rect panel{30, 40, 300, 404};
        const ui::color_picker_layout layout{panel};
        const auto wheel = layout.Wheel(), value = layout.Value(), alpha = layout.Alpha();
        const float cx = wheel.x + wheel.width * 0.5f, cy = wheel.y + wheel.height * 0.5f;
        picker model;
        model.Open(0x40000000);
        model.BeginDrag(picker::part::wheel); model.Update(panel, cx, wheel.y);
        const auto black = model.Value();
        model.BeginDrag(picker::part::value); model.Update(panel, value.x + 4, value.y - 20);
        if (!check(black.value == 0 && black.saturation == 1 && model.Value().hue == black.hue &&
            (model.Color() & 0xFFFFFF) != 0 && (model.Color() >> 24) == 0x40, "hue and saturation chosen at black survive brightness changes without losing alpha")) return false;
        model.BeginDrag(picker::part::wheel); model.Update(panel, cx, cy);
        if (!check(model.Value().saturation == 0 && (model.Color() & 0xFFFFFF) == 0xFFFFFF,
            "the center of the wheel selects white at full brightness")) return false;
        model.BeginDrag(picker::part::alpha); model.Update(panel, alpha.x - 30, alpha.y + 4);
        if (!check(model.Color() == 0x00FFFFFF && model.Hex() == "#FFFFFF00", "opacity clamps at zero while preserving RGB and uses RGBA hex order")) return false;
        model.Update(panel, alpha.x + alpha.width + 30, alpha.y + 4);
        if (!check(model.Color() == 0xFFFFFFFF && model.Hex() == "#FFFFFF", "opaque colors copy ordinary RGB hex")) return false;
        model.Restore(); model.EndDrag();
        if (!check(model.Color() == 0x40000000 && model.Dragging() == picker::part::none &&
            picker::HitTest(panel, wheel.x, wheel.y) != picker::part::wheel && picker::HitTest(panel, cx, cy) == picker::part::wheel,
            "restore retains the opening color and wheel corners do not claim drags")) return false;
        for (const auto viewport : {D3DVIEWPORT8{0, 0, 1280, 720, 0, 1}, D3DVIEWPORT8{0, 0, 640, 480, 0, 1}, D3DVIEWPORT8{0, 0, 320, 240, 0, 1}})
        {
            const auto bounds = ui::config_layout{{40, 40, 1100, 600}}.ColorEditor(viewport);
            const ui::color_picker_layout controls{bounds};
            if (!check(bounds.x >= 0 && bounds.y >= 0 && bounds.x + bounds.width <= viewport.Width && bounds.y + bounds.height <= viewport.Height &&
                controls.Hex().y + controls.Hex().height <= bounds.y + bounds.height,
                "the complete picker and controls remain inside small viewports")) return false;
        }

        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-wheel-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        hud_runtime runtime;
        runtime.settings_path_ = path.string(); runtime.editing_ = true;
        runtime.config_.bounds = {40, 40, 1100, 600};
        runtime.config_.module = &runtime.player_;
        runtime.player_.Settings().style_edit_mode = 1;
        runtime.player_.Settings().hp_color_top = 0x40FF0000;
        runtime.config_.active_color = &runtime.player_.Settings().hp_color_top;
        runtime.config_.color_picker.Open(*runtime.config_.active_color);
        const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
        const auto editor = runtime.ConfigLayout().ColorEditor(viewport);
        const ui::color_picker_layout editorLayout{editor};
        const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
            runtime.mouse_x_ = static_cast<int>(x); runtime.mouse_y_ = static_cast<int>(y);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto disc = editorLayout.Wheel();
        const bool enabled = runtime.player_.State().enabled;
        send(WM_LBUTTONDOWN, MK_LBUTTON, disc.x + disc.width * 0.5f, disc.y + disc.height - 1);
        send(WM_MOUSEMOVE, MK_LBUTTON, disc.x + disc.width + 40, disc.y + disc.height * 0.5f);
        send(WM_LBUTTONUP, 0, disc.x + disc.width + 40, disc.y + disc.height * 0.5f);
        if (!check(runtime.interaction_ == hud_runtime::interaction::none && runtime.config_.color_picker.Dragging() == picker::part::none &&
            !runtime.mouse_down_ && runtime.player_.State().enabled == enabled && (*runtime.config_.active_color >> 24) == 0x40,
            "wheel drags retain alpha, cannot click through to config, and release outside the disc")) return false;
        const auto opacity = editorLayout.Alpha();
        send(WM_LBUTTONDOWN, MK_LBUTTON, opacity.x + opacity.width * 0.5f, opacity.y + 4);
        send(WM_LBUTTONUP, 0, opacity.x + opacity.width * 0.5f, opacity.y + 4);
        const auto picked = *runtime.config_.active_color;
        runtime.FlushSettings();
        player_module restored;
        restored.Load(settings_store{path.string()});
        if (!check(restored.Settings().hp_color_top == picked && (picked >> 24) >= 126 && (picked >> 24) <= 128,
            "picker opacity survives the normal asynchronous save and module reload")) return false;
        const auto original = editorLayout.Original();
        send(WM_LBUTTONDOWN, MK_LBUTTON, original.x + 5, original.y + 5);
        send(WM_LBUTTONUP, 0, original.x + 5, original.y + 5);
        if (!check(*runtime.config_.active_color == 0x40FF0000, "the original swatch restores the opening color through normal config input")) return false;
        const auto close = editorLayout.Close();
        send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 4, close.y + 4);
        send(WM_LBUTTONUP, 0, close.x + 4, close.y + 4);
        if (!check(runtime.config_.active_color == nullptr && runtime.interaction_ == hud_runtime::interaction::none, "the close button dismisses the picker without leaving capture active")) return false;
        const auto legacy = settings_store::FromIniText("[meter.player.hp]\nfill_top=123456\n[meter.target.hp]\nfill_top=40556677\n[meter.experience]\nfill_top=8099AABB\n");
        restored.Load(legacy);
        target_module target; target.Load(legacy);
        experience_module experience; experience.Load(legacy);
        return check(restored.Settings().hp_color_top == 0xFF123456 && target.Settings().hp_color_top == 0x40556677 && experience.Settings().color_top == 0x8099AABB,
            "legacy RGB colors remain opaque while target and EXP retain alpha colors");
    }

    auto config_input_test::ExperiencePositionPersistence(void) -> bool
    {
        hud_runtime runtime;
        auto& exp = runtime.experience_;
        exp.State().bounds = {850, 1403, 860, 145};
        auto& settings = exp.Settings();
        settings.style.height = 6;
        settings.show_label = settings.show_current = settings.show_percent = settings.show_remaining = false;
        settings.show_points = settings.show_master_level = false;
        settings.readouts.fill(false);
        settings.readouts[static_cast<size_t>(experience_readout::job)] = true;
        settings.readout_anchor = 1; settings.readout_text.offset_y = -60;
        const auto saved = runtime.CaptureSettings();
        hud_runtime restored; restored.LoadSettings(saved);
        const D3DVIEWPORT8 viewport{0, 0, 2560, 1440, 0, 1};
        auto& loaded = restored.experience_;
        for (int pass = 0; pass < 3; ++pass) restored.EnsureLayout(viewport);
        if (!check(!loaded.PositionBoundsReady() && loaded.State().bounds.x == 850 && loaded.State().bounds.y == 1403,
            "loading a compact EXP bar near the screen bottom does not clamp against its old saved height")) return false;
        hud_snapshot snapshot{}; snapshot.exp_current = 200; snapshot.exp_needed = 1000;
        const module_render_context context{restored.design_system_, nullptr, true};
        loaded.Render(snapshot, context);
        restored.ClampLayout(viewport);
        if (!check(loaded.PositionBoundsReady() && loaded.State().bounds.height == 145 && loaded.State().bounds.x == 850 &&
            loaded.State().bounds.y == 1403 && loaded.State().content_bounds.y < 0,
            "the measured bar and above-bar readouts preserve their saved position despite an oversized layout rectangle")) return false;
        const auto roundtrip = restored.CaptureSettings();
        loaded.State().content_bounds = {0, 0, 860, 300};
        restored.LoadSettings(roundtrip); restored.EnsureLayout(viewport);
        if (!check(!loaded.PositionBoundsReady() && loaded.State().bounds.y == 1403 &&
            roundtrip.ReadRect("layout.exp", {}).y == 1403,
            "profile round trips clear stale EXP measurements before in-place reloads")) return false;
        loaded.Render(snapshot, context);
        const D3DVIEWPORT8 smaller{0, 0, 2560, 1080, 0, 1};
        restored.ClampLayout(smaller);
        const auto visual = restored.MovementBounds(loaded);
        return check(visual.y >= 0 && visual.y + visual.height <= smaller.Height && loaded.State().bounds.y < 1403,
            "measured EXP content still clamps into view after a real viewport reduction");
    }

    auto config_input_test::DragPersistence(void) -> bool
    {
        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-drag-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        const settings_store store{path.string()};
        store.WriteBool("player", "show_identity", false);
        store.Flush();
        hud_runtime runtime{};
        runtime.settings_path_ = path.string();
        runtime.editing_ = true;
        runtime.player_.Settings().show_identity = false;
        *runtime.general_.Option(runtime.general_.GridStart() + 1).toggle = false;
        *runtime.general_.Option(runtime.general_.GridStart() + 2).toggle = false;
        runtime.config_.bounds = {40, 40, 680, 430};
        const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
        const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
            runtime.mouse_x_ = static_cast<int>(x);
            runtime.mouse_y_ = static_cast<int>(y);
            return runtime.ProcessPointerMessage(message, buttons, viewport);
        };
        const auto idle = [&] {
            return runtime.interaction_ == hud_runtime::interaction::none && runtime.interaction_module_ == nullptr &&
                runtime.config_.active_slider == nullptr && !runtime.mouse_down_ && runtime.mouse_buttons_ == 0;
        };
        const auto moduleOrder = runtime.modules_.All();
        for (auto* module : moduleOrder)
        {
            if (!module->HasEditableBounds() || !module->PreviewVisible()) continue;
            const auto original = module->State().bounds;
            module->State().bounds.x = 900;
            module->State().bounds.y = 650;
            auto& layers = runtime.modules_.All();
            const auto layer = std::find(layers.begin(), layers.end(), module);
            std::rotate(layer, layer + 1, layers.end());
            const auto area = module->State().EditBounds();
            const auto x = area.x + std::min(20.0f, area.width / 2), y = area.y + std::min(20.0f, area.height / 2);
            send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
            if (!check(runtime.interaction_module_ == module, (std::string{"module presses begin their own drag: "} + module->Id() + "; actual=" + (runtime.interaction_module_ ? runtime.interaction_module_->Id() : "none")).c_str())) return false;
            send(WM_LBUTTONUP, 0, x + 50, y + 30);
            if (!check(runtime.settings_writer_.Flush(), "module layout writes finish successfully")) return false;
            const std::string section = module == &runtime.experience_ ? "layout.exp" : std::string{"layout."} + module->Id();
            const auto saved = store.ReadRect(section.c_str(), {});
            if (!check(idle() && saved.x == module->State().bounds.x && saved.y == module->State().bounds.y &&
                (saved.x != 900 || saved.y != 650) && saved.width == module->State().bounds.width && saved.height == module->State().bounds.height,
                (std::string{"dropping each module saves its final layout: "} + module->Id()).c_str())) return false;
            module->State().bounds = original;
        }
        runtime.modules_.All() = moduleOrder;
        send(WM_LBUTTONDOWN, MK_LBUTTON, 100, 50);
        send(WM_LBUTTONUP, 0, 120, 65);
        if (!check(runtime.settings_writer_.Flush(), "config layout writes finish successfully")) return false;
        const auto saved = store.ReadRect("layout.config", {});
        if (!check(idle() && saved.x == 60 && saved.y == 55, "config dragging saves only its final layout")) return false;
        runtime.editing_ = false;
        return check(!store.ReadBool("player", "show_identity", true), "layout gestures preserve the current module settings");
    }

    auto config_input_test::Run(void) -> bool
    {
        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-input-save-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        hud_runtime runtime;
        runtime.settings_path_ = path.string();
        runtime.editing_ = true;
        runtime.config_.module = &runtime.general_;
        runtime.config_.section = general_module::grid_section;
        runtime.config_.bounds = {40, 40, 1100, 600};
        const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
        const auto row = runtime.ConfigLayout().OptionRow(0);
        double maximumMessageMs = 0;
        const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
            runtime.mouse_x_ = static_cast<int>(x); runtime.mouse_y_ = static_cast<int>(y);
            const auto start = std::chrono::steady_clock::now();
            const bool captured = runtime.ProcessPointerMessage(message, buttons, viewport);
            maximumMessageMs = std::max(maximumMessageMs,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
            return captured;
        };
        const float x = row.x + row.width - 20, y = row.y + 20;
        auto& visible = runtime.general_.Grid().visible;
        const bool original = visible, snap = runtime.general_.Grid().snap_grid;
        if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, x, y) && visible != original,
            "a press applies immediately at its message position")) return false;
        send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
        runtime.UpdateMouseCapture(viewport, MK_LBUTTON);
        if (!check(visible != original, "duplicate messages and held-frame updates cannot repeat a press")) return false;
        const auto other = runtime.ConfigLayout().OptionRow(1);
        send(WM_MOUSEMOVE, MK_LBUTTON, other.x + 20, other.y + 20);
        send(WM_LBUTTONUP, 0, other.x + 20, other.y + 20);
        if (!check(visible != original && runtime.general_.Grid().snap_grid == snap,
            "moving during a gesture cannot retarget the press")) return false;
        send(WM_LBUTTONDBLCLK, MK_LBUTTON, x, y);
        send(WM_LBUTTONUP, 0, x, y);
        if (!check(visible == original, "the second press of a double-click changes the value once")) return false;
        for (int i = 0; i < 2; ++i)
        {
            send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
            send(WM_LBUTTONUP, 0, x, y);
        }
        if (!check(visible == original, "two complete clicks between frames are both retained")) return false;
        if (!check(!send(WM_LBUTTONDOWN, MK_LBUTTON, 1500, 1), "a game-owned press passes through")) return false;
        send(WM_MOUSEMOVE, MK_LBUTTON, x, y);
        if (!check(!send(WM_LBUTTONUP, 0, x, y) && visible == original,
            "dragging into configuration cannot activate a toggle")) return false;
        runtime.FlushSettings();
        const settings_store saved{path.string()};
        if (!check(saved.ReadBool("layout.grid", "visible", !visible) == visible &&
            saved.ReadBool("layout.grid", "snap_grid", !snap) == snap,
            "pointer edits persist together through the shared settings writer")) return false;
        *runtime.general_.Option(runtime.general_.GridStart()).toggle = !original;
        runtime.ExitEdit();
        if (!check(runtime.settings_writer_.Flush() && saved.ReadBool("layout.grid", "visible", original) != original,
            "leaving edit mode queues final preference changes")) return false;
        std::printf("Maximum input callback time with persistence enabled: %.2f ms.\n", maximumMessageMs);
        return true;
    }
}
