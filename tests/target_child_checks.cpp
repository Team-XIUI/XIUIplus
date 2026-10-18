#include "modules/target/target_child_module.hpp"

#include <cstdio>

namespace xiui
{
    struct target_children_test
    {
        static auto Check(const bool value, const char* message) -> bool
        { if (!value) std::printf("FAIL: %s\n", message); return value; }

        static auto Run(void) -> bool
        {
            target_module parent;
            const auto theme = ui::theme::CreateDefault();
            parent.ApplyMeterDefaults(theme.meter); parent.ApplyTextDefaults(theme);
            parent.Load(settings_store::FromIniText(""));
            target_child_module subtarget{parent, true}, recipient{parent, false};
            const auto empty = settings_store::FromIniText(""); subtarget.Load(empty); recipient.Load(empty);
            if (!Check(!subtarget.HasEditableBounds() && !recipient.HasEditableBounds() && !subtarget.PreviewVisible() &&
                parent.settings_.subtarget.enabled && parent.settings_.subtarget.percent == parent.settings_.show_percent,
                "missing child settings retain the attached Subtarget and linked recipient defaults")) return false;
            parent.details_ready_ = true;
            for (size_t slot = 0; slot < 3; ++slot)
            {
                auto& view = parent.views_[slot];
                view.id = static_cast<uint32_t>(101 * (slot + 1)); view.index = static_cast<uint32_t>(10 + slot);
                view.name = slot == 0 ? "Main" : slot == 1 ? "Subtarget" : "Recipient"; view.health = true; view.hp = 80;
                view.status_count = 1; view.statuses[0] = {static_cast<uint16_t>(slot == 0 ? 33 : slot == 1 ? 2 : 10)};
            }
            parent.settings_.recipient.statuses = true;
            ui::design_system design;
            const module_render_context context{design};
            parent.BeginRenderFrame(); parent.Render({}, context);
            if (!Check(parent.status_hits_.size() == 3 && parent.views_[0].id == 101 && parent.views_[1].id == 202 &&
                parent.status_hits_[0].entity == 101 && parent.status_hits_[1].entity == 303 && parent.status_hits_[2].entity == 202,
                "main target, recipient and native subtarget retain separate read-model identities")) return false;
            *subtarget.Option(13).choice = 1;
            parent.settings_.recipient.placement = target_of_target_settings::detached;
            const auto mainBounds = parent.State().bounds;
            subtarget.State().bounds = {200, 300, 320, 80}; recipient.State().bounds = {700, 400, 180, 80};
            parent.BeginRenderFrame(); parent.Render({}, context);
            if (!Check(parent.status_hits_.size() == 1 && subtarget.PreviewVisible() && recipient.PreviewVisible() &&
                !parent.settings_.recipient.ShowArrow(), "independent children leave the main draw pass and connecting arrow")) return false;
            subtarget.Render({}, context); recipient.Render({}, context);
            if (!Check(parent.status_hits_.size() == 3 && parent.State().bounds.x == mainBounds.x &&
                parent.State().bounds.width == mainBounds.width && subtarget.State().bounds.x == 200 && recipient.State().bounds.x == 700,
                "each independent child renders once at its own position without moving Target")) return false;
            parent.State().enabled = false;
            parent.BeginRenderFrame(); subtarget.Render({}, context); recipient.Render({}, context);
            if (!Check(subtarget.ShouldRender({}) && recipient.ShouldRender({}) && parent.HasStatusDetails() && parent.BeginControllerStatus(),
                "independent child displays and actual status inspection remain available with main Target disabled")) return false;
            parent.views_[1] = {};
            parent.BeginRenderFrame(); recipient.Render({}, context); subtarget.Render({}, context);
            if (!Check(!subtarget.ShouldRender({}) && parent.views_[0].id == 101 && parent.views_[2].id == 303 &&
                parent.status_hits_.size() == 1, "cancelled subtarget read-model clears its display without changing main or recipient identity")) return false;
            recipient.SetDisplayAllowed(false);
            if (!Check(!recipient.PreviewVisible() && !recipient.ShouldRender({}), "hidden independent recipient has no preview or normal display")) return false;
            recipient.SetDisplayAllowed(true);
            parent.settings_.recipient.placement = target_of_target_settings::inline_right;
            if (!Check(parent.settings_.recipient.ShowArrow() && !recipient.PreviewVisible(),
                "reattaching recipient restores its retained connecting arrow and removes detached edit bounds")) return false;
            auto saved = settings_store::FromIniText(""); parent.Save(saved); subtarget.Save(saved); recipient.Save(saved);
            target_module loaded; loaded.ApplyMeterDefaults(theme.meter); loaded.ApplyTextDefaults(theme); loaded.Load(saved);
            target_child_module loadedSub{loaded, true}, loadedRecipient{loaded, false}; loadedSub.Load(saved); loadedRecipient.Load(saved);
            if (!Check(loadedSub.HasEditableBounds() && loadedSub.State().bounds.x == 200 && loadedRecipient.State().bounds.x == 700 &&
                loaded.settings_.recipient.placement == target_of_target_settings::inline_right,
                "child placements and independent positions survive parent and adapter persistence")) return false;
            combat_source combat; party_source party;
            parent.UpdateDetails(nullptr, combat, party, 1000);
            return Check(parent.views_[0].id == 0 && parent.views_[1].id == 0 && parent.views_[2].id == 0 && !recipient.ShouldRender({}),
                "unavailable native source clears main, subtarget and recipient identities");
        }
    };

    auto RunTargetChildChecks(void) -> bool { return target_children_test::Run(); }
}
