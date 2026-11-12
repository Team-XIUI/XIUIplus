#include "modules/target/target_module.hpp"

#include <cstdio>

namespace xiui
{
    struct target_display_test
    {
        static auto Check(bool value, const char* message) -> bool { if (!value) std::printf("FAIL: %s\n", message); return value; }
        static auto Run(void) -> bool
        {
            const auto originalPalette = shared_entity_name_palette;
            shared_entity_name_palette = {};
            const auto restore = [&] { shared_entity_name_palette = originalPalette; };
            if (!Check(ClassifyEntity(1, 0, false, true) == entity_class::group_player &&
                ClassifyEntity(2, 0, false, false) == entity_class::npc && ClassifyEntity(16, 7, true, false) == entity_class::group_mob &&
                ClassifyEntity(16, 7, false, false) == entity_class::other_mob && TargetIdSuffix(123456, 1) == " [456]" &&
                TargetIdSuffix(0x123ABC, 2) == " [ABC]" && TargetIdSuffix(0, 2).empty(), "entity classification and XIUI+ server-ID suffix formats"))
            { restore(); return false; }
            const auto native = EntityNameColor(16, 0, false, false, 0);
            shared_entity_name_palette.colors[3] = 0xFF123456; shared_entity_name_palette.enabled = true;
            if (!Check(EntityNameColor(16, 0, false, false, 0) == 0xFF123456 && native == 0xFFFFFFFF,
                "shared custom palette is opt-in and reaches existing entity-name consumers")) { restore(); return false; }
            target_module target; target.ApplyTextDefaults(ui::theme::CreateDefault()); target.Load(settings_store::FromIniText(""));
            {
                target_module self; self.Load(settings_store::FromIniText("[target]\nhide_self=1\n")); self.state_.enabled = true;
                self.details_ready_ = true; self.views_[0].id = 42; self.self_target_ = true;
                const auto option = self.Option(target_module::hide_self_option);
                size_t pages{};
                for (size_t section = 0; section < self.ConfigSections().size(); ++section) pages += self.ConfigSectionContains(section, target_module::hide_self_option);
                if (!Check(self.settings_.hide_self && !self.ShouldRender({}) && option.label != nullptr && pages == 1 && self.ConfigSectionContains(0, target_module::hide_self_option),
                    "Hide when targeting yourself hides the Target window and appears once on the General page")) { restore(); return false; }
                self.self_target_ = false;
                if (!Check(self.ShouldRender({}), "other targets still show the Target window")) { restore(); return false; }
                self.settings_.hide_self = false; self.self_target_ = true;
                if (!Check(self.ShouldRender({}), "targeting yourself still shows the window when the option is off")) { restore(); return false; }
            }
            const auto reserved = target_module::display_options_start + target_display_settings::base_options + 1;
            size_t owners{};
            for (size_t section = 0; section < target.ConfigSections().size(); ++section) owners += target.ConfigSectionContains(section, reserved);
            if (!Check(target.Option(reserved).label == nullptr && owners == 0,
                "reserved cast-meter height slot cannot enter the live configuration")) { restore(); return false; }
            target_module::target_view view{}; view.id = 900; view.name = "Monster"; view.kind = entity_class::unclaimed_mob; view.health = true; view.hp = 75;
            view.cast = {"Stone", 1000, 0, 4};
            ui::design_system design; module_render_context context{design}; meter_animation_state animation;
            const auto normal = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
            target.settings_.display.casts = false;
            const auto noCast = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
            target.settings_.display.name = false; target.settings_.show_distance = false;
            const auto compact = target.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
            if (!Check(noCast == normal && compact < noCast, "casting does not change Target flow while name/distance still control header height")) { restore(); return false; }
            {
                target_module themed; themed.ApplyTextDefaults(ui::theme::CreateDefault());
                design.SetAppearanceTheme(ui::xiv1_appearance);
                auto idle = view; idle.cast = {};
                const auto idleHeight = themed.DrawView(idle, {0, 0, 400, 0}, "", animation, 2000, context);
                const auto castHeight = themed.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
                themed.settings_.display.name = false; themed.settings_.show_distance = false;
                const auto hiddenIdle = themed.DrawView(idle, {0, 0, 400, 0}, "", animation, 2000, context);
                const auto hiddenCast = themed.DrawView(view, {0, 0, 400, 0}, "", animation, 2000, context);
                if (!Check(idleHeight == castHeight && hiddenIdle == hiddenCast,
                    "XIV target casts replace the header without shifting HP, statuses or attached children")) { restore(); return false; }
                themed.settings_.display.cast_placement = 1;
                themed.cast_state_.bounds.x = 123; themed.cast_state_.bounds.y = 456;
                themed.settings_.display.cast_height = 32;
                target_cast_module detached{themed};
                detached.Render({}, context);
                if (!Check(detached.State().bounds.height <= 32 && detached.State().bounds.x == 123 && detached.State().bounds.y == 456 &&
                    themed.settings_.display.cast_height == 32 && themed.settings_.display.cast_text.style.shadow.enabled,
                    "XIV independent casts stay compact while retaining saved positions and text effects")) { restore(); return false; }
                design.SetAppearanceTheme(ui::native_appearance);
            }
            target.settings_.display.casts = true;
            target.settings_.display.cast_width = 210; target.settings_.display.cast_offset_x = -7; target.settings_.display.cast_offset_y = 12;
            const auto attached = target.AttachedCastBounds(target.State().bounds);
            if (!Check(attached.x == target.State().bounds.x && attached.width == 210 &&
                attached.y == target.State().bounds.y + 5 + target.settings_.hp_style.height + 2 + 3 + 12,
                "cast geometry anchors directly below HP with independent width and offsets")) { restore(); return false; }
            const auto positionIndex = target_module::display_options_start + target_display_settings::layout_options;
            for (size_t index = positionIndex; index < positionIndex + 4; ++index)
            {
                size_t count{};
                for (size_t section = 0; section < target.ConfigSections().size(); ++section) count += target.ConfigSectionContains(section, index);
                if (!Check(count == 1 && target.ConfigSectionContains(9, index), "cast placement controls belong only to Cast Bar")) { restore(); return false; }
            }
            target.settings_.display.cast_placement = 1; target.OnOptionChanged(positionIndex);
            target_cast_module independent{target};
            if (!Check(independent.HasEditableBounds() && independent.State().bounds.x == attached.x && independent.State().bounds.y == attached.y,
                "independent cast enters shared movement at its attached position")) { restore(); return false; }
            independent.State().bounds.x = 333; independent.State().bounds.y = 444; independent.OnPositionChanged();
            target.settings_.display.cast_placement = 0;
            if (!Check(!independent.HasEditableBounds(), "attached casts leave independent movement")) { restore(); return false; }
            target.settings_.display.cast_placement = 1; target.OnOptionChanged(positionIndex);
            if (!Check(independent.State().bounds.x == 333 && independent.State().bounds.y == 444,
                "reattaching and detaching retain the separate cast position")) { restore(); return false; }
            target.settings_.display.casts = false;
            target.settings_.display.id_format = 2; target.settings_.display.entity_fills = true;
            target.settings_.display.fill_bottom[3] = 0xFFAABBCC; shared_entity_name_palette.enabled = true;
            shared_entity_name_palette.colors[3] = 0xFF123456;
            auto saved = settings_store::FromIniText(""); target.Save(saved);
            target_module loaded; loaded.Load(saved);
            const bool persisted = !loaded.settings_.display.casts && !loaded.settings_.display.name && loaded.settings_.display.id_format == 2 &&
                loaded.settings_.display.fill_bottom[3] == 0xFFAABBCC && shared_entity_name_palette.colors[3] == 0xFF123456 &&
                loaded.settings_.display.cast_placement == 1 && loaded.settings_.display.cast_width == 210 &&
                loaded.settings_.display.cast_offset_x == -7 && loaded.settings_.display.cast_offset_y == 12 &&
                loaded.cast_state_.bounds.x == 333 && loaded.cast_state_.bounds.y == 444;
            restore();
            return Check(persisted, "Target display choices, entity gradients and shared name palette persist");
        }
    };
    auto RunTargetDisplayChecks(void) -> bool { return target_display_test::Run(); }
}
