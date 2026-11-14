#include "support/config_input_fixture.hpp"
#include "config/basic_settings_layout.hpp"
#include "settings/text_style_settings.hpp"
#include "resources/embedded_resource.hpp"

#include <memory>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui
{
    using namespace config_checks;

    auto config_input_test::HudScaling(const char* defaultsPath) -> bool
    {
        std::ifstream file{defaultsPath, std::ios::binary};
        const std::string defaults{std::istreambuf_iterator<char>{file}, {}};
        if (!check(!defaults.empty(), "HUD scale fixture has shipped defaults")) return false;
        auto runtime = std::make_unique<hud_runtime>();
        auto& hud = *runtime;
        hud.default_settings_text_ = defaults;
        hud.LoadSettings(settings_store::FromIniText(defaults));
        hud.editing_ = true;
        const D3DVIEWPORT8 viewport{0, 0, 2560, 1440, 0, 1};
        const auto closeTo = [](float a, float b) { return std::abs(a - b) < .01f; };
        auto& player = hud.player_.State();
        const auto position = player.bounds;
        hud.ResolveHudTransforms(viewport);
        const auto identity = hud.MovementBounds(hud.player_);
        const auto global = hud.general_.Option(hud.general_.HudScaleStart());
        *global.slider = 1.5f;
        hud.ResolveHudTransforms(viewport);
        const auto uniform = hud.MovementBounds(hud.player_);
        const auto scaleOption = hud.player_.ContentOptionCount();
        if (!check(player.scale.locked && closeTo(uniform.width, identity.width * 1.5f) && closeTo(uniform.height, identity.height * 1.5f) &&
            std::strcmp(hud.player_.Option(scaleOption).label, "Scale (%)") == 0 && !hud.player_.Option(scaleOption + 1).visible,
            "global and local scaling default to one uniform scale control")) return false;
        hud.BeginScale(hud.player_);
        hud.UpdateScale(hud.scale_gesture_.pointer.x + uniform.width * .2f, hud.scale_gesture_.pointer.y - uniform.height * .2f);
        const auto growing = hud.MovementBounds(hud.player_);
        if (!check(closeTo(player.scale.x, 1.2f) && closeTo(player.scale.y, 1.2f) && growing.y < uniform.y &&
            closeTo(growing.x, uniform.x) && closeTo(growing.y + growing.height, uniform.y + uniform.height),
            "top-right scaling grows upward and right while the opposite corner stays fixed")) return false;
        hud.FinishScale(false);
        hud.BeginScale(hud.player_, true);
        hud.HandleScaleAction(controller_action::right, false);
        if (!check(closeTo(player.scale.x, 1.05f) && closeTo(player.scale.y, 1.05f), "default directional resizing scales uniformly")) return false;
        hud.FinishScale(false);
        player.scale = player.scale_checkpoint = {1.2f, .8f, false};
        hud.ResolveHudTransforms(viewport);
        const auto scaled = hud.MovementBounds(hud.player_);
        if (!check(closeTo(scaled.width, identity.width * 1.8f) && closeTo(scaled.height, identity.height * 1.2f), "global and independent axes compose once")) return false;
        const auto mapped = player.transform.Map(position);
        const auto inverse = player.transform.Inverse(mapped);
        if (!check(closeTo(inverse.x, position.x) && closeTo(inverse.y, position.y) && closeTo(inverse.width, position.width), "HUD coordinates round trip")) return false;
        {
            const auto outer = hud.design_system_.Transform(player.transform);
            const auto localViewport = player.transform.Inverse(ui::rect{0, 0, 2560, 1440});
            module_render_context context{hud.design_system_, nullptr, true, nullptr, {0, 0, 2560, 1440}};
            if (!check(closeTo(context.Viewport().width, localViewport.width), "render viewport is in the element coordinate space")) return false;
            { const auto inner = hud.design_system_.Transform({}); if (!check(hud.design_system_.Transform().Identity(), "overlay suspends HUD scaling")) return false; }
            if (!check(closeTo(hud.design_system_.Transform().x, 1.8f), "nested transform restores its owner")) return false;
        }
        if (!check(hud.design_system_.Transform().Identity(), "render transform never leaks into configuration")) return false;

        hud.mouse_x_ = static_cast<int>(scaled.x + scaled.width);
        hud.mouse_y_ = static_cast<int>(scaled.y + scaled.height);
        hud.BeginScale(hud.player_);
        const auto gesture = hud.scale_gesture_;
        hud.UpdateScale(gesture.pointer.x + gesture.visual.width * .2f, gesture.pointer.y - gesture.visual.height * .4f);
        const auto stretchedBounds = hud.MovementBounds(hud.player_);
        if (!check(closeTo(stretchedBounds.x, gesture.visual.x) && closeTo(stretchedBounds.y + stretchedBounds.height, gesture.visual.y + gesture.visual.height),
            "unlocked axes use the same stationary bottom-left corner")) return false;
        const auto pending = hud.CaptureSettings();
        const auto fixedGrip = hud.ScaleGrip(hud.player_);
        const auto oldContent = player.content_bounds;
        player.content_bounds.x += 30; player.content_bounds.y += 20;
        const auto animatedGrip = hud.ScaleGrip(hud.player_);
        player.content_bounds = oldContent;
        if (!check(closeTo(fixedGrip.x, animatedGrip.x) && closeTo(fixedGrip.y, animatedGrip.y), "live content cannot move a pending scale grip")) return false;
        if (!check(closeTo(player.scale.x, 1.44f) && closeTo(player.scale.y, 1.12f) &&
            closeTo(pending.ReadFloat("hud.scale.player", "x", 0), 1.2f) &&
            closeTo(pending.ReadRect("layout.player", {}).y, gesture.original_bounds.y), "scale and anchor previews are excluded from an unrelated save")) return false;
        hud.HandleScaleAction(controller_action::cancel, false);
        if (!check(closeTo(player.scale.x, 1.2f) && closeTo(player.scale.y, .8f), "Cancel restores both axes")) return false;
        const auto grip = hud.ScaleGrip(hud.player_);
        hud.config_.bounds = {1400, 20, 1100, 600};
        hud.mouse_x_ = static_cast<int>(grip.x + 6); hud.mouse_y_ = static_cast<int>(grip.y + 6); hud.mouse_down_ = true;
        hud.BeginPanelInteraction(viewport);
        if (!check(hud.interaction_ == hud_runtime::interaction::scale_module, "the corner grip wins over the move surface")) return false;
        hud.FinishInteraction();
        if (!check(hud.scale_gesture_.module == nullptr && closeTo(player.scale.x, 1.2f), "interrupted pointer scaling cancels instead of committing")) return false;
        hud.mouse_down_ = false;
        hud.BeginScale(hud.player_, true);
        hud.HandleScaleAction(controller_action::right, false);
        hud.HandleScaleAction(controller_action::confirm, false);
        if (!check(closeTo(player.scale.x, 1.25f), "controller scale commits through the shared gesture")) return false;
        hud.player_.BindConfigScale(&player, true);
        player.scale.locked = true; player.scale_checkpoint = player.scale;
        const float ratio = player.scale.x / player.scale.y;
        const auto first = hud.player_.ContentOptionCount();
        *hud.player_.Option(first).slider += .1f;
        hud.player_.OnOptionChanged(first);
        if (!check(closeTo(player.scale.x / player.scale.y, ratio), "numeric adjustments preserve the locked ratio")) return false;

        const auto committedPosition = player.bounds;
        *global.slider = 1;
        hud.ClampLayout(viewport);
        if (!check(player.bounds.x == committedPosition.x && player.bounds.y == committedPosition.y && player.bounds.width == position.width && player.bounds.height == position.height,
            "clamping retains the committed corner placement and authored dimensions")) return false;
        hud.satchel_.State().enabled = true; hud.satchel_.Open(true);
        hud.satchel_.State().bounds = {400, 100, 500, 400}; hud.satchel_.State().scale = {1.5f, 1.25f};
        hud.ResolveHudTransforms(viewport);
        if (!check(hud.satchel_.PointerOwns(1050, 300) && !hud.satchel_.PointerOwns(1200, 300), "Satchel capture follows scaled visual bounds")) return false;
        hud.chat_.State().enabled = true;
        hud.chat_.State().bounds = {100, 600, 600, 240}; hud.chat_.State().scale = {1.5f, 1.5f};
        hud.ResolveHudTransforms(viewport);
        const auto chatCenter = hud.chat_.State().transform.Map(ui::rect{200, 650, 1, 1});
        if (!check(hud.chat_.HitTest(hud.design_system_, chatCenter.x, chatCenter.y), "Chat uses the inverse element transform for hit testing")) return false;
        for (auto* chat : {&hud.chat_, &hud.chat_second_})
        {
            chat->State().enabled = true; chat->State().bounds = {100, 600, 600, 240}; chat->State().scale = {1.5f, 1.5f};
            chat->SetLayoutEditing(true);
            hud.ResolveHudTransforms(viewport);
            const auto original = chat->State().bounds;
            std::vector<float> sizes;
            chat->VisitTextSettings([&](auto& style) { sizes.push_back(style.scale); });
            hud.BeginScale(*chat);
            const auto resize = hud.scale_gesture_;
            if (!check(resize.resizing_bounds, "chat edit grips resize the viewport")) return false;
            const auto nativeGrip = chat->EditGripBounds(hud.design_system_);
            const auto editGrip = hud.ScaleGrip(*chat);
            if (!check(nativeGrip && closeTo(editGrip.y, nativeGrip->y) && closeTo(editGrip.x, nativeGrip->x),
                "chat reuses its existing top-right grip")) return false;
            hud.UpdateScale(resize.pointer.x, resize.pointer.y + 60);
            const auto snapshot = hud.CaptureSettings();
            const auto section = std::string{"layout."} + chat->Id();
            if (!check(closeTo(chat->State().bounds.width, original.width) && closeTo(chat->State().bounds.height, 200) &&
                closeTo(chat->State().bounds.y + chat->State().bounds.height, original.y + original.height) &&
                closeTo(chat->State().scale.x, 1.5f) && closeTo(chat->State().scale.y, 1.5f) &&
                closeTo(snapshot.ReadRect(section.c_str(), {}).height, original.height),
                "vertical resizing changes only chat height and excludes the preview from autosave")) return false;
            hud.FinishScale(false);
            if (!check(closeTo(chat->State().bounds.height, original.height), "cancel restores chat viewport size")) return false;
            hud.BeginScale(*chat, true);
            hud.HandleScaleAction(controller_action::up, false);
            hud.HandleScaleAction(controller_action::confirm, false);
            size_t textIndex{}; bool sameText = true;
            chat->VisitTextSettings([&](auto& style) { sameText &= sizes[textIndex++] == style.scale; });
            const auto committed = hud.CaptureSettings();
            if (!check(chat->State().bounds.height > original.height && closeTo(chat->State().bounds.width, original.width) && sameText &&
                committed.ReadBool(chat->Id(), "height_limits_lines", false),
                "controller chat resize retains font sizes and commits its manual line capacity")) return false;
            const auto firstScale = chat->ContentOptionCount();
            if (!check(!chat->Option(firstScale + 1).visible && !chat->Option(firstScale + 2).visible,
                "chat exposes uniform scaling without text-stretch controls")) return false;
        }
        hud.actionbars_.Initialize(nullptr, &hud.action_store_, &hud.action_executor_);
        hud.actionbars_.State().enabled = true; hud.actionbars_.State().bounds = {200, 200, 600, 80};
        hud.actionbars_.State().scale = {1.5f, 1.25f};
        hud.ResolveHudTransforms(viewport);
        bool foundBar{};
        for (float y = 204; y < 240 && !foundBar; y += 4)
            for (float x = 204; x < 240 && !foundBar; x += 4)
                foundBar = hud.actionbars_.PointerOwns(hud.actionbars_.State().transform.X(x), hud.actionbars_.State().transform.Y(y));
        if (!check(foundBar, "Action Bars resolve hits through each independent bar transform")) return false;
        hud.config_.module = &hud.actionbars_;
        const auto views = hud.actionbars_.ConfigViews();
        const auto globalView = std::find_if(views.begin(), views.end(), [](const char* name) { return std::strcmp(name, "Global") == 0; });
        if (!check(globalView != views.end(), "Action Bars Global view is available")) return false;
        hud.actionbars_.SelectConfigView(static_cast<size_t>(globalView - views.begin()));
        if (!check(!hud.MatchesConfigOption(hud.actionbars_.ContentOptionCount()) && !hud.actionbars_.Option(hud.actionbars_.ContentOptionCount()).available,
            "nonvisual Action Bars views cannot edit a fallback bar scale")) return false;
        hud.actionbars_.SelectConfigView(0);

        {
            hud_runtime audit;
            audit.LoadSettings(settings_store::FromIniText(defaults)); audit.editing_ = true; audit.config_.edit_mode = true;
            size_t checked{}; bool magicBurst{};
            for (auto* module : audit.modules_.All())
            {
                if (!module->SupportsScale() || !module->HasEditableBounds() || !module->PreviewVisible() || module->EditGrip() != edit_grip_kind::scale) continue;
                const auto saved = module->State();
                auto& state = module->State();
                state.bounds = {800, 600, 200, 120}; state.content_bounds = {-12, -8, 224, 136}; state.scale = {1, 1, true};
                audit.ResolveHudTransforms(viewport);
                const auto before = audit.MovementBounds(*module), initialGrip = audit.ScaleGrip(*module);
                audit.mouse_x_ = static_cast<int>(initialGrip.x + 6); audit.mouse_y_ = static_cast<int>(initialGrip.y + 6);
                if (!check(audit.ModuleDisplayed(*module), "scale audit requires a visible edit preview")) return false;
                audit.BeginScale(*module);
                if (!check(audit.scale_gesture_.module == module, "visible scale audit starts the requested module gesture")) return false;
                audit.UpdateScale(audit.scale_gesture_.pointer.x + before.width * .2f, audit.scale_gesture_.pointer.y - before.height * .2f);
                const auto after = audit.MovementBounds(*module), movedGrip = audit.ScaleGrip(*module);
                if (!check(closeTo(after.x, before.x) && closeTo(after.y + after.height, before.y + before.height) &&
                    closeTo(movedGrip.x - initialGrip.x, before.width * .2f) && closeTo(movedGrip.y - initialGrip.y, -before.height * .2f),
                    (std::string{module->Id()} + " keeps the corner grip with the drag, including content offsets").c_str())) return false;
                audit.FinishScale(false);
                if (!check(state.bounds.x == 800 && state.bounds.y == 600 && state.scale.x == 1, "Cancel restores the complete scale placement")) return false;
                magicBurst |= module == &audit.magic_burst_; ++checked;
                state = saved;
            }
            if (!check(checked > 15 && magicBurst, "corner scaling audit includes Magic Burst and independent HUD elements")) return false;
            auto& thin = audit.player_.State(); thin.bounds = {400, 400, 600, 6}; thin.content_bounds = {}; thin.scale = {1, 1, true};
            audit.ResolveHudTransforms(viewport); audit.BeginScale(audit.player_);
            audit.UpdateScale(audit.scale_gesture_.pointer.x + 30, audit.scale_gesture_.pointer.y - 1);
            if (!check(thin.scale.x > 1.04f && thin.scale.x < 1.06f, "small vertical noise cannot make a thin bar's scale jump")) return false;
            audit.FinishScale(false);
            auto& burst = audit.magic_burst_.State(); burst.bounds = {800, 600, 160, 180}; burst.content_bounds = {}; burst.scale = {1, 1, true};
            audit.ResolveHudTransforms(viewport); audit.BeginScale(audit.magic_burst_);
            const auto before = audit.scale_gesture_.visual;
            audit.UpdateScale(audit.scale_gesture_.pointer.x + before.width * .25f, audit.scale_gesture_.pointer.y - before.height * .25f);
            const auto preview = audit.MovementBounds(audit.magic_burst_);
            audit.FinishScale(true);
            const auto committed = audit.MovementBounds(audit.magic_burst_);
            if (!check(closeTo(preview.x, committed.x) && closeTo(preview.y, committed.y) && closeTo(preview.width, committed.width) &&
                burst.bounds.y < 600 && closeTo(audit.CaptureSettings().ReadRect("layout.magicburst", {}).y, burst.bounds.y),
                "Magic Burst retains the preview position when corner scaling commits")) return false;
        }

        hud.config_.module = &hud.player_; hud.config_.section = 1; hud.config_.scroll = 2;
        const auto beforeMode = hud.CaptureSettings();
        hud.SetBasicSettings(true, false);
        if (!check(hud.config_.module == &hud.general_ && hud.BasicControllerItems(config_controller_region::options).size() == ui::basic_settings_layout::control_count &&
            hud.BasicControllerItems(config_controller_region::categories).empty(), "Basic exposes only its small shared control surface")) return false;
        hud.FocusModuleConfig(hud.satchel_);
        if (!check(hud.config_.module == &hud.general_ && hud.MovementSelectionModule() == &hud.satchel_, "Basic edit selection retains the Basic settings owner")) return false;
        hud.SetBasicSettings(false, false);
        if (!check(hud.config_.module == &hud.player_ && hud.config_.section == 1 && hud.config_.scroll == 2, "Advanced selection and scroll survive mode switches")) return false;
        const auto afterMode = hud.CaptureSettings();
        if (!check(beforeMode.ReadFloat("hud.scale.player", "x", 0) == afterMode.ReadFloat("hud.scale.player", "x", 0), "mode switches preserve scale")) return false;
        bool portableMode{};
        afterMode.VisitPreferences([&](std::string_view section, std::string_view, std::string_view) { portableMode |= section.starts_with("session."); });
        if (!check(!portableMode, "configuration mode is excluded from portable profiles")) return false;

        hud.SetBasicSettings(true, false);
        hud.party_.State().enabled = false;
        if (!check(hud.ActivateConfigAction(hud.general_, hud.general_.HudScaleStart() + 2) && hud.profile_name_.Focused() && hud.profile_keyboard_selection_ == 0,
            "Basic presets use the shared confirmation with Cancel selected")) return false;
        hud.CloseProfileControls();
        auto restored = std::make_unique<hud_runtime>();
        restored->LoadSettings(settings_store::FromIniText(hud.CaptureSettings().ToIniText()));
        if (!check(closeTo(restored->player_.State().scale.x, player.scale.x) && restored->player_.State().scale.locked, "scale and proportion lock persist")) return false;
        player_module legacy;
        legacy.LoadScale(settings_store::FromIniText("[hud.scale.player]\nx=1.2\ny=0.8\nlocked=false\n"));
        if (!check(legacy.State().scale.locked && closeTo(legacy.State().scale.x, 1.2f) && closeTo(legacy.State().scale.y, .8f),
            "earlier scale settings adopt the uniform default while retaining their dimensions")) return false;
        legacy.State().scale.locked = false;
        auto savedScale = settings_store::FromIniText(""); legacy.SaveScale(savedScale); legacy.LoadScale(savedScale);
        if (!check(!legacy.State().scale.locked, "an explicit unlock persists after the one-time default migration")) return false;
        chat_module legacyChat{true};
        const auto stretched = settings_store::FromIniText("[layout.chat2]\nx=100\ny=200\nwidth=700\nheight=200\n[hud.scale.chat2]\nx=0.955\ny=0.532\nlocked=false\nversion=2\n");
        legacyChat.Load(stretched); legacyChat.LoadScale(stretched);
        if (!check(closeTo(legacyChat.State().bounds.width, 668.5f) && closeTo(legacyChat.State().bounds.height, 106.4f) &&
            legacyChat.State().scale.x == 1 && legacyChat.State().scale.y == 1 && legacyChat.State().scale.locked,
            "legacy chat stretching becomes viewport dimensions with normal text proportions")) return false;
        auto converted = settings_store::FromIniText(""); legacyChat.Save(converted); legacyChat.SaveScale(converted);
        legacyChat.Load(converted); legacyChat.LoadScale(converted);
        if (!check(closeTo(legacyChat.State().bounds.width, 669) && closeTo(legacyChat.State().bounds.height, 106),
            "converted chat dimensions retain normal layout pixel rounding on reload")) return false;
        {
            auto oversized = settings_store::FromIniText("[meter.experience]\nheight=80\n[meter.player.hp]\nheight=28\n[meter.player.mp]\nheight=28\n[meter.player.tp]\nheight=28\n");
            auto overlaid = settings_store::FromIniText(hud.CaptureSettings().ToIniText());
            OverlayProfilePreferences(overlaid, oversized);
            experience_module xp; player_module resources;
            xp.Load(overlaid); resources.Load(overlaid);
            if (!check(xp.Settings().style.height == 12 && resources.Settings().hp_style.height == 12 &&
                resources.Settings().mp_style.height == 12 && resources.Settings().tp_style.height == 12,
                "legacy profiles migrate oversized heights without inheriting current migration markers")) return false;
            oversized.WriteInt("exp", "bar_height_version", 1);
            oversized.WriteInt("player", "bar_height_version", 1);
            xp.Load(oversized); resources.Load(oversized);
            if (!check(xp.Settings().style.height == 12 && resources.Settings().hp_style.height == 12,
                "the earlier profile-overlay marker also receives the correction")) return false;
            xp.Save(oversized); resources.Save(oversized);
            oversized.WriteFloat("meter.experience","height",80);
            for (const auto* section : {"meter.player.hp","meter.player.mp","meter.player.tp"}) oversized.WriteFloat(section,"height",28);
            OverlayProfilePreferences(overlaid, oversized);
            xp.Load(overlaid); resources.Load(overlaid);
            if (!check(xp.Settings().style.height == 80 && resources.Settings().hp_style.height == 28,
                "explicit tall settings persist after the one-time correction")) return false;
            const auto custom = settings_store::FromIniText("[meter.experience]\nheight=14\n[meter.player.hp]\nheight=7\n[meter.player.mp]\nheight=17\n[meter.player.tp]\nheight=27\n");
            xp.Load(custom); resources.Load(custom);
            if (!check(xp.Settings().style.height == 14 && resources.Settings().hp_style.height == 7 &&
                resources.Settings().mp_style.height == 17 && resources.Settings().tp_style.height == 27,
                "custom track heights remain intact during migration")) return false;
            const auto theme = ui::theme::CreateDefault();
            xp.ResetDefaults(theme); resources.ResetDefaults(theme);
            xp.State().bounds = {10,10,700,500}; resources.State().bounds = {10,10,700,500};
            module_render_context context{hud.design_system_,nullptr,true,nullptr,{0,0,2560,1440}};
            xp.Render({},context);
            if (!check(xp.Settings().style.height == 12 && resources.HPMeterBounds({}).height == 12,
                "large windows do not increase default track thickness while rendering")) return false;
        }
        auto hoverRuntime = std::make_unique<hud_runtime>();
        auto& hover = *hoverRuntime; hover.editing_ = hover.visible_ = true;
        hover.default_settings_text_ = defaults;
        for (auto* module : hover.modules_.All()) module->State().enabled = false;
        hover.player_.State().enabled = true; hover.player_.State().bounds = {400,400,300,60};
        hover.ResolveHudTransforms(viewport);
        const auto hoverGrip = hover.ScaleGrip(hover.player_);
        if (!check(closeTo(hoverGrip.y, 400) && closeTo(hoverGrip.x + hoverGrip.width, 700) && hover.EditGripAt(695,405) == &hover.player_ &&
            hover.EditGripAt(695,455) == nullptr, "shared scale grip occupies only the top-right corner")) return false;
        hover.ui_sounds_.sounds_[0].function = reinterpret_cast<uintptr_t>(&HoverSound);
        hover.ui_sounds_.initial_hover_ = false; hover_sounds = 0;
        hover.mouse_x_ = 695; hover.mouse_y_ = 405;
        const auto target = hover.HudSoundTarget();
        hover.ui_sounds_.Hover(target, 1000); hover.ui_sounds_.Hover(target, 1100);
        if (!check(target != 0 && hover_sounds == 1, "entering a scale grip plays one native hover cue")) return false;
        hover.ui_sounds_.Hover(0, 1200); hover.ui_sounds_.Hover(target, 1300, false);
        if (!check(hover_sounds == 1, "scale-grip hover stays quiet during dragging")) return false;
        hover.config_.bounds = {1500,20,1000,600};
        hover.player_.State().bounds.height = 6; hover.ResolveHudTransforms(viewport);
        hover.mouse_y_ = 410;
        if (!check(hover.EditGripAt(695,410) == &hover.player_ && hover.UpdateMouseCapture(viewport,0) &&
            hover.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal,
            "thin modules retain the full grip hit area and resize cursor")) return false;
        hover.SetBasicSettings(true, false);
        hover.config_.sidebar_collapsed = true;
        const auto overlaps = [](const ui::rect& a, const ui::rect& b) {
            return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
        };
        party_member previewMember;
        previewMember.id = 0xFFFF0000; previewMember.active = previewMember.in_zone = previewMember.preview = true;
        previewMember.name = "Player"; previewMember.main_job = 2;
        previewMember.hp = previewMember.hp_max = 9999; previewMember.hp_percent = 100;
        previewMember.mp = 8349; previewMember.mp_max = 10000; previewMember.mp_percent = 83; previewMember.tp = 2900;
        for (const auto bounds : {ui::rect{100,100,760,350}, ui::rect{100,100,1100,600}})
        {
            hover.config_.bounds = bounds;
            const ui::basic_settings_layout basic{bounds};
            const auto layout = hover.ConfigLayout();
            const auto items = hover.BasicControllerItems(config_controller_region::options);
            bool separated = items.size() == ui::basic_settings_layout::control_count && layout.ProfileSelector().width > 26;
            for (size_t i = 0; i < items.size(); ++i)
            {
                const auto& control = items[i].bounds;
                separated &= control.x >= bounds.x && control.y >= bounds.y && control.x + control.width <= bounds.x + bounds.width &&
                    control.y + control.height <= basic.FooterBackground().y;
                for (size_t j = i + 1; j < items.size(); ++j) separated &= !overlaps(control, items[j].bounds);
            }
            if (!check(separated && !overlaps(layout.ProfileActionsButton(), layout.Footer()) &&
                !overlaps(hover.SettingsModeBounds(), layout.Footer()) && !overlaps(hover.SettingsModeBounds(), layout.ProfileActionsButton()) &&
                hover.SettingsModeBounds().y >= basic.FooterBackground().y && layout.ProfileSelector().x == bounds.x + 24,
                "Basic controls and the footer mode button stay separate at minimum and normal sizes")) return false;
            module_render_context previewContext{hover.design_system_};
            for (size_t preset = 0; preset < ui::hud_presets.size(); ++preset)
            {
                const auto area = basic.Preview(preset);
                auto& sample = hover.basic_preset_samples_[preset];
                hover.design_system_.SetAppearanceTheme(ui::hud_presets[preset].appearance);
                const auto row = sample.RenderSample(previewMember, area, previewContext);
                hover.design_system_.SetAppearanceTheme(ui::native_appearance);
                if (!check(row.x >= area.x && row.y >= area.y && row.x + row.width <= area.x + area.width && row.y + row.height <= area.y + area.height,
                    "all three themed party samples fit both Basic window sizes")) return false;
            }
            for (const int liveTheme : {ui::native_appearance, ui::xiv1_appearance})
            {
                hover.general_.SetAppearanceTheme(liveTheme);
                hover.design_system_.SetAppearanceTheme(liveTheme);
                hover.container_renderer_.SetAppearanceTheme(liveTheme);
                const auto beforeSamples = hover.CaptureSettings();
                hover.DrawBasicSettings(viewport);
                const auto afterSamples = hover.CaptureSettings();
                if (!check(hover.design_system_.IsXiv1Theme() == (liveTheme == ui::xiv1_appearance) &&
                    hover.container_renderer_.AppearanceTheme() == liveTheme &&
                    !hover.basic_preset_samples_[0].xiv1_theme_ && !hover.basic_preset_samples_[1].xiv1_theme_ && hover.basic_preset_samples_[2].xiv1_theme_ &&
                    beforeSamples.ReadInt("appearance", "theme", -1) == afterSamples.ReadInt("appearance", "theme", -1) &&
                    beforeSamples.ReadRect("layout.party", {}).x == afterSamples.ReadRect("layout.party", {}).x,
                    "preset samples keep their own appearance and restore the live renderer without changing saved settings")) return false;
            }
            hover.general_.SetAppearanceTheme(ui::native_appearance);
            hover.design_system_.SetAppearanceTheme(ui::native_appearance);
            hover.container_renderer_.SetAppearanceTheme(ui::native_appearance);
        }
        hover.SetBasicSettings(false, false);
        for (const bool compact : {false, true})
            for (const auto bounds : {ui::rect{100,100,760,400}, ui::rect{100,100,1100,600}})
            {
                hover.config_.bounds = bounds; hover.config_.sidebar_collapsed = compact;
                const auto layout = hover.ConfigLayout();
                const auto mode = hover.SettingsModeBounds(), profile = layout.ProfileSelector(), add = layout.ProfileAdd(), actions = layout.ProfileActionsButton();
                if (!check(mode.y >= layout.FooterBackground().y && profile.y == add.y && add.y == actions.y &&
                    profile.width == 176 && profile.x == bounds.x + 24 &&
                    !overlaps(profile, add) && !overlaps(add, actions) && !overlaps(actions, layout.Footer()) && !overlaps(mode, layout.Footer()) &&
                    layout.ContentArea().y + layout.ContentArea().height <= layout.FooterBackground().y,
                    "Advanced shares the Basic footer without overlapping settings or compact navigation")) return false;
                const auto utilities = hover.ControllerConfigItems(config_controller_region::utilities);
                const auto controllerMode = std::find_if(utilities.begin(), utilities.end(), [](const auto& item) { return item.id == "settings-mode"; });
                if (!check(controllerMode != utilities.end() && controllerMode->bounds.y == mode.y,
                    "controller mode navigation follows the Advanced footer button")) return false;
            }
        const auto advancedMode = hover.SettingsModeBounds();
        hover.mouse_x_ = static_cast<int>(advancedMode.x + 5); hover.mouse_y_ = static_cast<int>(advancedMode.y + 5);
        if (!check(hover.ProcessConfigClick(viewport) && hover.config_.basic, "Advanced footer returns to Basic through the shared mode action")) return false;
        auto& classicSample = hover.basic_preset_samples_[0];
        auto& xivSample = hover.basic_preset_samples_[1];
        const auto classicBars = classicSample.LayoutEntry({0,0,300,0}, true, classicSample.ResourceSlots(previewMember), false).resources.bars;
        const auto xivBars = xivSample.LayoutEntry({0,0,300,0}, true, xivSample.ResourceSlots(previewMember), false).resources.bars;
        if (!check(classicSample.tp_display_ == 1 && classicBars[1].y > classicBars[0].y &&
            xivSample.tp_display_ == 0 && xivBars[0].x < xivBars[1].x && xivBars[1].x < xivBars[2].x &&
            closeTo(xivBars[0].y, xivBars[1].y) && closeTo(xivBars[1].y, xivBars[2].y),
            "Classic preserves its stacked Party layout and XIV preserves three horizontal bars")) return false;
        static constexpr const char* sampleTextNames[]{"name", "jobs", "labels", "values", "details", "cast", "heading", "buffs", "debuffs", "tp", "mp", "zone"};
        for (size_t preset = 0; preset < ui::hud_presets.size(); ++preset)
        {
            const auto bytes = resources::LoadEmbeddedResource(reinterpret_cast<HMODULE>(&__ImageBase), ui::hud_presets[preset].resource, 1024 * 1024);
            if (!check(!bytes.empty(), "Basic sample typography has an embedded preset")) return false;
            const auto reference = settings_store::FromIniText({reinterpret_cast<const char*>(bytes.data()), bytes.size()});
            for (size_t i = 0; i < std::size(sampleTextNames); ++i)
            {
                const auto section = std::string{"text.party."} + sampleTextNames[i];
                const auto& style = hover.basic_preset_samples_[preset].text_[i].style;
                if (!check(style.font == reference.ReadInt(section.c_str(), "font", -1) &&
                    closeTo(style.scale, reference.ReadFloat(section.c_str(), "scale", -1)),
                    (std::string{ui::hud_presets[preset].name} + " sample preserves its captured " + sampleTextNames[i] + " typography").c_str())) return false;
            }
        }
        const ui::basic_settings_layout scaleLayout{hover.config_.bounds};
        const auto track = scaleLayout.ScaleTrack(), value = scaleLayout.ScaleValue(), reset = scaleLayout.ResetButton();
        hover.mouse_x_ = static_cast<int>(track.x + track.width * .5f); hover.mouse_y_ = static_cast<int>(value.y + value.height * .5f);
        if (!check(hover.ProcessBasicSettingsClick(viewport) && closeTo(hover.general_.HudScale(), 1.25f) &&
            closeTo(value.y + value.height * .5f, track.y + track.height * .5f) && value.y == reset.y && value.height == reset.height,
            "compact scale rendering and pointer input share aligned geometry")) return false;
        hover.FinishInteraction();
        hover.mouse_x_ = static_cast<int>(value.x + 5); hover.mouse_y_ = static_cast<int>(value.y + 1);
        if (!check(hover.ProcessBasicSettingsClick(viewport) && hover.config_.slider_input.Focused(),
            "the full-height percentage field opens the shared numeric editor")) return false;
        hover.FinishConfigInput(false);
        hover.config_.search_expansion = 1;
        hover.mouse_x_ = static_cast<int>(hover.config_.bounds.x + hover.config_.bounds.width - 200);
        hover.mouse_y_ = static_cast<int>(hover.config_.bounds.y + 12);
        if (!check(hover.SettingsSearchTarget(viewport) == 0 && !hover.ProcessSettingsSearchClick(viewport) &&
            hover.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::move,
            "the former search flyout area stays available for dragging Basic's header")) return false;
        const auto modeButton = hover.SettingsModeBounds();
        hover.mouse_x_ = static_cast<int>(modeButton.x + 5); hover.mouse_y_ = static_cast<int>(modeButton.y + 5);
        if (!check(hover.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand && hover.ProcessConfigClick(viewport) && !hover.config_.basic,
            "the visible footer button opens Advanced without search intercepting its click")) return false;
        hover.SetBasicSettings(true, false);
        const auto utilities = hover.ControllerConfigItems(config_controller_region::utilities);
        if (!check(std::none_of(utilities.begin(), utilities.end(), [](const auto& item) {
            return item.id == "search" || item.id == "sidebar" || item.id == "scale";
        }), "Basic utilities expose only visible controls")) return false;
        const ui::basic_settings_layout basic{hover.config_.bounds};
        const auto card = basic.Preset(0), apply = basic.PresetButton(0);
        hover.mouse_x_ = static_cast<int>(card.x + card.width * .5f); hover.mouse_y_ = static_cast<int>(card.y + card.height * .5f);
        if (!check(hover.ConfigSoundTarget(viewport) == 0, "preset samples do not create phantom hover actions")) return false;
        hover.mouse_x_ = static_cast<int>(apply.x + 4); hover.mouse_y_ = static_cast<int>(apply.y + 4);
        if (!check(hover.ProcessBasicSettingsClick(viewport) && hover.profile_name_.Focused() && hover.profile_keyboard_selection_ == 0,
            "the visible Apply button opens the shared confirmation with Cancel selected")) return false;
        hover.CloseProfileControls();
        for (const bool simple : {false, true})
        {
            hover.SetBasicSettings(simple, false);
            const auto add = hover.ConfigLayout().ProfileAdd();
            hover.mouse_x_ = static_cast<int>(add.x + 4); hover.mouse_y_ = static_cast<int>(add.y + 4);
            hover.DrawConfig(viewport);
            if (!check(hover.config_.tooltip == "Create profile", "profile help appears in both settings views")) return false;
            hover.mouse_x_ = hover.mouse_y_ = 0;
            hover.DrawConfig(viewport);
            if (!check(hover.config_.tooltip.empty() && hover.config_.tooltip_anchor.width == 0,
                "moving away clears the previous frame's tooltip in both settings views")) return false;
        }
        hover.ActivateBasicItem(2, controller_action::actions, false);
        if (!check(hover.config_.slider_input.Focused() && hover.config_.slider_option == hover.general_.HudScaleStart(),
            "Basic controller scale entry uses the shared percentage editor")) return false;
        hover.FinishConfigInput(false);
        hover.config_.focus.active = true; hover.config_.focus.region = config_controller_region::options;
        hover.config_.focus.id = "basic/Reset Global Scale"; hover.config_.scroll = 17;
        hover.RefreshControllerConfigFocus();
        if (!check(hover.config_.scroll == 17, "Basic focus never scrolls the hidden Advanced option grid")) return false;
        if (!check(!hover.ActivateBasicItem(ui::basic_settings_layout::control_count, controller_action::confirm, false) && !hover.config_.collapsed,
            "invalid Basic controller targets cannot activate an action")) return false;
        const auto basicItems = hover.BasicControllerItems(config_controller_region::options);
        if (!check(basicItems[2].id == "basic/Global Scale" && basicItems[3].id == "basic/Reset Global Scale" &&
            basicItems[4].id == "basic/XIVClassic" && basicItems[5].id == "basic/Theme",
            "new Basic controls retain the existing scale/reset focus identities")) return false;
        const auto beforeTheme = hover.CaptureSettings();
        const ui::basic_settings_layout themeLayout{hover.config_.bounds};
        const auto xivThemeButton = ui::config_controls::Segment(themeLayout.ThemeChoices(), 1, 2);
        hover.mouse_x_ = static_cast<int>(xivThemeButton.x + 5); hover.mouse_y_ = static_cast<int>(xivThemeButton.y + 5);
        if (!check(hover.ProcessBasicSettingsClick(viewport) && hover.general_.AppearanceTheme() == ui::xiv1_appearance &&
            !hover.profile_name_.Focused(), "Basic theme buttons select the shared appearance without preset confirmation")) return false;
        hover.config_.focus.adjusting = false;
        hover.ActivateBasicItem(ui::basic_settings_layout::theme_control, controller_action::confirm, false);
        hover.ActivateBasicItem(ui::basic_settings_layout::theme_control, controller_action::left, false);
        hover.ActivateBasicItem(ui::basic_settings_layout::theme_control, controller_action::confirm, false);
        const auto afterTheme = hover.CaptureSettings();
        const auto beforePlayer = beforeTheme.ReadRect("layout.player", {}), afterPlayer = afterTheme.ReadRect("layout.player", {});
        if (!check(hover.general_.AppearanceTheme() == ui::native_appearance && !hover.config_.focus.adjusting &&
            beforePlayer.x == afterPlayer.x && beforePlayer.y == afterPlayer.y && beforePlayer.width == afterPlayer.width && beforePlayer.height == afterPlayer.height &&
            beforeTheme.ReadFloat("appearance.hud", "scale", 0) == afterTheme.ReadFloat("appearance.hud", "scale", 0),
            "controller theme selection shares the pointer action and preserves HUD placement and scale")) return false;
        if (!check(hover.ActivateBasicItem(ui::basic_settings_layout::xivclassic_control, controller_action::confirm, false) &&
            hover.profile_name_.Focused() && hover.profile_keyboard_selection_ == 0 &&
            hover.confirmation_option_ == hover.general_.HudPresetOption(2),
            "the third Basic card opens XIVClassic through the shared confirmation with Cancel selected")) return false;
        hover.CloseProfileControls();
        hover.config_.collapsed = true;
        const auto collapsedUtilities = hover.ControllerConfigItems(config_controller_region::utilities);
        if (!check(hover.SettingsModeBounds().width == 0 && std::none_of(collapsedUtilities.begin(), collapsedUtilities.end(), [](const auto& item) {
            return item.id == "settings-mode";
        }), "collapsed Basic settings cannot focus or click the hidden footer button")) return false;
        hover.config_.edit_mode = true;
        const auto commandIndex = native_hud_window_count + NativeMenuWindowIndex("playermo");
        auto& commandWindow = hover.native_windows_[commandIndex];
        hover.native_ui_.menu_available = true;
        hover.native_ui_.reference_width = 2560; hover.native_ui_.reference_height = 1440;
        hover.native_ui_.windows[commandIndex] = {true, true, 400, 400, 112, 168, true};
        hover.SetNativeEditMode(true); hover.UpdateNativeWindows(viewport); hover.FocusModuleConfig(commandWindow);
        commandWindow.State().scale = {2,2};
        *hover.general_.Option(hover.general_.HudScaleStart()).slider = 1.5f;
        hover.ResolveHudTransforms(viewport);
        hover.config_.layout_selection = &commandWindow;
        if (!check(hover.ModuleDisplayed(commandWindow), "Command Menu movement fixture uses the actual native window")) return false;
        const auto commandToolbar = hover.MovementToolbar(viewport);
        hover.BeginScale(commandWindow);
        if (!check(!commandWindow.SupportsScale() && commandWindow.OptionCount() == commandWindow.ContentOptionCount() &&
            commandWindow.EditGrip() == edit_grip_kind::none && commandWindow.State().transform.Identity() &&
            commandToolbar && commandToolbar->Button(2).width == 0 && hover.scale_gesture_.module == nullptr,
            "Commands remain movable at native size without scale settings, grips, toolbar actions or keyboard gestures")) return false;
        return true;
    }
}
