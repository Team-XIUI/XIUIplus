#include "config/module_visibility.hpp"

#include <array>
#include <stdexcept>

namespace xiui
{
    namespace
    {
        class visibility_module final : public hud_module
        {
        public:
            explicit visibility_module(const char* id) : id_(id) {}
            auto Id(void) const -> const char* override { return this->id_; }
            auto DisplayName(void) const -> const char* override { return this->id_; }
            auto State(void) -> module_state& override { return this->state_; }
            auto State(void) const -> const module_state& override { return this->state_; }
            auto ResetDefaults(const ui::theme&) -> void override {}
            auto Load(const settings_store&) -> void override {}
            auto Save(const settings_store&) const -> void override {}
            auto OptionCount(void) const -> size_t override { return 0; }
            auto Option(size_t) -> module_option override { return {}; }
            auto ShouldRender(const hud_snapshot&) const -> bool override { return this->state_.enabled; }
            auto Render(const hud_snapshot&, const module_render_context&) -> void override {}

        private:
            const char* id_;
            module_state state_{};
        };

        auto Check(const bool value, const char* message) -> void
        { if (!value) throw std::runtime_error{message}; }
    }

    auto RunVisibilityChecks(void) -> void
    {
        visibility_module player{"player"}, party{"party"}, alliance1{"alliance1"}, alliance2{"alliance2"},
            inventory{"inventory"}, gil{"gil"}, buffs{"buffs"}, debuffs{"debuffs"};
        std::array<hud_module*, 8> modules{&player, &party, &alliance1, &alliance2, &inventory, &gil, &buffs, &debuffs};
        module_visibility_settings settings;
        settings.SetModules(modules);
        settings.Load(settings_store::FromIniText("[inventory]\nhide_menu=1\n[gil]\nhide_menu=1\n"));
        module_visibility_context context{};
        context.event_active = true;
        Check(!settings.Visible("party", context) && settings.Visible("player", context),
            "legacy Party event defaults migrate without hiding unrelated modules");
        Check(settings.Visible("party", context, true), "editing retains enabled movement previews during events");
        context = {}; context.menu_open = true;
        Check(!settings.Visible("inventory", context) && !settings.Visible("gil", context) &&
            settings.Visible("player", context), "legacy menu preferences enter the shared policy");
        Check(inventory.State().enabled && gil.State().enabled, "context visibility never changes enabled flags");

        settings.Load(settings_store::FromIniText("[visibility]\nhide_menu=1\nkeep_macro=1\n[visibility.gil]\nkeep_macro=0\n"));
        context.macro_palette = true;
        Check(settings.Visible("player", context) && !settings.Visible("gil", context),
            "native macro exception requires both global and local permission");
        settings.Load(settings_store::FromIniText("[visibility]\nhide_menu=1\nkeep_macro=0\n"));
        Check(!settings.Visible("player", context), "a disabled global macro exception applies to every module");

        settings.Load(settings_store::FromIniText("[visibility]\nalliance_only_menu=1\n[visibility.party]\nhide_menu=1\n"));
        context.macro_palette = false;
        Check(settings.Visible("party", context) && !settings.Visible("alliance1", context) &&
            !settings.Visible("alliance2", context) && settings.Visible("player", context),
            "alliance-only menu hiding preserves Party and unrelated modules");

        settings.Load(settings_store::FromIniText("[visibility]\nplayer_in_party=1\nhide_events=1\n"));
        context = {}; context.in_party = true;
        Check(!settings.Visible("player", context) && settings.Visible("buffs", context),
            "Player hide-in-party does not hide independent status groups");
        context.event_active = true;
        Check(!settings.Visible("buffs", context) && !settings.Visible("debuffs", context),
            "global event rules include both new status groups");
        context = {}; context.map_open = true;
        Check(!settings.Visible("player", context) && settings.Visible("player", context, true),
            "native map visibility suspends during editing");
        context = {}; context.interface_hidden = true;
        Check(!settings.Visible("buffs", context), "native interface hiding reaches every HUD module");

        auto saved = settings_store::FromIniText("");
        settings.Save(saved);
        module_visibility_settings restored;
        restored.SetModules(modules); restored.Load(saved);
        context = {}; context.in_party = true;
        Check(!restored.Visible("player", context), "canonical visibility settings survive a round trip");
    }
}
