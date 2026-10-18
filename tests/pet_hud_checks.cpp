#include "modules/pet/pet_module.hpp"

#include <cstdio>

namespace xiui
{
    struct pet_hud_test
    {
        static auto Check(bool value, const char* message) -> bool { if (!value) std::printf("FAIL: %s\n", message); return value; }
        static auto PortraitPresets(void) -> bool
        {
            pet_module module;
            const auto old = settings_store::FromIniText(
                "[layout.pet]\nx=408\ny=602\nwidth=320\nheight=95\n[pet.avatar]\nportrait_size=126\nportrait_position=1\n"
                "[pet.portrait.carbuncle]\nscale=1.65\nopacity=0.65\noffset_x=173\noffset_y=-77\nclip=1\n"
                "[pet.portrait.ifrit]\nscale=1\nopacity=1\noffset_x=0\noffset_y=0\nclip=0\n"
                "[pet.portrait.shiva]\nscale=1\nopacity=0.9\noffset_x=12\noffset_y=0\nclip=0\n");
            module.Load(old);
            const auto& carbuncle = module.PortraitStyle(pet_kind::avatar, "Carbuncle");
            const auto& ifrit = module.PortraitStyle(pet_kind::avatar, "Ifrit");
            const auto& shiva = module.PortraitStyle(pet_kind::avatar, "Shiva");
            if (!Check(carbuncle.offset_x == 173 && carbuncle.offset_y == -77 && carbuncle.scale == 1.65f && carbuncle.opacity == .65f &&
                ifrit.clip && ifrit.opacity == .65f && ifrit.offset_y == -3 && shiva.opacity == .9f && shiva.offset_x == 12 && !shiva.clip &&
                module.State().bounds.x == 408 && module.State().bounds.y == 602 && module.types_[0].portrait_size == 126,
                "portrait presets replace untouched legacy images while retaining Carbuncle, custom images and window positions")) return false;
            for (size_t index = 3; index < module.avatar_portraits_.size(); ++index)
                if (!Check(module.avatar_portraits_[index].clip && module.avatar_portraits_[index].opacity == .65f,
                    "every unconfigured avatar and spirit receives a clipped portrait preset")) return false;
            module.avatar_portraits_[1] = {};
            module.types_[0].portrait_size = 48; module.types_[0].portrait_position = 0;
            auto saved = settings_store::FromIniText(""); module.Save(saved);
            pet_module reloaded; reloaded.Load(saved);
            if (!Check(reloaded.avatar_portraits_[1].LegacyDefault() && reloaded.types_[0].portrait_size == 48 &&
                reloaded.types_[0].portrait_position == 0 && saved.ReadInt("pet", "portrait_preset_version", 0) == 1,
                "the versioned preset migration preserves later customization, including a return to old values")) return false;
            reloaded.Load(settings_store::FromIniText("[pet.avatar]\nportrait_size=48\nportrait_position=0\n"));
            return Check(reloaded.types_[0].portrait_size == 126 && reloaded.types_[0].portrait_position == 1,
                "untouched legacy avatar layouts adopt the shared overlay base size once");
        }
        static auto TargetBackground(void) -> bool
        {
            pet_module module, target{true}; target.SetOwner(&module);
            module.Load(settings_store::FromIniText("[pet.avatar]\nbackground=0\n[pet.target]\nenabled=1\n"));
            if (!Check(module.target_settings_.background && !module.target_settings_.tint && !module.types_[0].background,
                "Pet Target defaults to its own untinted background independently of Pet artwork settings")) return false;
            module.SelectConfigView(pet_kind_count);
            if (!Check(module.Option(12).toggle == target.Option(12).toggle && target.ConfigSectionContains(1, 12) &&
                target.ConfigSectionContains(1, 15) && target.ConfigSectionContains(1, 16) && target.ConfigSectionContains(1, 17),
                "attached and independent Pet Target share reachable background and tint controls")) return false;
            *target.Option(13).slider = 0.4f;
            *target.Option(14).slider = 9;
            *target.Option(15).toggle = true;
            *target.Option(16).color = 0x8066CCAA;
            *target.Option(17).slider = 0.5f;
            module.target_settings_.snapped = false;
            target.State().bounds = {700, 320, 260, 80};
            auto saved = settings_store::FromIniText(""); module.Save(saved); target.Save(saved);
            pet_module restored, restoredTarget{true}; restoredTarget.SetOwner(&restored);
            restored.Load(saved); restoredTarget.Load(saved);
            const auto& settings = restored.target_settings_;
            if (!Check(settings.background && settings.tint && settings.container.opacity == 0.4f && settings.container.padding == 9 &&
                settings.TintColor() == 0x4066CCAA && restoredTarget.State().bounds.x == 700 && restoredTarget.State().bounds.y == 320 &&
                !restored.types_[0].background,
                "background and tint settings persist with independent placement and preserve Pet background preferences")) return false;
            ui::design_system design; module_render_context context{design, nullptr, true};
            const auto attachedHeight = restored.DrawTarget({}, {24, 500, 260, 0}, context);
            restoredTarget.Render({}, context);
            if (!Check(restoredTarget.State().bounds.height == attachedHeight,
                "attached and independent target containers use the same padded layout")) return false;
            *restoredTarget.Option(12).toggle = false;
            auto disabled = settings_store::FromIniText(""); restored.Save(disabled);
            pet_module reloaded; reloaded.Load(disabled);
            return Check(!reloaded.target_settings_.background && reloaded.target_settings_.tint &&
                reloaded.target_settings_.TintColor() == settings.TintColor(),
                "disabling the background survives reload while retaining its optional tint preferences");
        }
        static auto Portraits(void) -> bool
        {
            pet_module module;
            module.State().enabled = true;
            *module.Option(10).toggle = false;
            *module.Option(22).choice = 1;
            *module.Option(23).choice = 1;
            *module.Option(24).slider = 2;
            *module.Option(25).slider = 0.35f;
            *module.Option(26).slider = 60;
            *module.Option(27).slider = -40;
            *module.Option(28).toggle = true;
            *module.Option(23).choice = 3;
            *module.Option(24).slider = 0.5f;
            if (!Check(module.PortraitStyle(pet_kind::avatar, "Ifrit").scale == 2 &&
                module.PortraitStyle(pet_kind::avatar, "Garuda").scale == 0.5f &&
                module.PortraitStyle(pet_kind::avatar, "Carbuncle").scale == 1.65f,
                "each summoned avatar resolves its own image settings independently of the editor selection")) return false;
            module.SelectConfigView(3);
            *module.Option(25).slider = 0.6f;
            if (!Check(module.PortraitStyle(pet_kind::wyvern, "Azure").opacity == 0.6f &&
                module.PortraitStyle(pet_kind::avatar, "Ifrit").opacity == 0.35f,
                "named wyverns retain separate image settings from avatars")) return false;
            auto saved = settings_store::FromIniText(""); module.Save(saved);
            pet_module restored; restored.Load(settings_store::FromIniText(saved.ToIniText()));
            const auto& image = restored.PortraitStyle(pet_kind::avatar, "Ifrit");
            const auto at = image.Bounds({100, 200, 320, 180}, 48);
            if (!Check(!restored.types_[0].background && restored.types_[0].portrait_position == 1 &&
                image.opacity == 0.35f && image.clip && at.x == 160 && at.y == 160 && at.width == 96 && at.height == 96 &&
                restored.selected_avatar_ == 3 && restored.PortraitStyle(pet_kind::avatar, "Air Spirit").scale == 1.65f,
                "background-free overlays preserve per-image scale, opacity, offsets, clipping and preview choice")) return false;
            restored.SelectConfigView(0);
            ui::design_system design; module_render_context context{design, nullptr, true};
            restored.Render({}, context);
            const auto bounds = restored.State().bounds;
            *restored.Option(26).slider = -180;
            *restored.Option(27).slider = 200;
            *restored.Option(24).slider = 4;
            restored.Render({}, context);
            if (!Check(restored.State().bounds.x == bounds.x && restored.State().bounds.y == bounds.y &&
                restored.State().bounds.height == bounds.height,
                "moving and scaling overlay artwork leaves the Pet window and resource layout fixed")) return false;
            for (size_t option : {size_t{6}, size_t{10}, size_t{19}, size_t{20}, size_t{22}, size_t{23}, size_t{24}, size_t{25}, size_t{26}, size_t{27}, size_t{28}})
                if (!Check(restored.ConfigSectionContains(8, option), "portrait controls share the Portrait settings topic")) return false;
            auto invalid = settings_store::FromIniText("[pet.portrait.ifrit]\nscale=999\nopacity=-1\noffset_x=-999\noffset_y=999\n");
            restored.Load(invalid);
            const auto& clamped = restored.PortraitStyle(pet_kind::avatar, "Ifrit");
            return Check(clamped.scale == 4 && clamped.opacity == 0 && clamped.offset_x == -600 && clamped.offset_y == 600,
                "imported image settings stay within the shared controls' supported ranges");
        }
        static auto Run(void) -> bool
        {
            if (!PortraitPresets() || !TargetBackground() || !Portraits()) return false;
            if (!Check(PetKind(15, 0, 0x100, "Carbuncle") == pet_kind::avatar && PetKind(9, 0, 0x100, "CourierCarrie") == pet_kind::jug &&
                PetKind(9, 0, 0x10, "Wild Rabbit") == pet_kind::charm && PetKind(14, 0, 0x100, "Azure") == pet_kind::wyvern &&
                PetKind(18, 0, 0x100, "Automaton") == pet_kind::automaton && PetRecast(102, pet_kind::jug) && !PetRecast(102, pet_kind::wyvern),
                "five pet categories and ability timer families remain separate")) return false;
            if (!Check(pet_module::LifetimeLabel(1000, 61000, false) == "Lifetime unavailable" &&
                pet_module::LifetimeLabel(1000, 61000, true) == "Lifetime unavailable  Observed 1:00" &&
                pet_module::LifetimeLabel(0, 61000, true) == "Lifetime unavailable",
                "unknown lifetime remains explicit and observed elapsed is never presented as a remaining timer")) return false;
            const auto now = ::GetTickCount64();
            pet_source source;
            pet_snapshot sample; sample.owner = 1; sample.zone = 2; sample.main_job = 9; sample.ready = true; sample.kind = pet_kind::jug;
            sample.pet.id = 101; sample.pet.index = 11; sample.pet.name = "CourierCarrie"; sample.pet.hp = 75;
            sample.tp = 1234; sample.tp_known = true;
            sample.target.id = 303; sample.target.index = 13; sample.target.name = "Enemy"; sample.target.hp = 80;
            sample.status_count = 1; sample.statuses[0] = {2, 0, now, 0, false};
            source.Publish(sample, now); source.Publish(sample, now + 1);
            if (!Check(source.State().observed_since == now && source.State().pet.hp == 75 && source.State().tp == 1234 && !source.State().mp_known,
                "same pet keeps its observed generation and available native vitals without fabricating MP")) return false;
            recast_source recasts; recasts.available_ = true;
            recasts.tracker_.Observe({recast_kind::ability, 102, 2, 3, 15, 30, "Ready"}, now);
            pet_module module, target{true}; target.SetOwner(&module); module.SetSource(&source); module.SetRecasts(&recasts);
            if (!Check(!module.State().enabled && !target.State().enabled && !target.PreviewVisible(), "new Pet and Pet Target displays start disabled")) return false;
            module.State().enabled = true; module.target_settings_.enabled = true;
            const auto cooldowns = module.Cooldowns(pet_kind::jug, now);
            if (!Check(cooldowns.size() == 1 && cooldowns[0].charges == 2 && cooldowns[0].maximum_charges == 3,
                "Pet reuses native Ready charges from the existing recast source")) return false;
            ui::design_system design; module_render_context context{design}; module.Render({}, context);
            if (!Check(module.HasStatusDetails() && module.BeginControllerStatus() && module.ControllerStatusBounds().width > 0,
                "confirmed observed pet effects support deliberate controller inspection")) return false;
            sample.pet.id = 202; sample.pet.index = 12; sample.status_count = 0; sample.target = {};
            source.Publish(sample, now + 10);
            if (!Check(source.State().observed_since == now + 10 && source.State().status_count == 0 && source.State().target.id == 0 &&
                module.ControllerStatusBounds().width == 0, "pet replacement clears status/target data and stale inspection identity")) return false;
            recasts.tracker_.FinishSample(now + 10);
            if (!Check(recasts.Entries()[0].charges == 3 && recasts.Entries()[0].Remaining(now + 10) == 0,
                "Ready completion follows the shared native recast tracker")) return false;
            sample.pet = {}; source.Publish(sample, now + 20);
            if (!Check(!module.ShouldRender({}), "pet loss hides the default Pet display")) return false;
            module.types_[1].without_pet = true;
            if (!Check(module.ShouldRender({}), "configured no-pet presentation can retain known native cooldowns")) return false;
            module.target_settings_.snapped = false; module.target_settings_.enabled = true;
            module.types_[1].text[0].style.glow.enabled = true; module.types_[1].meters[2].bookends = true;
            module.types_[1].portrait_size = 64; module.types_[1].lifetime = false;
            target.State().bounds = {700, 320, 260, 80};
            auto saved = settings_store::FromIniText(""); module.Save(saved); target.Save(saved);
            pet_module restored, restoredTarget{true}; restoredTarget.SetOwner(&restored); restored.Load(saved); restoredTarget.Load(saved);
            if (!Check(restored.types_[1].without_pet && restored.types_[1].text[0].style.glow.enabled && restored.types_[1].meters[2].bookends &&
                !restored.target_settings_.snapped && restoredTarget.State().bounds.x == 700 && restoredTarget.PreviewVisible(),
                "per-type styles and shared target placement persist independently")) return false;
            if (!Check(restored.types_[1].portrait && restored.types_[1].portrait_size == 64 && !restored.types_[1].lifetime,
                "per-type portrait and lifetime visibility preferences persist")) return false;
            size_t visits{}; restored.VisitTextSettings([&](ui::text_style&) { ++visits; }); restoredTarget.VisitTextSettings([&](ui::text_style&) { ++visits; });
            if (!Check(visits == pet_kind_count * 9 + 3, "global text visits every pet type and target profile exactly once")) return false;
            auto theme = ui::theme::CreateDefault(); theme.caption_text.scale = 0.77f; theme.meter.height = 13;
            pet_module reset, resetTarget{true}; resetTarget.SetOwner(&reset);
            reset.ResetDefaults(theme); resetTarget.ResetDefaults(theme);
            reset.ApplyMeterDefaults(theme.meter); reset.ApplyTextDefaults(theme);
            auto baseline = settings_store::FromIniText(""); reset.Save(baseline); resetTarget.Save(baseline);
            reset.types_[1].text[0].style.font = 3; reset.target_settings_.text[2].style.font = 3;
            reset.ResetDefaults(theme); reset.ApplyMeterDefaults(theme.meter); reset.ApplyTextDefaults(theme);
            resetTarget.ResetDefaults(theme); resetTarget.ApplyMeterDefaults(theme.meter); resetTarget.ApplyTextDefaults(theme);
            const auto missingPetDefaults = settings_store::FromIniText(""); reset.Load(missingPetDefaults); resetTarget.Load(missingPetDefaults);
            auto afterReset = settings_store::FromIniText(""); reset.Save(afterReset); resetTarget.Save(afterReset);
            if (!Check(afterReset.ToIniText() == baseline.ToIniText(),
                "registry-order reset preserves shared Pet Target theme defaults when captured keys are absent")) return false;
            combat_source combat; source.Update(nullptr, combat, now + 1000);
            return Check(!source.State().ready && source.State().pet.id == 0 && source.State().status_count == 0, "unavailable native source clears all pet state");
        }
    };
    auto RunPetHudChecks(void) -> bool { return pet_hud_test::Run(); }
}
