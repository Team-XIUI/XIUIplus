#include "general_module.hpp"
#include "config/clipboard.hpp"
#include <shellapi.h>

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto general_module::ResetDefaults(const ui::theme&) -> void
    {
        this->requested_window_type_ = -1;
        this->dark_backing_opacity_ = 1.0f;
        this->inventory_enabled_ = true;
        this->party_enabled_ = true;
        this->grid_ = {};
        this->global_text_ = {};
        this->config_text_ = {};
    }

    auto general_module::Load(const settings_store& store) -> void
    {
        this->dark_backing_opacity_ = std::clamp(store.ReadFloat("appearance", "dark_backing_opacity", 1.0f), 0.0f, 1.0f);
        this->inventory_enabled_ = store.ReadBool("modules", "inventory", true);
        this->party_enabled_ = store.ReadBool("modules", "party", true);
        this->global_text_.Load(store);
        this->config_text_.Load(store);
        this->grid_.visible = store.ReadBool("layout.grid", "visible", true);
        this->grid_.snap_grid = store.ReadBool("layout.grid", "snap_grid", false);
        this->grid_.snap_elements = store.ReadBool("layout.grid", "snap_elements", true);
        const auto read = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const float value = store.ReadFloat("layout.grid", key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->grid_.amount = std::round(read("amount", 6, 1, 20));
        this->grid_.spacing = std::round(read("spacing", 20, 4, 128));
        this->grid_.opacity = read("opacity", 0.25f, 0, 1);
    }

    auto general_module::Save(const settings_store& store) const -> void
    {
        store.WriteFloat("appearance", "dark_backing_opacity", this->dark_backing_opacity_);
        this->global_text_.Save(store);
        this->config_text_.Save(store);
        store.WriteBool("modules", "inventory", this->inventory_enabled_);
        store.WriteBool("modules", "party", this->party_enabled_);
        store.WriteBool("layout.grid", "visible", this->grid_.visible);
        store.WriteBool("layout.grid", "snap_grid", this->grid_.snap_grid);
        store.WriteBool("layout.grid", "snap_elements", this->grid_.snap_elements);
        store.WriteFloat("layout.grid", "amount", this->grid_.amount);
        store.WriteFloat("layout.grid", "spacing", this->grid_.spacing);
        store.WriteFloat("layout.grid", "opacity", this->grid_.opacity);
    }

    auto general_module::Option(const size_t index) -> module_option
    {
        if (index >= this->ConfigTextStart() && index < this->OptionCount())
            return this->config_text_.Option(index - this->ConfigTextStart());
        if (index >= this->GlobalTextStart() && index < this->ConfigTextStart())
            return this->global_text_.Option(index - this->GlobalTextStart());
        if (index >= this->ContributorsStart() && index < this->GlobalTextStart())
        {
            const auto& contributor = ui::xiui_contributors[index - this->ContributorsStart()];
            auto option = module_option::Action(contributor.name, contributor.url != nullptr ? contributor.url : "No linked GitHub account in upstream history.",
                contributor.url != nullptr ? "GitHub" : "No profile");
            option.action_icon = ui::icon_glyph::github;
            option.available = contributor.url != nullptr;
            return option;
        }
        if (index >= this->GridStart() && index < this->ContributorsStart())
        {
            switch (index - this->GridStart())
            {
            case 0: return module_option::Toggle("Show grid", "Screen-wide grid behind all XIUI elements in edit mode. Independent of snapping.", &this->grid_.visible);
            case 1: return module_option::Toggle("Snap to grid", "Catch nearby grid lines while moving an element. Works with the grid hidden.", &this->grid_.snap_grid);
            case 2: return module_option::Toggle("Snap to elements", "Align edges and centers with other visible previews and the screen.", &this->grid_.snap_elements);
            case 3: return module_option::Slider("Snap amount", "Distance in pixels for catching grid lines or another element. Smaller values give finer control.", &this->grid_.amount, 1, 20, 1);
            case 4: return module_option::Slider("Grid spacing", "Distance in pixels between grid lines; also sets the grid snapping positions.", &this->grid_.spacing, 4, 128, 1);
            case 5: return module_option::Slider("Grid opacity", "Visibility of the background grid. Alignment guides remain visible while snapping.", &this->grid_.opacity, 0, 1, 0.05f);
            }
        }
        if (index >= this->AttributionStart() && index < this->GridStart())
        {
            const auto& credit = ui::attributions[index - this->AttributionStart()];
            return module_option::Action(credit.project, credit.credit, this->copied_credit_ == index ? "Copied" : "Copy link");
        }
        if (index >= enabled_modules_option && index < this->AttributionStart())
        {
            auto& module = *this->modules_[index - enabled_modules_option];
            if (&module == this->party_module_)
            {
                auto result = module_option::Toggle("Party", "Individual list visibility is set in Party settings.", &this->party_enabled_);
                result.submodules = "Alliance 1, Alliance 2";
                return result;
            }
            if (&module == this->inventory_module_)
            {
                auto result = module_option::Toggle("Inventory", "Individual visibility is set in Inventory settings.", &this->inventory_enabled_);
                result.submodules = "Satchel, Locker, Safe, Storage, Wardrobes";
                return result;
            }
            const auto enabled = module.Option(0);
            return module_option::Toggle(module.DisplayName(), enabled.description, enabled.toggle);
        }
        if (index == dark_backing_opacity_option)
            return module_option::Slider("Dark backing opacity", "Dark fill behind XIUI windows, borders and themed bars.",
                &this->dark_backing_opacity_, 0.0f, 1.0f, 0.05f, module_option_group::appearance);
        if (index == window_theme_option)
        {
            static constexpr const char* types[]{"Type 1", "Type 2", "Type 3", "Type 4", "Type 5", "Type 6", "Type 7", "Type 8"};
            auto result = module_option::Choice("Window theme", "Changes Window Type for the whole game.", &this->window_type_, types, std::size(types), module_option_group::appearance);
            result.available = this->window_type_available_;
            if (!result.available) result.description = "Unavailable on this client.";
            return result;
        }
        return {};
    }

    auto general_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == window_theme_option) this->RequestWindowType(this->window_type_);
        else if (index >= this->GlobalTextStart() && index < this->ConfigTextStart())
        {
            const auto property = index - this->GlobalTextStart();
            this->global_text_.applied[property] = true;
            for (auto* module : this->text_modules_)
                module->VisitTextSettings([&](ui::text_style& style) { this->global_text_.Apply(property, style); });
        }
        else if (index >= this->ContributorsStart() && index < this->GlobalTextStart())
        {
            const auto& contributor = ui::xiui_contributors[index - this->ContributorsStart()];
            if (contributor.url != nullptr) this->open_link_(this->clipboard_owner_, contributor.url);
        }
        else if (index >= this->AttributionStart() && index < this->GridStart())
        {
            if (ui::CopyClipboardText(this->clipboard_owner_, ui::attributions[index - this->AttributionStart()].url)) this->copied_credit_ = index;
        }
        else if (index >= enabled_modules_option && index < this->AttributionStart() &&
            this->modules_[index - enabled_modules_option] != this->inventory_module_ && this->modules_[index - enabled_modules_option] != this->party_module_)
            this->modules_[index - enabled_modules_option]->OnOptionChanged(0);
    }

    auto general_module::OpenLink(const HWND owner, const char* url) -> bool
    {
        return reinterpret_cast<INT_PTR>(::ShellExecuteA(owner, "open", url, nullptr, nullptr, SW_SHOWNORMAL)) > 32;
    }

    auto general_module::Enabled(hud_module& module) -> bool
    {
        const auto option = module.Option(0);
        return option.toggle != nullptr && *option.toggle;
    }

    auto general_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"Appearance", "Window appearance and global HUD text defaults. Individual text settings can override global changes."},
            {"Enabled Modules", "Disabled modules leave the sidebar. Shared pages remain while any member is enabled."},
            {"Attribution", "Project credits and the original XIUI contributors. GitHub buttons open contributor profiles."},
            {"Grid", "Edit-mode grid and shared edge/center snapping for every movable HUD element. Existing positions stay in place until dragged."}};
        return sections;
    }

    auto general_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (section == grid_section) return index >= this->GridStart() && index < this->ContributorsStart();
        if (section == attribution_section) return (index >= this->ContributorsStart() && index < this->GlobalTextStart()) || (index >= this->AttributionStart() && index < this->GridStart());
        return section == appearance_section ? index < enabled_modules_option || index >= this->GlobalTextStart() :
            section == enabled_modules_section && index >= enabled_modules_option && index < this->AttributionStart();
    }

    auto general_module::UpdateWindowType(const std::optional<uint8_t> type) -> void
    {
        this->window_type_available_ = type && *type < 8;
        if (this->window_type_available_ && this->requested_window_type_ < 0) this->window_type_ = *type;
    }

    auto general_module::RequestWindowType(const int type) -> bool
    {
        if (!this->window_type_available_ || type < 0 || type > 7) return false;
        this->window_type_ = this->requested_window_type_ = type;
        return true;
    }

    auto general_module::TakeWindowTypeRequest(void) -> std::optional<uint8_t>
    {
        if (this->requested_window_type_ < 0) return std::nullopt;
        const auto type = static_cast<uint8_t>(this->requested_window_type_);
        this->requested_window_type_ = -1;
        return type;
    }
} // namespace xiui
