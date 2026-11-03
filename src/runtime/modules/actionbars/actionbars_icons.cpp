#include "actionbars_module.hpp"
#include "config/config_controls.hpp"
#include "resources/custom_action_icons.hpp"
#include "resources/action_icons.hpp"
#include "chat/chat_encoding.hpp"
#include "resources/custom_icon_library.hpp"
#include "localization/localization.hpp"

#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <charconv>

#pragma comment(lib, "shell32.lib")

namespace xiui
{
    namespace
    {
        constexpr float pitch = 52, tile = 48;
        auto Lower(std::string value) -> std::string
        {
            for (auto& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            return value;
        }
    }

    auto actionbars_module::BuildIconCatalog(void) -> void
    {
        const auto* gameResources = this->core_ != nullptr ? this->core_->GetResourceManager() : nullptr;
        if (!this->icon_catalog_.empty() && (gameResources == nullptr || this->icon_catalog_game_ready_)) { this->SyncCustomIconCatalog(); return; }
        this->icon_catalog_.clear();
        for (const auto& icon : resources::custom_action_icons)
            this->icon_catalog_.push_back({std::string{icon.key}, std::string{icon.label}, std::string{icon.category}, 3});
        this->icon_catalog_game_ready_ = gameResources != nullptr;
        const auto name = [](const char* const* names) {
            const auto* value = names[2] != nullptr ? names[2] : names[0];
            return value != nullptr ? FromGameText(value) : std::string{};
        };
        for (const auto& icon : resources::action_icons)
        {
            std::string label{icon.key}, category{"XIUI+ artwork"};
            const auto split = icon.key.find(':');
            uint32_t id{};
            if (gameResources != nullptr && split != std::string_view::npos)
            {
                const auto number = icon.key.substr(split + 1);
                const auto parsed = std::from_chars(number.data(), number.data() + number.size(), id);
                if (parsed.ec == std::errc{} && parsed.ptr == number.data() + number.size())
                {
                    if (icon.key.starts_with("spell:"))
                    {
                        if (const auto* spell = gameResources->GetSpellById(id)) { label = name(spell->Name); category = "XIUI+ spell"; }
                    }
                    else if (const auto* ability = gameResources->GetAbilityById(id))
                    { label = name(ability->Name); category = id < 256 ? "XIUI+ weapon skill" : "XIUI+ ability"; }
                }
            }
            this->icon_catalog_.push_back({std::string{icon.key}, label.empty() ? std::string{icon.key} : label, category, 2});
        }
        this->custom_icon_revision_ = UINT64_MAX;
        if (gameResources == nullptr) { this->SyncCustomIconCatalog(); return; }
        for (uint32_t id = 1; id < 2048; ++id)
            if (const auto* spell = gameResources->GetSpellById(id); spell != nullptr)
                if (auto label = name(spell->Name); !label.empty()) this->icon_catalog_.push_back({"game:spell:" + std::to_string(id), std::move(label), "Game spell", 1});
        for (uint32_t id = 256; id < 0x700; ++id)
            if (const auto* ability = gameResources->GetAbilityById(id); ability != nullptr && ability->Type != 4 && ability->Type != 20)
                if (auto label = name(ability->Name); !label.empty()) this->icon_catalog_.push_back({"game:ability:" + std::to_string(id), std::move(label), "Game ability / pet command", 1});
        for (uint32_t id = 1; id < 65536; ++id)
            if (const auto* item = gameResources->GetItemById(id); item != nullptr)
                if (auto label = name(item->Name); !label.empty()) this->icon_catalog_.push_back({"item:" + std::to_string(id), std::move(label), "Game item / equipment", 1});
        this->SyncCustomIconCatalog();
    }

    auto actionbars_module::SyncCustomIconCatalog(void) -> void
    {
        if (this->custom_icons_ == nullptr) return;
        const auto catalog = this->custom_icons_->Catalog();
        if (catalog.revision == this->custom_icon_revision_) return;
        std::string focused;
        if (this->focused_control_ >= icon_first && static_cast<size_t>(this->focused_control_ - icon_first) < this->icon_catalog_.size())
            focused = this->icon_catalog_[this->focused_control_ - icon_first].key;
        std::erase_if(this->icon_catalog_, [](const auto& icon) { return icon.source == 4; });
        for (const auto& icon : *catalog.entries)
            this->icon_catalog_.push_back({icon.key, icon.label, icon.category, 4});
        this->custom_icon_revision_ = catalog.revision;
        if (this->dialog_ != dialog::icons) return;
        const auto scroll = this->icon_scroll_;
        this->RebuildIconResults();
        if (!focused.empty())
        {
            this->focused_control_ = field_icon_query;
            for (const auto index : this->icon_results_)
                if (this->icon_catalog_[index].key == focused) { this->focused_control_ = icon_first + static_cast<int>(index); break; }
        }
        this->ScrollIcons(static_cast<int>(scroll));
    }

    auto actionbars_module::ActivateCustomIconControl(const int id) -> bool
    {
        if (id != command_icon_folder && id != command_icon_refresh) return false;
        if (this->custom_icons_ == nullptr) return true;
        if (id == command_icon_refresh) this->custom_icons_->Refresh();
        else
        {
            std::error_code error;
            const auto& directory = this->custom_icons_->Directory();
            std::filesystem::create_directories(directory, error);
            const auto owner = this->core_ && this->core_->GetProperties() ? this->core_->GetProperties()->GetFinalFantasyHwnd() : nullptr;
            if (error || reinterpret_cast<INT_PTR>(::ShellExecuteW(owner, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) <= 32)
                this->Notice("Could not open the custom icon folder.");
        }
        return true;
    }

    auto actionbars_module::OpenIconPicker(void) -> void
    {
        this->CommitField();
        if (this->custom_icons_ != nullptr)
        {
            const auto catalog = this->custom_icons_->Catalog();
            if (!catalog.scanned && !catalog.scanning) this->custom_icons_->Refresh();
        }
        this->BuildIconCatalog();
        const auto selected = this->executor_ != nullptr ? this->executor_->SuggestedIcon(this->core_, this->draft_) : this->draft_.icon;
        if (const auto icon = std::find_if(this->icon_catalog_.begin(), this->icon_catalog_.end(), [&](const auto& entry) { return entry.key == selected; });
            icon != this->icon_catalog_.end() && this->icon_source_ != 0 && icon->source != this->icon_source_) this->icon_source_ = icon->source;
        this->dialog_ = dialog::icons; this->icon_query_.clear(); this->RebuildIconResults();
        this->focused_control_ = this->icon_results_.empty() ? field_icon_query : icon_first + static_cast<int>(this->icon_results_.front());
        const auto& icons = this->icon_catalog_;
        for (const auto index : this->icon_results_)
            if (icons[index].key == selected) { this->focused_control_ = icon_first + static_cast<int>(index); break; }
        const auto layout = this->IconPickerLayout();
        const auto found = std::find(this->icon_results_.begin(), this->icon_results_.end(), static_cast<size_t>(this->focused_control_ - icon_first));
        if (found != this->icon_results_.end())
            this->ScrollIcons(static_cast<int>(static_cast<size_t>(found - this->icon_results_.begin()) / layout.columns));
    }

    auto actionbars_module::CloseIconPicker(void) -> void
    {
        this->input_.Cancel(); this->editing_field_ = 0;
        this->dialog_ = dialog::editor; this->focused_control_ = command_icon_picker;
    }

    auto actionbars_module::RebuildIconResults(void) -> void
    {
        this->icon_results_.clear(); this->icon_scroll_ = 0;
        std::istringstream query{Lower(this->icon_query_)};
        std::vector<std::string> terms;
        for (std::string term; query >> term;) terms.push_back(std::move(term));
        auto& icons = this->icon_catalog_;
        for (size_t index = 0; index < icons.size(); ++index)
        {
            auto& icon = icons[index];
            if (this->icon_source_ != 0 && icon.source != this->icon_source_) continue;
            if (icon.search.empty()) icon.search = Lower(icon.label + " " + icon.category + " " + icon.key);
            if (std::all_of(terms.begin(), terms.end(), [&](const auto& term) { return icon.search.find(term) != std::string::npos; }))
                this->icon_results_.push_back(index);
        }
        if (this->dialog_ == dialog::icons && this->focused_control_ >= icon_first &&
            std::find(this->icon_results_.begin(), this->icon_results_.end(), static_cast<size_t>(this->focused_control_ - icon_first)) == this->icon_results_.end())
            this->focused_control_ = field_icon_query;
    }

    auto actionbars_module::IconPickerLayout(void) const -> icon_picker_layout
    {
        const auto bounds = this->DialogBounds();
        icon_picker_layout result;
        const float tools = this->icon_source_ == 4 ? 32.0f : 0;
        result.search = {bounds.x + 16, bounds.y + 86, bounds.width - 32, 28};
        result.columns = static_cast<size_t>(std::max(1, static_cast<int>((bounds.width - 48 + 4) / pitch)));
        result.rows = static_cast<size_t>(std::max(1, static_cast<int>((bounds.height - 222 - tools + 4) / pitch)));
        result.rows = std::min(result.rows, std::max<size_t>(1, 128 / result.columns));
        result.list = {bounds.x + 16, bounds.y + 126 + tools, static_cast<float>(result.columns) * pitch - 4, static_cast<float>(result.rows) * pitch - 4};
        result.folder = {bounds.x + 16, bounds.y + 122, 136, 26};
        result.refresh = {bounds.x + 160, bounds.y + 122, 104, 26};
        result.scrollbar = {bounds.x + bounds.width - 24, result.list.y, 8, result.list.height};
        result.details = {bounds.x + 16, bounds.y + bounds.height - 84, bounds.width - 32, 28};
        result.defaults = {bounds.x + 16, bounds.y + bounds.height - 44, 192, 28};
        result.back = {bounds.x + bounds.width - 120, result.defaults.y, 104, 28};
        return result;
    }

    auto actionbars_module::IconControls(void) const -> std::vector<control>
    {
        const auto layout = this->IconPickerLayout();
        std::vector<control> result{{field_icon_query, layout.search, this->icon_query_, {}, control_kind::field}};
        constexpr const char* sources[]{"All", "Game", "XIUI+", "Vincerno", "Custom"};
        const auto bounds = this->DialogBounds(); const float width = (bounds.width - 56) / 5;
        for (int source = 0; source < 5; ++source)
            result.push_back({icon_source_first + source, {bounds.x + 16 + source * (width + 6), bounds.y + 50, width, 26}, sources[source], {}, control_kind::button,
                source != 4 || (this->custom_icons_ != nullptr && !this->custom_icons_->Directory().empty()), source == this->icon_source_});
        if (this->icon_source_ == 4)
        {
            const auto catalog = this->custom_icons_ != nullptr ? this->custom_icons_->Catalog() : resources::custom_icon_catalog{};
            result.push_back({command_icon_folder, layout.folder, "Open folder", "Add PNG, JPEG, BMP, GIF, TIFF, ICO or WebP images, then choose Refresh. Subfolders are supported.", control_kind::button});
            result.push_back({command_icon_refresh, layout.refresh, "Refresh", "Reload custom icons after adding, changing or removing image files.", control_kind::button, !catalog.scanning});
            if (this->icon_results_.empty())
            {
                const char* message = catalog.scanning ? "Scanning custom icons..." :
                    catalog.error == resources::custom_icon_scan_error::folder ? "Could not read the custom icon folder." :
                    !this->icon_query_.empty() ? "No matching icons." : "Add image files using Open folder, then Refresh.";
                result.push_back({label_first, {layout.list.x, layout.list.y, bounds.width - 48, 28}, message, {}, control_kind::label, false});
            }
        }
        const auto first = this->icon_scroll_ * layout.columns;
        const auto& icons = this->icon_catalog_;
        for (size_t offset = 0; offset < layout.rows * layout.columns && first + offset < this->icon_results_.size(); ++offset)
        {
            const auto index = this->icon_results_[first + offset]; const auto& icon = icons[index];
            const ui::rect cell{layout.list.x + static_cast<float>(offset % layout.columns) * pitch,
                layout.list.y + static_cast<float>(offset / layout.columns) * pitch, tile, tile};
            const auto category = icon.source == 4 && icon.category.empty() ? std::string{i18n::Text("Custom icons")} : icon.category;
            const bool failed = icon.source == 4 && this->custom_icons_ != nullptr && this->custom_icons_->State(icon.key) == resources::custom_icon_state::failed;
            result.push_back({icon_first + static_cast<int>(index), cell, std::string{icon.key}, failed ? std::string{i18n::Text("Could not load this image. Maximum size: 2048 x 2048 pixels and 8 MB.")} : std::string{icon.label} + "  /  " + category,
                control_kind::icon, true, icon.key == this->draft_.icon});
        }
        result.push_back({command_icon_default, layout.defaults, "Automatic (from action)", {}, control_kind::button, true, this->draft_.icon.empty()});
        result.push_back({command_icon_back, layout.back, "Back", {}, control_kind::button});
        return result;
    }

    auto actionbars_module::ScrollIcons(const int value) -> void
    {
        const auto layout = this->IconPickerLayout();
        const auto rows = (this->icon_results_.size() + layout.columns - 1) / layout.columns;
        this->icon_scroll_ = static_cast<size_t>(std::clamp(value, 0, static_cast<int>(rows > layout.rows ? rows - layout.rows : 0)));
        if (this->focused_control_ < icon_first || this->icon_results_.empty()) return;
        const auto found = std::find(this->icon_results_.begin(), this->icon_results_.end(), static_cast<size_t>(this->focused_control_ - icon_first));
        if (found == this->icon_results_.end()) { this->focused_control_ = field_icon_query; return; }
        const auto index = static_cast<size_t>(found - this->icon_results_.begin());
        const auto row = std::clamp(index / layout.columns, this->icon_scroll_, this->icon_scroll_ + layout.rows - 1);
        this->focused_control_ = icon_first + static_cast<int>(this->icon_results_[std::min(row * layout.columns + index % layout.columns, this->icon_results_.size() - 1)]);
    }

    auto actionbars_module::MoveIconControl(const int dx, const int dy) -> bool
    {
        if (this->dialog_ != dialog::icons || this->focused_control_ < icon_first) return false;
        const auto found = std::find(this->icon_results_.begin(), this->icon_results_.end(), static_cast<size_t>(this->focused_control_ - icon_first));
        if (found == this->icon_results_.end()) return false;
        const auto layout = this->IconPickerLayout(); const int columns = static_cast<int>(layout.columns);
        const int current = static_cast<int>(found - this->icon_results_.begin()), next = current + (dy ? dy * columns : dx);
        if (next < 0 || static_cast<size_t>(next) >= this->icon_results_.size() || (!dy && next / columns != current / columns)) return false;
        this->focused_control_ = icon_first + static_cast<int>(this->icon_results_[next]);
        const auto row = static_cast<size_t>(next) / layout.columns;
        if (row < this->icon_scroll_) this->ScrollIcons(static_cast<int>(row));
        else if (row >= this->icon_scroll_ + layout.rows) this->ScrollIcons(static_cast<int>(row - layout.rows + 1));
        return true;
    }

    auto actionbars_module::SelectIcon(const size_t index) -> void
    {
        if (index >= this->icon_catalog_.size()) return;
        const auto& icon = this->icon_catalog_[index];
        if (icon.source == 4 && this->custom_icons_ != nullptr)
        {
            const auto state = this->custom_icons_->State(icon.key);
            if (state == resources::custom_icon_state::failed)
            { this->Notice("Could not load this image. Maximum size: 2048 x 2048 pixels and 8 MB."); return; }
            if (state != resources::custom_icon_state::ready)
            { this->custom_icons_->Request(icon.key); this->Notice("The icon is still loading. Try again in a moment."); return; }
        }
        this->draft_.icon = this->icon_catalog_[index].key;
        this->CloseIconPicker();
    }

    auto actionbars_module::DrawIconPickerDetails(const module_render_context& context) const -> void
    {
        const auto layout = this->IconPickerLayout();
        std::string text = this->icon_results_.empty() ? i18n::Text("No matching icons.") : i18n::Format("%zu icons", this->icon_results_.size());
        if (this->icon_source_ == 4 && this->custom_icons_ != nullptr)
        {
            const auto catalog = this->custom_icons_->Catalog();
            if (catalog.scanning) text = i18n::Text("Scanning custom icons...");
            else if (catalog.error == resources::custom_icon_scan_error::limit) text = i18n::Text("Custom icon limit reached. Keep up to 4096 image files.");
        }
        const auto controls = this->IconControls();
        const auto focus = std::find_if(controls.begin(), controls.end(), [&](const auto& control) {
            if (control.kind != control_kind::icon) return false;
            return this->navigation_focus_ ? control.id == this->focused_control_ :
                this->hover_x_ >= control.bounds.x && this->hover_x_ < control.bounds.x + control.bounds.width &&
                this->hover_y_ >= control.bounds.y && this->hover_y_ < control.bounds.y + control.bounds.height;
        });
        if (focus != controls.end()) text = focus->hint;
        ui::config_controls renderer{context.Design()};
        if (this->config_text_) renderer.SetTextSettings(*this->config_text_);
        auto style = context.Design().GetTheme().caption_text; style.scale = 0.72f;
        const auto clip = context.Design().Clip(layout.details);
        renderer.Text(text.c_str(), layout.details, style);
    }
}
