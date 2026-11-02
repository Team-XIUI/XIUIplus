#include "satchel_module.hpp"
#include "ui/game_icon_cache.hpp"
#include "ui/item_slot_frame.hpp"
#include "ui/text_fit.hpp"

#include "satchel_source.hpp"
#include "satchel_cache.hpp"
#include "satchel_actions.hpp"
#include "storage_slips.hpp"
#include "chat/chat_encoding.hpp"
#include "config/config_controls.hpp"
#include "modules/equipment/equipment_source.hpp"
#include "modules/gil/gil_module.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <numeric>

namespace xiui
{
    namespace
    {
        auto Contains(const ui::rect& r, const float x, const float y) -> bool
        { return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height; }
        auto Intersect(const ui::rect& a, const ui::rect& b) -> ui::rect
        {
            const auto x = std::max(a.x, b.x), y = std::max(a.y, b.y);
            return {x, y, std::max(0.0f, std::min(a.x + a.width, b.x + b.width) - x),
                std::max(0.0f, std::min(a.y + a.height, b.y + b.height) - y)};
        }
        auto Lower(std::string value) -> std::string
        { for (auto& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return value; }
        auto Number(const std::string& text, uint32_t& value) -> bool
        {
            if (text.empty()) return false;
            const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
            return result.ec == std::errc{} && result.ptr == text.data() + text.size();
        }
        auto Observed(const uint64_t value) -> std::string
        {
            if (value == 0) return "Last observed: unavailable";
            const auto timestamp = static_cast<time_t>(value > 100000000000ULL ? value / 1000 : value);
            tm time{}; char text[64]{};
            if (localtime_s(&time, &timestamp) == 0) std::strftime(text, sizeof(text), "Last observed: %b %d, %Y %H:%M", &time);
            return text;
        }
        constexpr const char* profile_sections[]{"text.satchel.title", "text.satchel.caption", "text.satchel.quantity", "text.satchel.tooltip"};
        constexpr const char* empty_slot_key = "empty";
        constexpr const char* sort_names[]{"Custom order", "Name", "Item ID", "Quantity"};
        constexpr size_t picker_rows = 8;
        constexpr float item_menu_width = 252, item_menu_header = 30, item_menu_row = 24;
        constexpr float sidebar_width = 136, sidebar_gap = 12, bag_pitch = 28, scroll_gutter = 14;

        auto FitCells(const float available, const float cell, const float spacing, const size_t previous) -> float
        {
            const auto compactGap = std::min(spacing, 1.0f);
            const auto normal = std::floor((available + spacing) / (cell + spacing));
            const auto compact = std::floor((available + compactGap) / (cell + compactGap));
            return std::clamp(std::max(normal, std::min(static_cast<float>(previous), compact)), 1.0f, 128.0f);
        }

        auto CellGap(const float available, const float cell, const size_t count, const float spacing) -> float
        {
            return count < 2 ? 0 : std::clamp((available - static_cast<float>(count) * cell) / static_cast<float>(count - 1), 0.0f, spacing);
        }

        auto Augments(const satchel_item& item) -> std::vector<std::string>
        {
            std::vector<std::string> result;
            if ((item.extra[0] != 2 && item.extra[0] != 3) || (item.extra[1] & 0xA0) != 0) return result;
            constexpr const char* basic[]{"Accuracy", "Accuracy", "Attack", "Attack", "Ranged accuracy", "Ranged accuracy",
                "Ranged attack", "Ranged attack", "Evasion", "Evasion", "DEF", "DEF", "Magic accuracy", "Magic accuracy", "Magic evasion", "Magic evasion", "Enmity", "Enmity"};
            constexpr const char* stats[]{"STR", "DEX", "VIT", "AGI", "INT", "MND", "CHR"};
            const size_t end = (item.extra[1] & 0x40) != 0 ? 10 : 12;
            for (size_t index = 2; index < end; index += 2)
            {
                const auto id = item.extra[index] + (item.extra[index + 1] & 7) * 256;
                int amount = (item.extra[index + 1] >> 3) + 1;
                const char* name{};
                if (id >= 1 && id <= 16)
                {
                    const int type = (id - 1) % 8; name = id <= 8 ? "HP" : "MP";
                    amount += (type % 4) * 32; if (type >= 4) amount = -amount;
                }
                else if (id >= 0x17 && id <= 0x28)
                { name = basic[id - 0x17]; if ((id - 0x17) % 2) amount = -amount; }
                else if (id >= 0x200 && id <= 0x20D)
                { name = stats[(id - 0x200) % 7]; if (id >= 0x207) amount = -amount; }
                if (name) result.push_back(std::string{name} + (amount > 0 ? "+" : "") + std::to_string(amount));
            }
            return result;
        }
    }

    auto satchel_module::Initialize(IAshitaCore* core, satchel_source* source, satchel_cache* cache, satchel_actions* actions) -> void
    { this->core_ = core; this->source_ = source; this->cache_ = cache; this->actions_ = actions; this->rebuild_ = true; }

    auto satchel_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.caption_text},
            module_text_profile{theme.meter_text}, module_text_profile{theme.body_text}};
        this->text_[0].style.scale = 0.85f;
        this->text_[1].style.scale = 0.65f;
        this->text_[2].style.scale = 0.65f;
        this->text_[2].style.alignment = ui::text_alignment::right;
        this->text_[3].style.scale = 0.8f;
    }

    auto satchel_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        auto* core = this->core_; auto* source = this->source_; auto* cache = this->cache_; auto* actions = this->actions_;
        auto* sounds = this->sounds_; const auto focus = this->focus_style_; const auto* configText = this->config_text_;
        this->SuspendInput();
        *this = {};
        this->Initialize(core, source, cache, actions); this->SetSounds(sounds); this->SetFocusStyle(focus);
        this->SetConfigText(configText);
        this->state_.bounds = bounds;
        this->ApplyTextDefaults(theme);
    }

    auto satchel_module::Load(const settings_store& store) -> void
    {
        this->SuspendInput();
        this->state_.enabled = store.ReadBool("satchel", "enabled", true);
        this->opened_ = store.ReadBool("session.satchel", "open", false);
        this->minimized_ = store.ReadBool("session.satchel", "minimized", false);
        this->region_ = this->minimized_ ? region::minimize : region::grid;
        this->state_.bounds = store.ReadRect("layout.satchel", this->state_.bounds);
        this->width_ = static_cast<float>(store.ReadInt("layout.satchel", "width", 0));
        this->height_ = static_cast<float>(store.ReadInt("layout.satchel", "height", 0));
        if (this->minimized_ && this->height_ < this->state_.minimum_height) this->height_ = 0;
        this->override_satchel_ = store.ReadBool("satchel", "override_satchel", false);
        this->empty_slots_ = store.ReadBool("satchel", "empty_slots", true);
        this->all_bags_search_ = store.ReadBool("satchel", "all_bags_search", true);
        this->tooltips_ = store.ReadBool("satchel", "tooltips", true);
        this->restricted_ = store.ReadBool("satchel", "read_only", false);
        this->autosort_ = store.ReadBool("satchel", "autosort", false);
        this->background_ = store.ReadBool("satchel", "background", true);
        this->frames_ = store.ReadBool("satchel", "slot_frames", true);
        this->position_locked_ = store.ReadBool("satchel", "lock_position", false);
        const auto number = [&](const char* key, const float value, const float minimum, const float maximum) {
            const auto result = store.ReadFloat("satchel", key, value);
            return std::clamp(std::isfinite(result) ? result : value, minimum, maximum);
        };
        this->columns_ = number("columns", 10, 1, 128); this->rows_ = number("rows", 5, 1, 128);
        this->icon_size_ = number("icon_size", 32, 24, 64); this->spacing_ = number("spacing", 5, 0, 16);
        this->slot_opacity_ = number("slot_background_opacity", 0.8f, 0, 1);
        if (store.ReadInt("satchel", "slot_background_version", 0) < 1 && std::abs(this->slot_opacity_ - 0.55f) < 0.002f)
            this->slot_opacity_ = 0.8f;
        this->container_.opacity = number("opacity", 0.35f, 0.1f, 1); this->container_.padding = number("padding", 8, 0, 24);
        if (store.ReadInt("satchel", "container_opacity_version", 0) < 1 && std::abs(this->container_.opacity - 1.0f) < 0.002f)
            this->container_.opacity = 0.35f;
        this->sort_ = std::clamp(store.ReadInt("satchel", "display_sort", 0), 0, 3);
        this->selected_text_ = std::clamp(store.ReadInt("satchel", "selected_text", 0), 0, 3);
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, profile_sections[i], this->text_[i]);
        this->rebuild_ = true;
    }

    auto satchel_module::Save(const settings_store& store) const -> void
    {
        auto bounds = this->state_.bounds;
        if (this->width_ > 0) bounds.width = this->width_;
        if (this->height_ > 0) bounds.height = this->height_;
        store.WriteBool("satchel", "enabled", this->state_.enabled); store.WriteRect("layout.satchel", bounds);
        store.WriteBool("session.satchel", "open", this->opened_);
        store.WriteBool("session.satchel", "minimized", this->minimized_);
        store.WriteBool("satchel", "override_satchel", this->override_satchel_); store.WriteBool("satchel", "empty_slots", this->empty_slots_);
        store.WriteBool("satchel", "all_bags_search", this->all_bags_search_); store.WriteBool("satchel", "tooltips", this->tooltips_);
        store.WriteBool("satchel", "read_only", this->restricted_); store.WriteBool("satchel", "autosort", this->autosort_);
        store.WriteBool("satchel", "background", this->background_); store.WriteBool("satchel", "slot_frames", this->frames_);
        store.WriteBool("satchel", "lock_position", this->position_locked_);
        store.WriteFloat("satchel", "slot_background_opacity", this->slot_opacity_);
        store.WriteInt("satchel", "slot_background_version", 1);
        store.WriteFloat("satchel", "columns", this->columns_); store.WriteFloat("satchel", "rows", this->rows_);
        store.WriteFloat("satchel", "icon_size", this->icon_size_); store.WriteFloat("satchel", "spacing", this->spacing_);
        store.WriteFloat("satchel", "opacity", this->container_.opacity); store.WriteFloat("satchel", "padding", this->container_.padding);
        store.WriteInt("satchel", "container_opacity_version", 1);
        store.WriteInt("satchel", "display_sort", this->sort_); store.WriteInt("satchel", "selected_text", this->selected_text_);
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, profile_sections[i], this->text_[i]);
    }

    auto satchel_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Open with /xiuiplus satchel. Browse current bags, saved characters and Porter Moogle slips.", "general"},
            {"Layout", "Grid dimensions and display order. Custom positions are saved locally for each character and view.", "layout"},
            {"Appearance", "Native item artwork and the shared game window.", "appearance"},
            {"Text", "Title, captions, quantities and item details retain their configured effects.", "text"}};
        return sections;
    }

    auto satchel_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (index == lock_position) return section == 1;
        if (index == slot_opacity) return section == 2;
        if (index <= autosort) return section == 0;
        if (index <= sort) return section == 1;
        if (index <= padding) return section == 2;
        return section == 3;
    }

    auto satchel_module::ContentOption(const size_t index) -> module_option
    {
        static constexpr const char* profiles[]{"Title", "Captions", "Stack quantities", "Item details"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Make the Satchel browser available through its command and focus entry.", &this->state_.enabled).Ordered(-50);
        case override_command: return module_option::Toggle("Override /satchel", "Use /satchel for this browser. Off preserves the native command.", &this->override_satchel_);
        case empty_slots: return module_option::Toggle("Empty slots", "Show unused slots in the selected bag. Empty slots are skipped during controller browsing.", &this->empty_slots_);
        case all_bags_search: return module_option::Toggle("Search all bags", "Search every settled bag while text is entered, retaining bag names in details.", &this->all_bags_search_);
        case tooltips: return module_option::Toggle("Item tooltips", "Show descriptions, equipment requirements and supported instance details on hover or focus.", &this->tooltips_);
        case restricted: return module_option::Toggle("Read-only / restricted server", "Disable item actions for servers that restrict inventory packets. Cached characters and slips are always read-only.", &this->restricted_);
        case autosort: return module_option::Toggle("Autosort after transfers", "Sort the destination after a Satchel move or split is acknowledged. Sorting does not repeat from its own inventory updates.", &this->autosort_).VisibleWhen(!this->restricted_);
        case columns: return module_option::Slider("Columns", "Set the window width for this many item columns. Dragging the resize grip adjusts it freely.", &this->columns_, 1, 128, 1).Ordered(110, true);
        case rows: return module_option::Slider("Rows", "Set the window height for this many item rows. Dragging the resize grip adjusts it freely.", &this->rows_, 1, 128, 1).Ordered(111);
        case icon_size: return module_option::Slider("Icon size", "Native item artwork size in pixels.", &this->icon_size_, 24, 64, 1).Ordered(120);
        case spacing: return module_option::Slider("Slot spacing", "Pixels between item cells.", &this->spacing_, 0, 16, 1).Ordered(300);
        case sort: return module_option::Choice("Display order", "Change the presentation without sorting game inventory. Drag between item cells in Custom order to rearrange.", &this->sort_, sort_names, 4).Ordered(-30);
        case background: return module_option::Toggle("Container", "Use the shared native game window around the browser.", &this->background_, module_option_group::appearance).Ordered(-10);
        case frames: return module_option::Toggle("Slot frames", "Draw flat backgrounds and thin rounded outlines around item cells. Native item quality borders remain intact.", &this->frames_, module_option_group::appearance).Ordered(470);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the outer container and slot outlines.", &this->container_.opacity, 0.1f, 1, 0.05f, module_option_group::appearance).Ordered(450).VisibleWhen(this->background_ || this->frames_);
        case lock_position: return module_option::Toggle("Lock position", "Prevent moving or resizing Satchel independently of HUD Edit Mode. Browsing and item actions remain available.", &this->position_locked_).Ordered(-50);
        case slot_opacity: return module_option::Slider("Slot background opacity", "Background opacity for filled and empty item cells, independently of the outer window and item artwork.", &this->slot_opacity_, 0, 1, 0.05f, module_option_group::appearance).Ordered(471).VisibleWhen(this->frames_);
        case padding: return module_option::Slider("Container padding", "Space inside the outer frame.", &this->container_.padding, 0, 24, 1, module_option_group::appearance).Ordered(402);
        case selected_text: return module_option::Element("Selected text", "Configure each text role independently.", &this->selected_text_, profiles, 4, module_option_group::text);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]).VisibleWhen(this->selected_text_ != 3 || this->tooltips_);
        }
    }

    auto satchel_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index == lock_position && this->position_locked_)
        {
            const bool pointer = this->pointer_down_ && this->window_action_ != window_action::none;
            this->FinishWindow(false);
            if (pointer) { this->pointer_down_ = false; this->discard_pointer_release_ = true; }
            if (!this->RegionAvailable(this->region_)) this->region_ = this->minimized_ ? region::minimize : region::grid;
        }
        if (index == columns) this->width_ = 0;
        if (index == rows) this->height_ = 0;
        this->UpdateSize(); this->rebuild_ = true;
        if (this->restricted_) { this->CancelDialog(); if (this->actions_) this->actions_->Cancel(); }
    }

    auto satchel_module::Open(const bool opened) -> void
    {
        if (this->opened_ != opened) this->settings_changed_ = true;
        this->opened_ = opened;
        if (!opened) this->SuspendInput();
        else { this->rebuild_ = true; if (this->sounds_) this->sounds_->MenuOpened(::GetTickCount64()); }
    }

    auto satchel_module::SuspendInput(void) -> void
    {
        this->FinishWindow(false);
        this->ReleaseBrowseFocus();
        this->pointer_down_ = this->dragging_ = this->discard_pointer_release_ = false;
        this->dialog_ = dialog::none; this->editor_.Cancel(); this->picks_.clear(); this->pending_autosort_bag_ = -1;
        this->pressed_pick_.reset();
        this->hover_x_ = this->hover_y_ = -1;
        this->help_open_ = false;
        this->navigation_focus_ = false;
        this->scrolling_ = scroll_area::none;
        if (this->actions_) this->actions_->Cancel();
    }

    auto satchel_module::BeginControllerBrowse(void) -> void
    { this->Open(true); this->controller_ = this->navigation_focus_ = true; this->region_ = this->minimized_ ? region::minimize : region::grid; this->Rebuild(); }
    auto satchel_module::EndControllerBrowse(void) -> void
    { if (this->controller_) this->SuspendInput(); }
    auto satchel_module::PlaySelect(void) -> void { if (this->sounds_) this->sounds_->Select(this->now_); }

    auto satchel_module::Snapshot(void) const -> const satchel_snapshot*
    {
        if (this->character_.empty()) return this->source_ ? &this->source_->Snapshot() : nullptr;
        if (this->cache_) for (const auto& character : this->cache_->Characters()) if (character.identity == this->character_) return &character;
        return nullptr;
    }
    auto satchel_module::Live(void) const -> bool { return this->character_.empty() && this->slip_ < 0; }
    auto satchel_module::ReadOnly(void) const -> bool
    { const auto* snapshot = this->Snapshot(); return !this->Live() || this->restricted_ || !snapshot || !snapshot->ready || snapshot->read_only; }

    auto satchel_module::Name(const uint16_t id) -> const std::string&
    {
        if (const auto found = this->names_.find(id); found != this->names_.end()) return found->second;
        const auto* resources = this->core_ ? this->core_->GetResourceManager() : nullptr;
        const auto* item = resources ? resources->GetItemById(id) : nullptr;
        const char* name = item ? (item->Name[2] ? item->Name[2] : item->Name[0]) : nullptr;
        return this->names_.emplace(id, name && *name ? FromGameText(name) : "Item #" + std::to_string(id)).first->second;
    }
    auto satchel_module::ItemKey(const satchel_item& item) const -> std::string
    {
        if (this->slip_ >= 0) return "slip:" + std::to_string(this->slip_) + ":" + std::to_string(item.id);
        uint64_t fingerprint = 14695981039346656037ULL;
        for (const auto byte : item.extra) { fingerprint ^= byte; fingerprint *= 1099511628211ULL; }
        size_t occurrence{};
        if (const auto* snapshot = this->Snapshot(); snapshot && item.bag < snapshot->bags.size())
            for (const auto& other : snapshot->bags[item.bag].items)
                if (other.id == item.id && other.extra == item.extra && other.slot < item.slot) ++occurrence;
        char extra[24]{}; std::snprintf(extra, sizeof(extra), "%016llx", static_cast<unsigned long long>(fingerprint));
        return std::to_string(item.bag) + ":" + std::to_string(item.id) + ":" + extra + ":" + std::to_string(occurrence);
    }

    auto satchel_module::Columns(void) const -> size_t { return static_cast<size_t>(std::clamp(std::round(this->columns_), 1.0f, 128.0f)); }
    auto satchel_module::Rows(void) const -> size_t { return static_cast<size_t>(std::clamp(std::round(this->rows_), 1.0f, 128.0f)); }
    auto satchel_module::PageSize(void) const -> size_t { return this->Columns() * this->Rows(); }
    auto satchel_module::PageCount(void) const -> size_t { return std::max<size_t>(1, (this->cells_.size() + this->PageSize() - 1) / this->PageSize()); }
    auto satchel_module::CurrentCell(void) const -> const cell*
    { return this->focus_cell_ < this->cells_.size() && !this->cells_[this->focus_cell_].empty ? &this->cells_[this->focus_cell_] : nullptr; }

    auto satchel_module::ReconcileFocus(const std::optional<std::string>& previous) -> void
    {
        if (previous)
            for (size_t i = 0; i < this->cells_.size(); ++i)
                if (this->cells_[i].key == *previous) { this->focus_cell_ = i; break; }
        if (this->cells_.empty()) this->focus_cell_ = this->page_ = 0;
        else
        {
            this->focus_cell_ = std::min(this->focus_cell_, this->cells_.size() - 1);
            if (this->cells_[this->focus_cell_].empty)
                for (size_t i = 0; i < this->cells_.size(); ++i) if (!this->cells_[i].empty) { this->focus_cell_ = i; break; }
            this->page_ = this->controller_ ? this->focus_cell_ / this->PageSize() : std::min(this->page_, this->PageCount() - 1);
        }
    }

    auto satchel_module::Rebuild(void) -> void
    {
        const auto* snapshot = this->Snapshot();
        const auto previous = this->CurrentCell() ? std::optional{this->CurrentCell()->key} : std::nullopt;
        this->cells_.clear(); this->owned_slips_.clear();
        if (!snapshot) { this->ReconcileFocus(previous); return; }
        this->view_identity_ = snapshot->identity;
        for (const auto bag : satchel_bag_order)
            for (const auto& item : snapshot->bags[bag].items)
                if (!SatchelSlipCatalog(item.id).empty() && std::none_of(this->owned_slips_.begin(), this->owned_slips_.end(),
                    [&](const auto& other) { return other.id == item.id; })) this->owned_slips_.push_back(item);
        const auto query = Lower(this->query_);
        const auto append = [&](const satchel_item& item) {
            if (query.empty() || Lower(this->Name(item.id)).find(query) != std::string::npos || std::to_string(item.id).find(query) != std::string::npos)
                this->cells_.push_back({item, false, this->ItemKey(item)});
        };
        if (this->slip_ >= 0)
        {
            const auto slipId = static_cast<uint16_t>(29312 + this->slip_);
            for (const auto& slip : this->owned_slips_) if (slip.id == slipId)
                for (const auto id : SatchelDecodeSlip(slip.id, slip.extra))
                { auto item = slip; item.id = id; item.count = 1; item.read_only = true; item.equipped = false; item.flags = item.price = 0; item.extra = {}; append(item); }
        }
        else
        {
            const bool all = this->bag_ < 0 || (!query.empty() && this->all_bags_search_);
            for (const auto bag : satchel_bag_order)
                if (all || bag == this->bag_)
                {
                    for (const auto& item : snapshot->bags[bag].items) append(item);
                    if (!all && query.empty() && this->empty_slots_)
                        for (size_t slot = snapshot->bags[bag].items.size(); slot < snapshot->bags[bag].capacity; ++slot)
                            this->cells_.push_back({{}, true});
                }
        }
        const auto occupied = std::stable_partition(this->cells_.begin(), this->cells_.end(), [](const auto& value) { return !value.empty; });
        if (this->sort_ == 0 && this->cache_)
        {
            const auto order = this->cache_->ReadOrder(snapshot->identity, this->slip_ >= 0 ? 100 + this->slip_ : this->bag_);
            std::unordered_map<std::string, size_t> positions;
            for (size_t i = 0; i < order.size(); ++i) positions.emplace(order[i], i);
            if (this->empty_slots_ && this->bag_ >= 0 && this->slip_ < 0 && query.empty() &&
                std::find(order.begin(), order.end(), empty_slot_key) != order.end())
            {
                std::vector<cell> arranged(this->cells_.size(), cell{{}, true, {}}), remaining;
                for (auto& value : this->cells_)
                {
                    if (value.empty) continue;
                    const auto found = positions.find(value.key);
                    if (found != positions.end() && found->second < arranged.size() && arranged[found->second].empty)
                        arranged[found->second] = std::move(value);
                    else remaining.push_back(std::move(value));
                }
                size_t next{};
                for (auto& value : remaining)
                {
                    while (next < arranged.size() && !arranged[next].empty) ++next;
                    if (next < arranged.size()) arranged[next++] = std::move(value);
                }
                this->cells_ = std::move(arranged);
            }
            else
                std::stable_sort(this->cells_.begin(), occupied, [&](const auto& a, const auto& b) {
                    const auto left = positions.find(a.key), right = positions.find(b.key);
                    return (left == positions.end() ? order.size() : left->second) < (right == positions.end() ? order.size() : right->second);
                });
        }
        else if (this->sort_ == 1) std::stable_sort(this->cells_.begin(), occupied, [&](const auto& a, const auto& b) { return Lower(this->Name(a.item.id)) < Lower(this->Name(b.item.id)); });
        else if (this->sort_ == 2) std::stable_sort(this->cells_.begin(), occupied, [](const auto& a, const auto& b) { return a.item.id < b.item.id; });
        else if (this->sort_ == 3) std::stable_sort(this->cells_.begin(), occupied, [](const auto& a, const auto& b) { return a.item.count > b.item.count; });
        this->ReconcileFocus(previous);
        this->last_generation_ = snapshot->generation; this->last_bag_ = this->bag_; this->last_slip_ = this->slip_;
        this->last_sort_ = this->sort_; this->last_character_ = this->character_; this->last_query_ = this->query_; this->rebuild_ = false;
    }

    auto satchel_module::Update(const uint64_t now) -> void
    {
        this->now_ = now;
        const auto* snapshot = this->Snapshot();
        if (this->rebuild_ || (snapshot && snapshot->generation != this->last_generation_) || this->last_bag_ != this->bag_ ||
            this->last_slip_ != this->slip_ || this->last_sort_ != this->sort_ || this->last_character_ != this->character_ || this->last_query_ != this->query_) this->Rebuild();
        if (this->pending_autosort_bag_ >= 0 && this->actions_ && !this->actions_->Busy() && this->source_)
        {
            const auto bag = this->pending_autosort_bag_; this->pending_autosort_bag_ = -1;
            if (this->autosort_ && this->opened_ && !this->ReadOnly() && this->actions_->Outcome() == satchel_action_outcome::observed)
            {
                satchel_action_request request{}; request.action = satchel_action::sort; request.identity = this->source_->Snapshot().identity; request.item.bag = static_cast<uint8_t>(bag);
                this->actions_->Submit(this->core_, this->source_->Snapshot(), request, now);
            }
        }
    }

    auto satchel_module::ChangeBag(const int bag) -> void
    { this->bag_ = bag; this->slip_ = -1; this->page_ = this->focus_cell_ = 0; this->rebuild_ = true; this->PlaySelect(); this->Rebuild(); this->EnsureSidebarFocus(); }
    auto satchel_module::VisibleBagTabs(void) const -> std::vector<size_t>
    {
        std::vector<size_t> result{0};
        const auto* snapshot = this->Snapshot();
        for (size_t i = 0; i < satchel_bag_order.size(); ++i)
        {
            const auto bag = satchel_bag_order[i];
            if ((snapshot && snapshot->bags[bag].capacity > 0 && (bag != 3 || !snapshot->bags[bag].items.empty())) ||
                (bag == 0 && (!snapshot || snapshot->bags[0].capacity == 0))) result.push_back(i + 1);
        }
        return result;
    }
    auto satchel_module::MoveBag(const int direction) -> void
    {
        const auto tabs = this->VisibleBagTabs();
        size_t current{};
        for (size_t i = 0; i < tabs.size(); ++i)
            if (tabs[i] != 0 && satchel_bag_order[tabs[i] - 1] == this->bag_) current = i;
        const auto next = static_cast<size_t>((static_cast<int>(current) + direction + static_cast<int>(tabs.size())) % static_cast<int>(tabs.size()));
        this->ChangeBag(tabs[next] == 0 ? -1 : satchel_bag_order[tabs[next] - 1]);
    }
    auto satchel_module::Page(const int direction) -> void
    {
        if (direction < 0 && this->page_ > 0) --this->page_;
        if (direction > 0 && this->page_ + 1 < this->PageCount()) ++this->page_;
        this->focus_cell_ = this->page_ * this->PageSize();
        this->ReconcileFocus({});
    }

    auto satchel_module::Inner(void) const -> ui::rect
    { const auto pad = this->container_.padding + 5; return {this->state_.bounds.x + pad, this->state_.bounds.y + pad, this->state_.bounds.width - pad * 2, this->state_.bounds.height - pad * 2}; }
    auto satchel_module::SidebarBounds(void) const -> ui::rect { const auto b = this->Inner(); return this->minimized_ ? ui::rect{} : ui::rect{b.x, b.y + 68, sidebar_width, std::max(0.0f, b.height - 120)}; }
    auto satchel_module::GridBounds(void) const -> ui::rect { const auto b = this->Inner(); return this->minimized_ ? ui::rect{} : ui::rect{b.x + sidebar_width + sidebar_gap, b.y + 68, std::max(0.0f, b.width - sidebar_width - sidebar_gap - (this->PageCount() > 1 ? scroll_gutter : 0)), std::max(0.0f, b.height - 120)}; }
    auto satchel_module::TitleBounds(void) const -> ui::rect { const auto b = this->Inner(); return {b.x, b.y, b.width - 126, 24}; }
    auto satchel_module::MinimizeBounds(void) const -> ui::rect { const auto b = this->Inner(); return {b.x + b.width - 58, b.y, 24, 24}; }
    auto satchel_module::ResizeBounds(void) const -> ui::rect { const auto b = this->state_.bounds; return this->minimized_ ? ui::rect{} : ui::rect{b.x + b.width - 20, b.y + b.height - 20, 18, 18}; }
    auto satchel_module::CloseBounds(void) const -> ui::rect { const auto b = this->Inner(); return {b.x + b.width - 28, b.y, 28, 24}; }
    auto satchel_module::HelpBounds(void) const -> ui::rect { const auto b = this->Inner(); return {b.x + b.width - 88, b.y, 24, 24}; }
    auto satchel_module::CharacterBounds(void) const -> ui::rect { const auto b = this->Inner(); return {b.x + b.width - 118, b.y, 24, 24}; }
    auto satchel_module::SlipBounds(void) const -> ui::rect { const auto b = this->SidebarBounds(); return {b.x, b.y + this->VisibleBagTabs().size() * bag_pitch + 4 - this->sidebar_scroll_, b.width - (this->SidebarScrollbar().Visible() ? scroll_gutter : 0), 26}; }
    auto satchel_module::SearchBounds(void) const -> ui::rect { const auto b = this->Inner(); return this->minimized_ ? ui::rect{} : ui::rect{b.x, b.y + 32, b.width, 28}; }
    auto satchel_module::BagBounds(const size_t index) const -> ui::rect
    {
        const auto tabs = this->VisibleBagTabs();
        const auto found = std::find(tabs.begin(), tabs.end(), index);
        if (found == tabs.end()) return {};
        const auto b = this->SidebarBounds();
        return {b.x, b.y + static_cast<float>(found - tabs.begin()) * bag_pitch - this->sidebar_scroll_, b.width - (this->SidebarScrollbar().Visible() ? scroll_gutter : 0), 26};
    }
    auto satchel_module::SidebarScrollbar(void) const -> ui::config_scrollbar
    {
        if (this->minimized_) return {};
        const auto b = this->SidebarBounds();
        return {{b.x + b.width - 10, b.y, 10, b.height}, static_cast<int>(this->VisibleBagTabs().size() * bag_pitch + 30),
            static_cast<int>(b.height), this->sidebar_scroll_};
    }
    auto satchel_module::GridScrollbar(void) const -> ui::config_scrollbar
    {
        if (this->minimized_) return {};
        const auto b = this->GridBounds();
        return {{b.x + b.width + 4, b.y, 10, b.height}, static_cast<int>(this->PageCount()), 1, static_cast<int>(this->page_)};
    }
    auto satchel_module::Scroll(const scroll_area area, const int position) -> void
    {
        if (area == scroll_area::bags) this->sidebar_scroll_ = std::clamp(position, 0, this->SidebarScrollbar().Maximum());
        else if (area == scroll_area::items)
        {
            this->page_ = static_cast<size_t>(std::clamp(position, 0, this->GridScrollbar().Maximum()));
            this->focus_cell_ = this->page_ * this->PageSize();
            this->ReconcileFocus({});
        }
    }
    auto satchel_module::EnsureSidebarFocus(void) -> void
    {
        if (this->minimized_ || (this->region_ != region::bags && this->region_ != region::slips)) return;
        const auto b = this->SidebarBounds();
        const auto focus = this->FocusBounds();
        if (focus.y < b.y) this->sidebar_scroll_ -= static_cast<int>(std::ceil(b.y - focus.y));
        else if (focus.y + focus.height > b.y + b.height)
            this->sidebar_scroll_ += static_cast<int>(std::ceil(focus.y + focus.height - b.y - b.height));
        this->sidebar_scroll_ = std::clamp(this->sidebar_scroll_, 0, this->SidebarScrollbar().Maximum());
    }
    auto satchel_module::RegionAvailable(const region value) const -> bool
    {
        if (this->position_locked_ && (value == region::move || value == region::resize)) return false;
        return !this->minimized_ || (value != region::grid && value != region::bags && value != region::slips &&
            value != region::search && value != region::resize);
    }
    auto satchel_module::CycleRegion(const int direction) -> void
    {
        const auto count = static_cast<int>(region::count);
        do { this->region_ = static_cast<region>((static_cast<int>(this->region_) + direction + count) % count); }
        while (!this->RegionAvailable(this->region_));
        this->help_open_ = false;
        this->EnsureSidebarFocus();
    }
    auto satchel_module::ToggleMinimized(void) -> void
    {
        this->FinishWindow(true);
        this->minimized_ = !this->minimized_;
        this->dialog_ = dialog::none; this->editor_.Cancel(); this->picks_.clear();
        this->help_open_ = this->dragging_ = false; this->scrolling_ = scroll_area::none;
        if (!this->navigation_focus_) this->ReleaseBrowseFocus();
        this->down_item_ = {}; this->last_click_ = 0;
        this->region_ = region::minimize;
        this->settings_changed_ = true;
        this->UpdateSize();
        if (!this->minimized_ && this->viewport_.width > 0)
        {
            auto& b = this->state_.bounds;
            b.x = std::clamp(b.x, this->viewport_.x, std::max(this->viewport_.x, this->viewport_.x + this->viewport_.width - b.width));
            b.y = std::clamp(b.y, this->viewport_.y, std::max(this->viewport_.y, this->viewport_.y + this->viewport_.height - b.height));
        }
        this->PlaySelect();
    }
    auto satchel_module::CellBounds(const size_t index) const -> ui::rect
    {
        const auto b = this->GridBounds(); const auto cell = this->icon_size_ + 8; const auto pageIndex = index % this->PageSize();
        const auto columns = this->Columns(), rows = this->Rows();
        const auto pitchX = cell + CellGap(b.width, cell, columns, this->spacing_);
        const auto pitchY = cell + CellGap(b.height, cell, rows, this->spacing_);
        return {b.x + (pageIndex % columns) * pitchX, b.y + (pageIndex / columns) * pitchY, cell, cell};
    }
    auto satchel_module::ItemAt(const float x, const float y) const -> std::optional<size_t>
    {
        if (this->minimized_ || !Contains(this->GridBounds(), x, y)) return std::nullopt;
        const auto first = this->page_ * this->PageSize();
        for (size_t i = first; i < std::min(first + this->PageSize(), this->cells_.size()); ++i)
            if (Contains(this->CellBounds(i), x, y)) return i;
        return std::nullopt;
    }
    auto satchel_module::FocusBounds(void) const -> ui::rect
    {
        if (!this->RegionAvailable(this->region_)) return {};
        switch (this->region_)
        {
        case region::grid: return this->CurrentCell() ? this->CellBounds(this->focus_cell_) : ui::rect{};
        case region::bags:
            if (this->bag_ < 0) return this->BagBounds(0);
            for (size_t i = 0; i < satchel_bag_order.size(); ++i) if (satchel_bag_order[i] == this->bag_) return this->BagBounds(i + 1);
            return this->BagBounds(0);
        case region::character: return this->CharacterBounds();
        case region::slips: return this->SlipBounds();
        case region::search: return this->SearchBounds();
        case region::help: return this->HelpBounds();
        case region::minimize: return this->MinimizeBounds();
        case region::move: return this->TitleBounds();
        case region::resize: return this->ResizeBounds();
        case region::close: return this->CloseBounds();
        default: break;
        }
        return {};
    }

    auto satchel_module::UpdateSize(void) -> void
    {
        const auto pitch = this->icon_size_ + 8 + this->spacing_;
        const auto inset = (this->container_.padding + 5) * 2;
        const auto chrome = sidebar_width + sidebar_gap + inset;
        const auto oldColumns = this->Columns(), oldRows = this->Rows();
        if (this->width_ <= 0) this->width_ = chrome + static_cast<float>(oldColumns) * pitch - this->spacing_ +
            (this->cells_.size() > oldColumns * oldRows ? scroll_gutter : 0);
        if (this->height_ <= 0) this->height_ = 120 + inset + static_cast<float>(oldRows) * pitch - this->spacing_;


        this->width_ = std::max(std::max(this->state_.minimum_width, chrome + this->icon_size_ + 8), this->width_);
        this->height_ = std::max(std::max(this->state_.minimum_height, 120 + inset + this->icon_size_ + 8), this->height_);
        const auto cell = this->icon_size_ + 8;
        this->rows_ = FitCells(this->height_ - 120 - inset, cell, this->spacing_, oldRows);
        this->columns_ = FitCells(this->width_ - chrome, cell, this->spacing_, oldColumns);
        if (this->cells_.size() > this->PageSize())
            this->columns_ = FitCells(this->width_ - chrome - scroll_gutter, cell, this->spacing_, oldColumns);
        this->state_.bounds.width = this->width_;
        this->state_.bounds.height = this->minimized_ ? 24 + inset : this->height_;
        if (oldColumns != this->Columns() || oldRows != this->Rows()) this->ReconcileFocus({});
        if (!this->minimized_) this->sidebar_scroll_ = std::clamp(this->sidebar_scroll_, 0, this->SidebarScrollbar().Maximum());
    }

    auto satchel_module::BeginWindow(const window_action action) -> void
    {
        if (this->position_locked_) return;
        this->UpdateSize();
        this->window_start_ = this->state_.bounds;
        this->window_columns_ = this->columns_; this->window_rows_ = this->rows_;
        this->window_action_ = action;
        this->region_ = action == window_action::move ? region::move : region::resize;
        this->editor_.Cancel(); this->help_open_ = false;
    }

    auto satchel_module::AdjustWindow(const float dx, const float dy) -> void
    {
        auto& bounds = this->state_.bounds;
        if (this->window_action_ == window_action::move)
        {
            bounds.x += dx * (this->pointer_down_ ? this->pointer_transform_.x : 1); bounds.y += dy * (this->pointer_down_ ? this->pointer_transform_.y : 1);
            if (this->viewport_.width > 0)
            {
                const auto viewport = this->state_.transform.Map(this->viewport_);
                bounds.x = std::clamp(bounds.x, viewport.x, std::max(viewport.x, viewport.x + viewport.width - bounds.width * this->state_.transform.x));
                bounds.y = std::clamp(bounds.y, viewport.y, std::max(viewport.y, viewport.y + viewport.height - bounds.height * this->state_.transform.y));
            }
        }
        else if (this->window_action_ == window_action::resize)
        {
            const auto maximumWidth = this->viewport_.width > 0 ? this->viewport_.x + this->viewport_.width - bounds.x : 16384.0f;
            const auto maximumHeight = this->viewport_.height > 0 ? this->viewport_.y + this->viewport_.height - bounds.y : 16384.0f;
            this->width_ = std::clamp(bounds.width + dx, this->state_.minimum_width, std::max(this->state_.minimum_width, maximumWidth));
            this->height_ = std::clamp(bounds.height + dy, this->state_.minimum_height, std::max(this->state_.minimum_height, maximumHeight));
            this->UpdateSize(); this->EnsureSidebarFocus();
        }
    }

    auto satchel_module::FinishWindow(const bool accept) -> void
    {
        if (this->window_action_ == window_action::none) return;
        if (this->pointer_down_ && !this->navigation_focus_) this->ReleaseBrowseFocus();
        if (accept)
            this->settings_changed_ = this->settings_changed_ || this->state_.bounds.x != this->window_start_.x || this->state_.bounds.y != this->window_start_.y ||
                this->state_.bounds.width != this->window_start_.width || this->state_.bounds.height != this->window_start_.height;
        else
        {
            this->state_.bounds = this->window_start_;
            this->columns_ = this->window_columns_; this->rows_ = this->window_rows_;
            if (!this->minimized_) { this->width_ = this->window_start_.width; this->height_ = this->window_start_.height; }
            this->rebuild_ = true;
        }
        this->window_action_ = window_action::none;
        if (!this->controller_ && !this->keyboard_) this->navigation_focus_ = false;
    }

    auto satchel_module::DialogBounds(void) const -> ui::rect
    {
        const auto b = this->state_.transform.Map(this->state_.bounds);
        if (this->ItemMenuOpen())
        {
            const auto viewport = this->viewport_.width > 0 ? this->state_.transform.Map(this->viewport_) : b;
            const float width = std::min(item_menu_width, std::max(80.0f, viewport.width - 16));
            const float height = item_menu_header + 12 + static_cast<float>(std::min(picker_rows, this->picks_.size())) * item_menu_row +
                (this->dialog_ == dialog::actions ? 12 : 0) + (this->picks_.size() > picker_rows ? 24 : 0);
            float x = this->action_anchor_.x + this->action_anchor_.width + 4;
            if (x + width > viewport.x + viewport.width - 8) x = this->action_anchor_.x - width - 4;
            float y = this->action_anchor_.y + (this->action_anchor_.height == 0 ? 4 : 0);
            if (y + height > viewport.y + viewport.height - 8) y = this->action_anchor_.y + this->action_anchor_.height - height;
            return {std::clamp(x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8)),
                std::clamp(y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8)), width, height};
        }
        if (this->dialog_ == dialog::characters)
        {
            const auto viewport = this->viewport_.width > 0 ? this->state_.transform.Map(this->viewport_) : b;
            const auto anchor = this->state_.transform.Map(this->CharacterBounds());
            const float width = std::min(240.0f, std::max(80.0f, viewport.width - 16));
            const float height = 16 + static_cast<float>(std::min(picker_rows, this->picks_.size())) * 28 + (this->picks_.size() > picker_rows ? 24 : 0);
            const float x = std::clamp(anchor.x + anchor.width - width, viewport.x + 8,
                std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
            float y = anchor.y + anchor.height + 6;
            if (y + height > viewport.y + viewport.height - 8) y = anchor.y - height - 6;
            y = std::clamp(y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
            return {x, y, width, height};
        }
        const float width = std::min(350.0f, b.width - 24);
        const bool numeric = this->dialog_ == dialog::quantity || this->dialog_ == dialog::price;
        const float height = numeric ? 152.0f : 64.0f + static_cast<float>(std::min(picker_rows, this->picks_.size())) * 28;
        return {b.x + (b.width - width) * 0.5f, b.y + std::max(10.0f, (b.height - height) * 0.5f), width, height};
    }
    auto satchel_module::PickBounds(const size_t index) const -> ui::rect
    {
        const auto b = this->DialogBounds();
        if (this->ItemMenuOpen())
        {
            const float gap = this->dialog_ == dialog::actions ? (index >= 5 ? 6.0f : 0) + (index >= 6 ? 6.0f : 0) : 0;
            return {b.x + 6, b.y + item_menu_header + 4 + index * item_menu_row + gap, b.width - 12, item_menu_row - 2};
        }
        if (this->dialog_ == dialog::characters) return {b.x + 8, b.y + 8 + index * 28, b.width - 16, 26};
        if (this->dialog_ == dialog::quantity || this->dialog_ == dialog::price)
        {
            if (index == 0) return {b.x + 18, b.y + 54, b.width - 36, 28};
            return {b.x + 18 + (index - 1) * (b.width - 30) * 0.5f, b.y + b.height - 42, (b.width - 42) * 0.5f, 26};
        }
        return {b.x + 12, b.y + 36 + index * 28, b.width - 24, 26};
    }

    auto satchel_module::OpenPicker(const dialog type) -> void
    {
        this->pressed_pick_.reset();
        this->editor_.Cancel(); this->dialog_ = type; this->picks_.clear(); this->pick_index_ = this->pick_page_ = 0;
        if (type == dialog::characters)
        {
            const auto* live = this->source_ ? &this->source_->Snapshot() : nullptr;
            this->picks_.push_back({live && !live->name.empty() ? live->name + " (live)" : "Current character", "", 0});
            if (this->cache_) for (const auto& character : this->cache_->Characters())
                if (!live || character.identity != live->identity)
                    this->picks_.push_back({character.name + " (saved)", character.identity, 0});
            for (size_t i = 0; i < this->picks_.size(); ++i) if (this->picks_[i].value == this->character_) this->pick_index_ = i;
        }
        else if (type == dialog::slips)
        {
            this->picks_.push_back({"Bag contents", "", -1});
            for (const auto& slip : this->owned_slips_)
                this->picks_.push_back({this->Name(slip.id) + " (" + std::to_string(SatchelDecodeSlip(slip.id, slip.extra).size()) + " stored)", "", slip.id - 29312});
            for (size_t i = 0; i < this->picks_.size(); ++i) if (this->picks_[i].id == this->slip_) this->pick_index_ = i;
        }
        this->pick_page_ = this->pick_index_ / picker_rows;
        if (this->sounds_) this->sounds_->MenuOpened(this->now_);
    }

    auto satchel_module::OpenActions(const satchel_item& item, const std::optional<ui::point> pointer) -> void
    {
        if (this->ReadOnly() || item.read_only) { this->message_ = "Read-only view: item actions are unavailable."; return; }
        if (this->actions_ && this->actions_->Busy()) { this->message_ = "Waiting for the previous inventory action."; return; }
        this->action_item_ = item; this->action_identity_ = this->Snapshot()->identity;
        this->action_name_ = this->Name(item.id);
        this->action_anchor_ = this->state_.transform.Map(pointer ? ui::rect{pointer->x, pointer->y, 0, 0} : this->CellBounds(this->focus_cell_));
        this->quantity_ = item.count; this->price_ = item.price; this->equipment_slot_ = -1;
        this->ShowActions(); this->PlaySelect();
    }

    auto satchel_module::ShowActions(const bool returning) -> void
    {
        this->editor_.Cancel(); this->pressed_pick_.reset(); this->dialog_ = dialog::actions;
        if (this->pointer_down_) this->discard_pointer_release_ = true;
        this->picks_ = {{"Move to bag...", "", static_cast<int>(satchel_action::move)},
            {"Split stack...", "", static_cast<int>(satchel_action::split), this->action_item_.count > 1},
            {"Use item", "", static_cast<int>(satchel_action::use)},
            {"Equip...", "", static_cast<int>(satchel_action::equip)},
            {this->action_item_.price ? "Change / remove bazaar price..." : "Set bazaar price...", "", static_cast<int>(satchel_action::bazaar)},
            {"Drop item...", "", static_cast<int>(satchel_action::drop)},
            {"Sort this bag", "", static_cast<int>(satchel_action::sort)}, {"Cancel", "", -1}};
        this->pick_index_ = this->picks_.size() - 1; this->pick_page_ = 0;
        if (returning)
            for (size_t i = 0; i < this->picks_.size(); ++i) if (this->picks_[i].id == this->action_) { this->pick_index_ = i; break; }
    }

    auto satchel_module::CancelDialog(void) -> void
    {
        if (this->pointer_down_) this->discard_pointer_release_ = true;
        this->pressed_pick_.reset();
        this->region_ = this->dialog_ == dialog::characters ? region::character : region::grid;
        if (!this->RegionAvailable(this->region_)) this->region_ = region::minimize;
        this->dialog_ = dialog::none; this->picks_.clear(); this->editor_.Cancel();
        if (!this->controller_ && !this->keyboard_) this->navigation_focus_ = false;
    }

    auto satchel_module::StartAction(const int action) -> void
    {
        this->pressed_pick_.reset();
        if (this->pointer_down_) this->discard_pointer_release_ = true;
        this->action_ = action; this->quantity_ = this->action_item_.count;
        const auto type = static_cast<satchel_action>(action);
        if (type == satchel_action::move)
        {
            this->dialog_ = dialog::destinations; this->picks_.clear(); this->picks_.push_back({"Back", "", -1});
            const auto* snapshot = this->Snapshot();
            if (snapshot) for (const auto bag : satchel_bag_order)
                if (bag != this->action_item_.bag && bag != 3 && snapshot->bags[bag].ready && snapshot->bags[bag].accessible)
                    this->picks_.push_back({satchel_bag_names[bag], "", bag});
            this->pick_index_ = this->pick_page_ = 0; return;
        }
        if (type == satchel_action::split)
        { this->destination_ = this->action_item_.bag; this->quantity_ = 1; this->EnterQuantity(); return; }
        if (type == satchel_action::equip)
        {
            const auto* resources = this->core_ ? this->core_->GetResourceManager() : nullptr;
            const auto* resource = resources ? resources->GetItemById(this->action_item_.id) : nullptr;
            this->dialog_ = dialog::equipment; this->picks_.clear(); this->picks_.push_back({"Back", "", -1});
            if (resource) for (int slot = 0; slot < 16; ++slot)
                if (resource->Slots & (1u << slot)) this->picks_.push_back({equipment_slot_names[slot], "", slot});
            this->pick_index_ = this->pick_page_ = 0;
            if (this->picks_.size() == 1) { this->message_ = "This item has no equipment slot."; this->CancelDialog(); }
            return;
        }
        if (type == satchel_action::bazaar)
        { this->dialog_ = dialog::price; this->pick_index_ = 0; this->editor_.Open(std::to_string(this->price_), 10); return; }
        if (type == satchel_action::drop) { this->EnterQuantity(); return; }
        this->Submit();
    }

    auto satchel_module::EnterQuantity(void) -> void
    {
        if (this->action_item_.count <= 1 && static_cast<satchel_action>(this->action_) != satchel_action::split)
        {
            this->quantity_ = 1;
            if (static_cast<satchel_action>(this->action_) == satchel_action::drop)
            { this->dialog_ = dialog::drop; this->picks_ = {{"Cancel", "", 0}, {"Drop 1 item", "", 1}}; this->pick_index_ = this->pick_page_ = 0; }
            else this->Submit();
            return;
        }
        this->dialog_ = dialog::quantity; this->pick_index_ = 0;
        this->editor_.Open(std::to_string(this->quantity_), 10);
    }

    auto satchel_module::Submit(void) -> void
    {
        if (this->ReadOnly() || !this->actions_ || !this->source_ || this->action_identity_ != this->source_->Snapshot().identity)
        { this->message_ = "The selected character or inventory is no longer actionable."; this->CancelDialog(); return; }
        satchel_action_request request{};
        request.action = static_cast<satchel_action>(this->action_); request.identity = this->action_identity_; request.item = this->action_item_;
        request.destination = static_cast<uint8_t>(this->destination_); request.quantity = this->quantity_; request.price = this->price_; request.equipment_slot = this->equipment_slot_;
        const bool accepted = this->actions_->Submit(this->core_, this->source_->Snapshot(), request, this->now_);
        this->message_ = this->actions_->Status();
        if (accepted && this->autosort_ && (request.action == satchel_action::move || request.action == satchel_action::split)) this->pending_autosort_bag_ = request.destination;
        this->CancelDialog(); this->PlaySelect();
    }

    auto satchel_module::Transfer(const satchel_item& item, const int destination) -> void
    {
        if (this->ReadOnly()) { this->message_ = "Read-only view: transfers are unavailable."; return; }
        if (destination > 16 || destination == item.bag) return;
        this->action_item_ = item; this->action_identity_ = this->Snapshot()->identity;
        this->action_ = static_cast<int>(satchel_action::move); this->destination_ = destination < 0 ? (item.bag == 0 ? 5 : 0) : destination; this->quantity_ = item.count;
        if (destination < 0 && !this->Snapshot()->bags[this->destination_].accessible) { this->StartAction(this->action_); return; }
        this->EnterQuantity();
    }

    auto satchel_module::SelectPick(void) -> void
    {
        if (this->pick_index_ >= this->picks_.size() || !this->picks_[this->pick_index_].available) return;
        const auto pick = this->picks_[this->pick_index_]; const auto type = this->dialog_;
        this->PlaySelect();
        if (type == dialog::characters)
        { this->character_ = pick.value; this->slip_ = -1; this->page_ = this->focus_cell_ = 0; this->CancelDialog(); this->rebuild_ = true; this->Rebuild(); }
        else if (type == dialog::slips)
        { this->slip_ = pick.id; this->page_ = this->focus_cell_ = 0; this->CancelDialog(); this->rebuild_ = true; this->Rebuild(); }
        else if (type == dialog::actions)
        { if (pick.id < 0) this->CancelDialog(); else this->StartAction(pick.id); }
        else if (type == dialog::destinations)
        { if (pick.id < 0) this->ShowActions(true); else { this->destination_ = pick.id; this->EnterQuantity(); } }
        else if (type == dialog::equipment)
        { if (pick.id < 0) this->ShowActions(true); else { this->equipment_slot_ = pick.id; this->Submit(); } }
        else if (type == dialog::drop)
        { if (pick.id == 0) this->CancelDialog(); else this->Submit(); }
    }

    auto satchel_module::BeginSearch(void) -> void
    { if (this->minimized_) this->ToggleMinimized(); this->region_ = region::search; this->editor_.Open(this->query_, 100); }

    auto satchel_module::ReleaseBrowseFocus(void) -> void
    { this->keyboard_ = this->controller_ = this->navigation_focus_ = false; }

    auto satchel_module::CommitEditor(void) -> void
    {
        if (this->dialog_ == dialog::quantity || this->dialog_ == dialog::price)
        {
            uint32_t value{};
            const auto maximum = this->dialog_ == dialog::quantity ?
                this->action_item_.count - (static_cast<satchel_action>(this->action_) == satchel_action::split ? 1 : 0) : 99999999u;
            if (!Number(this->editor_.Editor().Text(), value) || value > maximum || (this->dialog_ == dialog::quantity && value == 0))
            { this->message_ = "Enter " + std::to_string(this->dialog_ == dialog::price ? 0 : 1) + " to " + std::to_string(maximum) + "."; return; }
            if (this->dialog_ == dialog::quantity) this->quantity_ = value; else this->price_ = value;
            this->editor_.Cancel();
            if (static_cast<satchel_action>(this->action_) == satchel_action::drop)
            {
                this->dialog_ = dialog::drop; this->picks_ = {{"Cancel", "", 0}, {"Drop " + std::to_string(this->quantity_) + " item(s)", "", 1}};
                this->pick_index_ = this->pick_page_ = 0;
            }
            else this->Submit();
        }
        else { this->query_ = this->editor_.Editor().Text(); this->editor_.Cancel(); this->region_ = region::grid; this->rebuild_ = true; this->Rebuild(); }
    }

    auto satchel_module::ActivateRegion(void) -> void
    {
        if (!this->RegionAvailable(this->region_)) return;
        switch (this->region_)
        {
        case region::grid: if (const auto* cell = this->CurrentCell()) this->OpenActions(cell->item); break;
        case region::character: this->OpenPicker(dialog::characters); break;
        case region::slips: this->OpenPicker(dialog::slips); break;
        case region::search: this->BeginSearch(); break;
        case region::help: this->help_open_ = !this->help_open_; break;
        case region::minimize: this->ToggleMinimized(); break;
        case region::move: this->BeginWindow(window_action::move); break;
        case region::resize: this->BeginWindow(window_action::resize); break;
        case region::close: this->Open(false); break;
        case region::bags: this->slip_ = -1; this->region_ = region::grid; this->rebuild_ = true; break;
        default: break;
        }
    }

    auto satchel_module::MoveGrid(const int dx, const int dy) -> void
    {
        if (this->cells_.empty() || (!dx && !dy)) return;
        const auto count = static_cast<int>(this->cells_.size());
        const auto choose = [&](const size_t index) { this->focus_cell_ = index; this->page_ = index / this->PageSize(); };
        if (dy)
        {
            const auto columns = static_cast<int>(this->Columns()); const auto rows = (count + columns - 1) / columns;
            const auto row = static_cast<int>(this->focus_cell_) / columns; const auto column = static_cast<int>(this->focus_cell_) % columns;
            for (int step = 1; step < rows; ++step)
            {
                const auto nextRow = (row + dy * step + rows * 2) % rows;
                int candidate = -1, distance = columns + 1;
                for (int col = 0; col < columns; ++col)
                {
                    const auto next = nextRow * columns + col;
                    if (next < count && !this->cells_[next].empty && std::abs(column - col) < distance)
                    { candidate = next; distance = std::abs(column - col); }
                }
                if (candidate >= 0) { choose(candidate); return; }
            }
        }
        const auto direction = dx ? dx : dy;
        for (int step = 1; step <= count; ++step)
        { const auto next = (static_cast<int>(this->focus_cell_) + direction * step + count * 2) % count; if (!this->cells_[next].empty) { choose(next); return; } }
    }

    auto satchel_module::HandleControllerAction(const controller_action action, const bool repeated) -> bool
    {
        if (!this->opened_ || !this->state_.enabled) return false;
        if (!this->controller_ && !this->InputFocused()) return false;
        this->navigation_focus_ = true;
        if (repeated && action != controller_action::up && action != controller_action::down && action != controller_action::left && action != controller_action::right) return true;
        if (this->window_action_ != window_action::none)
        {
            if (action == controller_action::cancel || action == controller_action::confirm)
            {
                this->FinishWindow(action == controller_action::confirm);
                this->discard_pointer_release_ = this->pointer_down_; this->pointer_down_ = false;
            }
            else
            {
                const float step = 5;
                const auto dx = action == controller_action::left ? -step : action == controller_action::right ? step : 0;
                const auto dy = action == controller_action::up ? -step : action == controller_action::down ? step : 0;
                if (dx || dy) this->AdjustWindow(dx, dy);
            }
            return true;
        }
        if (action == controller_action::cancel)
        {
            if (this->dialog_ == dialog::destinations || this->dialog_ == dialog::equipment) this->ShowActions(true);
            else if (this->dialog_ != dialog::none) this->CancelDialog();
            else if (this->editor_.Focused()) { this->editor_.Cancel(); this->region_ = region::grid; }
            else if (this->help_open_ || this->region_ == region::help) { this->help_open_ = false; this->region_ = this->minimized_ ? region::minimize : region::grid; }
            else this->Open(false);
            return true;
        }
        if (this->dialog_ == dialog::quantity || this->dialog_ == dialog::price)
        {
            if (action == controller_action::left || action == controller_action::right)
            { this->pick_index_ = (this->pick_index_ + (action == controller_action::left ? 2 : 1)) % 3; return true; }
            if (action == controller_action::up || action == controller_action::down)
            {
                uint32_t value{}; Number(this->editor_.Editor().Text(), value);
                const auto maximum = this->dialog_ == dialog::price ? 99999999u : this->action_item_.count - (static_cast<satchel_action>(this->action_) == satchel_action::split ? 1 : 0);
                const auto minimum = this->dialog_ == dialog::price ? 0u : 1u;
                const uint32_t step = this->dialog_ == dialog::price ? 100 : 1;
                if (action == controller_action::up) value = value > maximum - std::min(maximum, step) ? maximum : std::min(maximum, value + step);
                else value = value > step ? std::max(minimum, value - step) : minimum;
                this->editor_.Open(std::to_string(value), 10); this->pick_index_ = 0; return true;
            }
            if (action == controller_action::confirm)
            { if (this->pick_index_ == 1) this->CancelDialog(); else if (this->pick_index_ == 2) this->CommitEditor(); else this->pick_index_ = 2; return true; }
            if (action == controller_action::actions) { this->editor_.SelectAll(); return true; }
            return true;
        }
        if (this->dialog_ != dialog::none)
        {
            const auto count = this->picks_.size();
            if (count && (action == controller_action::up || action == controller_action::down || action == controller_action::left || action == controller_action::right))
            {
                const int step = action == controller_action::up ? -1 : action == controller_action::down ? 1 : action == controller_action::left ? -static_cast<int>(picker_rows) : static_cast<int>(picker_rows);
                const auto size = static_cast<int>(count);
                this->pick_index_ = static_cast<size_t>(((static_cast<int>(this->pick_index_) + step) % size + size) % size);
                if (this->ItemMenuOpen())
                    for (size_t attempt = 0; attempt < count && !this->picks_[this->pick_index_].available; ++attempt)
                        this->pick_index_ = static_cast<size_t>((static_cast<int>(this->pick_index_) + (step < 0 ? -1 : 1) + size) % size);
                this->pick_page_ = this->pick_index_ / picker_rows;
            }
            if (action == controller_action::confirm) this->SelectPick();
            return true;
        }
        if (this->editor_.Focused())
        { if (action == controller_action::confirm) this->CommitEditor(); else if (action == controller_action::actions) { this->query_.clear(); this->editor_.Open({}, 100); this->rebuild_ = true; } return true; }
        if (action == controller_action::next_tab || action == controller_action::previous_tab)
        {
            if (!this->minimized_) this->MoveBag(action == controller_action::previous_tab ? -1 : 1);
            return true;
        }
        if (action == controller_action::cycle_window || action == controller_action::focus)
        { this->CycleRegion(1); return true; }
        if (action == controller_action::confirm || action == controller_action::actions)
        { if (action == controller_action::actions && this->region_ == region::search) this->BeginSearch(); else this->ActivateRegion(); return true; }
        const int dx = action == controller_action::left ? -1 : action == controller_action::right ? 1 : 0;
        const int dy = action == controller_action::up ? -1 : action == controller_action::down ? 1 : 0;
        if (this->region_ == region::grid) this->MoveGrid(dx, dy);
        else if (this->region_ == region::bags && (dx || dy))
        {
            this->MoveBag(dx ? dx : dy);
        }
        else if (dx || dy)
        {
            this->CycleRegion(dx < 0 || dy < 0 ? -1 : 1);
        }
        return true;
    }

    auto satchel_module::HandleKey(const WPARAM key, const LPARAM flags) -> bool
    {
        if (!this->opened_ || !this->state_.enabled || (static_cast<uint32_t>(flags) & 0x80000000u) != 0) return false;
        const bool repeated = (static_cast<uint32_t>(flags) & 0x40000000u) != 0;
        if ((::GetKeyState(VK_MENU) & 0x8000) != 0 || (static_cast<uint32_t>(flags) & 0x20000000u) != 0) return false;
        if (key == 'F' && (::GetKeyState(VK_CONTROL) & 0x8000) != 0 && this->dialog_ == dialog::none && this->window_action_ == window_action::none)
        { if (!repeated) { this->BeginSearch(); this->navigation_focus_ = true; } return true; }
        if (key == VK_ESCAPE && this->help_open_ && !this->InputFocused())
        { if (!repeated) this->help_open_ = false; return true; }
        if (!this->InputFocused()) return false;
        if (this->editor_.Focused())
        {
            if (key == VK_ESCAPE)
            {
                if (!repeated)
                {
                    if (this->dialog_ != dialog::none) this->CancelDialog();
                    else
                    {
                        this->editor_.Cancel(); this->region_ = region::grid;
                        this->ReleaseBrowseFocus();
                    }
                }
                return true;
            }
            if (key == VK_RETURN)
            {
                if (!repeated)
                {
                    if (this->dialog_ == dialog::none) this->keyboard_ = this->navigation_focus_ = true;
                    this->CommitEditor();
                }
                return true;
            }
            this->editor_.HandleKey(key, flags, ::GetForegroundWindow());
            if (this->dialog_ == dialog::none) { this->query_ = this->editor_.Editor().Text(); this->page_ = this->focus_cell_ = 0; this->rebuild_ = true; }
            return true;
        }
        if ((key == VK_PRIOR || key == VK_NEXT) && this->dialog_ == dialog::none)
        { this->navigation_focus_ = true; if (!this->minimized_ && this->window_action_ == window_action::none) this->Page(key == VK_PRIOR ? -1 : 1); return true; }
        controller_action action{};
        switch (key)
        {
        case VK_ESCAPE: action = controller_action::cancel; break;
        case VK_RETURN: action = controller_action::confirm; break;
        case VK_APPS: case VK_SPACE: action = controller_action::actions; break;
        case VK_UP: action = controller_action::up; break;
        case VK_DOWN: action = controller_action::down; break;
        case VK_LEFT: action = controller_action::left; break;
        case VK_RIGHT: action = controller_action::right; break;
        case VK_PRIOR: action = controller_action::previous_tab; break;
        case VK_NEXT: action = controller_action::next_tab; break;
        case VK_TAB: case VK_F6: action = controller_action::cycle_window; break;
        default: return this->window_action_ != window_action::none;
        }
        return this->HandleControllerAction(action, repeated);
    }

    auto satchel_module::LayoutHandleMouse(const uint32_t message, const WPARAM value, const float x, const float y, const bool blocked) -> bool
    {
        this->hover_x_ = x; this->hover_y_ = y;
        const bool release = message == WM_LBUTTONUP;
        if (release && this->discard_pointer_release_)
        { this->pointer_down_ = this->discard_pointer_release_ = false; this->scrolling_ = scroll_area::none; return true; }
        if (release && this->pointer_down_ && (!this->opened_ || blocked))
        { this->SuspendInput(); return true; }
        if (!this->opened_ || !this->state_.enabled || blocked) return false;
        const bool inside = Contains(this->state_.bounds, x, y);
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDOWN || message == WM_MOUSEWHEEL)
        {
            this->ReleaseBrowseFocus();
            if (this->dialog_ == dialog::none && !Contains(this->SearchBounds(), x, y)) this->editor_.Cancel();
        }
        if (this->scrolling_ != scroll_area::none && this->pointer_down_ && (message == WM_MOUSEMOVE || release))
        {
            const auto scrollbar = this->scrolling_ == scroll_area::bags ? this->SidebarScrollbar() : this->GridScrollbar();
            this->Scroll(this->scrolling_, scrollbar.PositionAt(y, this->scroll_grab_));
            if (release) { this->scrolling_ = scroll_area::none; this->pointer_down_ = false; }
            return true;
        }
        if (this->window_action_ != window_action::none && this->pointer_down_ && (message == WM_MOUSEMOVE || release))
        {
            this->state_.bounds = this->window_start_;
            this->columns_ = this->window_columns_; this->rows_ = this->window_rows_;
            this->AdjustWindow(x - this->down_x_, y - this->down_y_);
            if (release) { this->FinishWindow(true); this->pointer_down_ = false; }
            return true;
        }
        if (message == WM_MOUSEMOVE)
        {
            if (!this->position_locked_ && this->dialog_ == dialog::none && Contains(this->ResizeBounds(), x, y))
            { ::SetCursor(::LoadCursor(nullptr, IDC_SIZENWSE)); return true; }
            if (!this->position_locked_ && this->dialog_ == dialog::none && Contains(this->TitleBounds(), x, y))
            { ::SetCursor(::LoadCursor(nullptr, IDC_SIZEALL)); return true; }
            if (this->pointer_down_ && this->down_item_.id && std::abs(x - this->down_x_) + std::abs(y - this->down_y_) > 7) this->dragging_ = true;
            return this->pointer_down_ || this->dialog_ != dialog::none;
        }
        if (message == WM_MOUSEWHEEL && (inside || (this->dialog_ != dialog::none && Contains(this->DialogBounds(), x, y))))
        {
            const int direction = GET_WHEEL_DELTA_WPARAM(value) > 0 ? -1 : 1;
            if (this->dialog_ != dialog::none && this->dialog_ != dialog::quantity && this->dialog_ != dialog::price)
            {
                const auto pages = std::max<size_t>(1, (this->picks_.size() + picker_rows - 1) / picker_rows);
                if (direction < 0 && this->pick_page_) --this->pick_page_;
                else if (direction > 0 && this->pick_page_ + 1 < pages) ++this->pick_page_;
                this->pick_index_ = this->pick_page_ * picker_rows;
            }
            else if (this->dialog_ == dialog::none && !this->minimized_)
            {
                if (Contains(this->SidebarBounds(), x, y)) this->Scroll(scroll_area::bags, this->sidebar_scroll_ + direction * static_cast<int>(bag_pitch * 3));
                else if (Contains(this->GridBounds(), x, y) || Contains(this->GridScrollbar().track, x, y)) this->Page(direction);
            }
            return true;
        }
        if (message == WM_RBUTTONDOWN && (inside || this->dialog_ != dialog::none))
        {
            if (this->dialog_ != dialog::none) { this->CancelDialog(); return true; }
            if (const auto index = this->ItemAt(x, y); index && !this->cells_[*index].empty)
            { this->focus_cell_ = *index; this->region_ = region::grid; this->OpenActions(this->cells_[*index].item, ui::point{x, y}); }
            return true;
        }
        if (message == WM_RBUTTONUP && inside) return true;
        if (message == WM_LBUTTONDBLCLK && inside && this->dialog_ == dialog::none)
        {
            this->last_click_ = 0; this->pointer_down_ = this->discard_pointer_release_ = true;
            if (const auto index = this->ItemAt(x, y); index && !this->cells_[*index].empty)
            {
                const auto item = this->cells_[*index].item;
                this->Transfer(item, -1);
            }
            return true;
        }
        if (message == WM_LBUTTONDOWN)
        {
            if (this->window_action_ != window_action::none) this->FinishWindow(true);
            if (!inside && this->dialog_ == dialog::none) { this->editor_.Cancel(); this->help_open_ = false; return false; }
            if (!Contains(this->HelpBounds(), x, y)) this->help_open_ = false;
            this->pointer_down_ = true; this->dragging_ = false;
            this->down_x_ = x; this->down_y_ = y; this->down_item_ = {}; this->down_key_.clear();
            if (this->ItemMenuOpen())
            {
                this->pressed_pick_.reset();
                if (!Contains(this->DialogBounds(), x, y)) { this->CancelDialog(); return true; }
                for (size_t i = this->pick_page_ * picker_rows; i < std::min((this->pick_page_ + 1) * picker_rows, this->picks_.size()); ++i)
                    if (this->picks_[i].available && Contains(this->PickBounds(i % picker_rows), x, y)) { this->pressed_pick_ = i; break; }
                return true;
            }
            if (this->dialog_ == dialog::none && !this->minimized_)
            {
                for (const auto area : {scroll_area::bags, scroll_area::items})
                {
                    const auto scrollbar = area == scroll_area::bags ? this->SidebarScrollbar() : this->GridScrollbar();
                    if (!scrollbar.Visible() || !Contains(scrollbar.track, x, y)) continue;
                    if (Contains(scrollbar.Thumb(), x, y))
                    { this->scrolling_ = area; this->scroll_grab_ = y - scrollbar.Thumb().y; }
                    else this->Scroll(area, scrollbar.PageAt(y));
                    // A track click owns its release without activating a row underneath.
                    if (this->scrolling_ == scroll_area::none) this->discard_pointer_release_ = true;
                    return true;
                }
            }
            if (!this->position_locked_ && this->dialog_ == dialog::none && (Contains(this->TitleBounds(), x, y) || Contains(this->ResizeBounds(), x, y)))
            { this->BeginWindow(Contains(this->ResizeBounds(), x, y) ? window_action::resize : window_action::move); return true; }
            if (this->dialog_ == dialog::none)
                if (const auto index = this->ItemAt(x, y); index && !this->cells_[*index].empty)
                { this->focus_cell_ = *index; this->region_ = region::grid; this->down_item_ = this->cells_[*index].item; this->down_key_ = this->cells_[*index].key; }
            return true;
        }
        if (!release || !this->pointer_down_) return this->dialog_ != dialog::none;
        this->pointer_down_ = false;
        if (this->dragging_ && this->down_item_.id)
        {
            this->dragging_ = false;
            if (Contains(this->SidebarBounds(), x, y))
                for (size_t i = 1; i < 18; ++i)
                    if (Contains(this->BagBounds(i), x, y)) { this->Transfer(this->down_item_, satchel_bag_order[i - 1]); return true; }
            if (const auto index = this->ItemAt(x, y); index && this->sort_ == 0 && this->cache_)
            {
                auto from = this->cells_.end();
                for (auto it = this->cells_.begin(); it != this->cells_.end(); ++it) if (it->key == this->down_key_) { from = it; break; }
                if (from != this->cells_.end())
                {
                    const auto to = this->cells_.begin() + *index;
                    if (to->empty) std::iter_swap(from, to);
                    else if (from < to) std::rotate(from, from + 1, to + 1); else if (to < from) std::rotate(to, from, from + 1);
                    std::vector<std::string> order;
                    for (const auto& cell : this->cells_) order.push_back(cell.empty ? empty_slot_key : cell.key);
                    this->cache_->WriteOrder(this->view_identity_, this->slip_ >= 0 ? 100 + this->slip_ : this->bag_, order);
                    this->focus_cell_ = *index;
                }
            }
            return true;
        }
        if (this->dialog_ != dialog::none)
        {
            if (this->ItemMenuOpen())
            {
                const auto pressed = std::exchange(this->pressed_pick_, {});
                if (pressed && *pressed < this->picks_.size() && this->picks_[*pressed].available &&
                    *pressed / picker_rows == this->pick_page_ && Contains(this->PickBounds(*pressed % picker_rows), x, y))
                { this->pick_index_ = *pressed; this->SelectPick(); }
                return true;
            }
            if (!Contains(this->DialogBounds(), x, y)) { this->CancelDialog(); return true; }
            if (this->dialog_ == dialog::quantity || this->dialog_ == dialog::price)
            {
                for (size_t i = 0; i < 3; ++i) if (Contains(this->PickBounds(i), x, y))
                { this->pick_index_ = i; if (i == 0) this->editor_.SelectAll(); else if (i == 1) this->CancelDialog(); else this->CommitEditor(); return true; }
            }
            else for (size_t i = this->pick_page_ * picker_rows; i < std::min(this->picks_.size(), (this->pick_page_ + 1) * picker_rows); ++i)
                if (Contains(this->PickBounds(i % picker_rows), x, y)) { this->pick_index_ = i; this->SelectPick(); return true; }
            return true;
        }
        if (!inside) return true;
        if (Contains(this->CloseBounds(), x, y)) { this->Open(false); return true; }
        if (Contains(this->HelpBounds(), x, y)) { this->region_ = region::help; this->ActivateRegion(); return true; }
        if (Contains(this->MinimizeBounds(), x, y)) { this->ToggleMinimized(); return true; }
        if (Contains(this->CharacterBounds(), x, y)) { this->region_ = region::character; this->OpenPicker(dialog::characters); return true; }
        if (this->minimized_) return true;
        if (Contains(Intersect(this->SlipBounds(), this->SidebarBounds()), x, y)) { this->region_ = region::slips; this->OpenPicker(dialog::slips); return true; }
        if (Contains(this->SearchBounds(), x, y)) { this->BeginSearch(); return true; }
        this->editor_.Cancel();
        for (const auto i : this->VisibleBagTabs()) if (Contains(Intersect(this->BagBounds(i), this->SidebarBounds()), x, y)) { this->region_ = region::bags; this->ChangeBag(i == 0 ? -1 : satchel_bag_order[i - 1]); return true; }
        if (const auto index = this->ItemAt(x, y); index && !this->cells_[*index].empty)
        {
            this->focus_cell_ = *index; this->region_ = region::grid;
            const auto item = this->cells_[*index].item;
            if (this->now_ - this->last_click_ < 350 && item.bag == this->last_click_bag_ && item.slot == this->last_click_slot_)
            { this->last_click_ = 0; this->Transfer(item, -1); }
            else { this->last_click_ = this->now_; this->last_click_bag_ = item.bag; this->last_click_slot_ = item.slot; }
        }
        return true;
    }

    auto satchel_module::DrawButton(const ui::rect& bounds, const char* label, const bool selected, const module_render_context& context) const -> void
    {
        const bool hovered = Contains(bounds, this->hover_x_, this->hover_y_);
        auto profile = this->text_[1]; profile.style.alignment = ui::text_alignment::center;
        context.DrawButton(bounds, label, selected || hovered, profile);
    }

    auto satchel_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        this->viewport_ = context.Viewport();
        this->Update(::GetTickCount64());
        this->UpdateSize();
        const auto& design = context.Design(); const auto& colors = design.GetTheme().colors;
        if (this->background_)
        {
            const auto inner = this->Inner();
            const ui::container_fill fills[]{{this->SidebarBounds(), (colors.surface_raised & 0xFFFFFF) | 0x30000000},
                {{inner.x, inner.y - 2, inner.width, 28}, (colors.surface_raised & 0xFFFFFF) | 0x40000000}};
            context.DrawContainer(this->state_.bounds, this->container_, fills);
        }
        const auto inner = this->Inner(); const auto* snapshot = this->Snapshot();
        context.DrawText("Satchel", this->TitleBounds(), this->TitleBounds(), this->text_[0]);
        this->DrawButton(this->HelpBounds(), "?", this->help_open_, context);
        this->DrawButton(this->CloseBounds(), "X", false, context);
        this->DrawButton(this->MinimizeBounds(), this->minimized_ ? "+" : "-", false, context);
        ui::config_controls controls{design, nullptr, context.NativeAssets()};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        controls.IconButton(this->CharacterBounds(), ui::icon_glyph::player,
            this->dialog_ == dialog::characters || Contains(this->CharacterBounds(), this->hover_x_, this->hover_y_));
        if (this->minimized_) return;
        if (!this->editor_.Focused() && this->dialog_ == dialog::none) this->editor_.Open(this->query_, 100), this->editor_.Cancel();
        if (this->dialog_ == dialog::none) controls.TextInput(this->SearchBounds(), this->editor_, false, this->all_bags_search_ ? "Search all bags by name or item ID" : "Search this view by name or item ID", "Ctrl+F");
        else
        {
            ui::text_input display; display.Open(this->query_, 100); display.Cancel();
            controls.TextInput(this->SearchBounds(), display, false, "Search items", "Ctrl+F");
        }
        const auto totalItems = snapshot ? std::accumulate(snapshot->bags.begin(), snapshot->bags.end(), size_t{},
            [](const size_t total, const satchel_bag& bag) { return total + bag.items.size(); }) : 0;
        {
            const auto clip = design.Clip(this->SidebarBounds());
            const auto slips = this->slip_ >= 0 ? "Slip " + std::to_string(this->slip_ + 1) : "Storage slips (" + std::to_string(this->owned_slips_.size()) + ")";
            controls.NavigationItem(this->SlipBounds(), slips.c_str(), this->slip_ >= 0,
                Contains(Intersect(this->SlipBounds(), this->SidebarBounds()), this->hover_x_, this->hover_y_));
            for (const auto i : this->VisibleBagTabs())
            {
                const auto bag = i == 0 ? -1 : static_cast<int>(satchel_bag_order[i - 1]);
                const auto count = bag < 0 ? totalItems : snapshot ? snapshot->bags[bag].items.size() : 0;
                const bool ready = snapshot && (bag < 0 ? snapshot->ready : snapshot->bags[bag].ready);
                const auto label = std::string{i == 0 ? "All bags" : satchel_bag_names[bag]} + " (" + (ready ? std::to_string(count) : "--") + ")";
                const auto bounds = this->BagBounds(i);
                controls.NavigationItem(bounds, label.c_str(), this->bag_ == bag && this->slip_ < 0,
                    Contains(Intersect(bounds, this->SidebarBounds()), this->hover_x_, this->hover_y_));
            }
        }
        controls.Scrollbar(this->SidebarScrollbar(), Contains(this->SidebarScrollbar().track, this->hover_x_, this->hover_y_), this->scrolling_ == scroll_area::bags);
        const auto first = this->page_ * this->PageSize(); const auto end = std::min(first + this->PageSize(), this->cells_.size());
        const auto slotBackground = (colors.surface & 0xFFFFFF) | (static_cast<uint32_t>(std::round(this->slot_opacity_ * 255)) << 24);
        const ui::item_slot_frame slotFrame{design, this->icon_size_ + 8, this->container_.opacity, slotBackground};
        const bool itemDrag = this->pointer_down_ && this->dragging_ && this->down_item_.id && this->sort_ == 0 && this->cache_ &&
            std::any_of(this->cells_.begin(), this->cells_.end(), [&](const auto& cell) { return cell.key == this->down_key_; });
        const auto dropTarget = itemDrag ? this->ItemAt(this->hover_x_, this->hover_y_) : std::nullopt;
        {
            const auto clip = design.Clip(this->GridBounds());
            for (size_t index = first; index < end; ++index)
            {
                const auto& cell = this->cells_[index]; const auto bounds = this->CellBounds(index);
                if (this->frames_) slotFrame.Draw(bounds);
                if (dropTarget == index && cell.key != this->down_key_) slotFrame.Highlight(bounds, colors.accent);
                if (cell.empty) continue;
                const ui::rect icon{bounds.x + 4, bounds.y + 4, this->icon_size_, this->icon_size_};
                auto* texture = context.Icons() ? context.Icons()->Item(cell.item.id) : nullptr;
                if (texture) design.DrawTexture(texture, icon);
                else
                {
                    auto fallback = this->text_[1]; fallback.style.alignment = ui::text_alignment::center;
                    context.DrawText(this->Name(cell.item.id).c_str(), bounds, bounds, fallback);
                }
                const ui::rect marker{bounds.x + 1, bounds.y + 1, bounds.width - 2, bounds.height - 2};
                if (cell.item.equipped) slotFrame.Outline(marker, colors.accent, 2);
                else if (cell.item.price) slotFrame.Outline(marker, colors.gold, 2);
                else if (cell.item.flags) slotFrame.Outline(marker, colors.muted);
                if (cell.item.count > 1)
                {
                    const auto quantity = std::to_string(cell.item.count); const float height = std::max(14.0f, design.TextHeight(this->text_[2].style));
                    const ui::rect at{icon.x - 2, icon.y + icon.height - height + 2, icon.width + 1, height};
                    context.DrawText(quantity.c_str(), at, at, this->text_[2]);
                }
                if (!this->dragging_ && Contains(bounds, this->hover_x_, this->hover_y_) && !this->controller_) slotFrame.Outline(bounds, colors.accent_dim);
            }
            if (end == first)
            {
                std::string empty = !snapshot || !snapshot->ready ? "Waiting for settled inventory..." : this->slip_ >= 0 ? "No stored items match this view." : "No items match this view.";
                if (context.IsEditing()) empty = "Satchel inventory browser  |  /xiuiplus satchel";
                const auto grid = this->GridBounds();
                const ui::rect at{grid.x, grid.y + 8, grid.width, 28}; context.DrawText(empty.c_str(), at, at, this->text_[1]);
            }
        }
        controls.Scrollbar(this->GridScrollbar(), Contains(this->GridScrollbar().track, this->hover_x_, this->hover_y_), this->scrolling_ == scroll_area::items);
        const auto occupied = std::count_if(this->cells_.begin(), this->cells_.end(), [](const auto& cell) { return !cell.empty; });
        size_t capacity{};
        if (snapshot)
        {
            if (this->bag_ >= 0 && this->slip_ < 0) capacity = snapshot->bags[this->bag_].capacity;
            else if (this->slip_ < 0) for (const auto& bag : snapshot->bags) capacity += bag.capacity;
        }
        const std::string counts = std::to_string(occupied) + (capacity ? "/" + std::to_string(capacity) : "") + " items" +
            (this->ReadOnly() ? "  |  Read-only" : "") +
            (this->PageCount() > 1 ? "  |  Page " + std::to_string(this->page_ + 1) + "/" + std::to_string(this->PageCount()) : "");
        const auto balance = snapshot && snapshot->ready ? FormatGil(snapshot->gil) : "--";
        auto gilProfile = this->text_[1]; gilProfile.style.alignment = ui::text_alignment::right;
        const float gilWidth = design.MeasureText(balance.c_str(), gilProfile.style) + 4;
        const ui::rect gilAt{inner.x + inner.width - gilWidth - 18, inner.y + inner.height - 22, gilWidth, 22};
        context.DrawText(balance.c_str(), gilAt, gilAt, gilProfile);
        if (auto* texture = context.Icons() ? context.Icons()->Item(0xFFFF) : nullptr)
            design.DrawTexture(texture, {gilAt.x - 22, gilAt.y + 3, 16, 16});
        const ui::rect countAt{inner.x, inner.y + inner.height - 22, std::max(0.0f, gilAt.x - inner.x - 32), 22};
        context.DrawText(counts.c_str(), countAt, countAt, this->text_[1]);
        std::string status;
        if (this->window_action_ != window_action::none && !this->pointer_down_)
            status = this->window_action_ == window_action::move ? "Directions: move | Confirm: save | Cancel: restore" : "Directions: resize | Confirm: save | Cancel: restore";
        else if (!this->character_.empty() && snapshot) status = Observed(snapshot->observed);
        else if (this->actions_ && !this->actions_->Status().empty()) status = this->actions_->Status();
        else if (!this->message_.empty()) status = this->message_;
        const ui::rect statusAt{inner.x, inner.y + inner.height - 48, inner.width, 22};
        if (!status.empty()) context.DrawText(status.c_str(), statusAt, statusAt, this->text_[1]);
        if (!this->position_locked_)
        {
            const auto grip = this->ResizeBounds();
            design.DrawTriangle(grip.x + 5, grip.y + 15, grip.x + 15, grip.y + 5, grip.x + 15, grip.y + 15,
                Contains(grip, this->hover_x_, this->hover_y_) || this->window_action_ == window_action::resize ? colors.accent : colors.muted);
        }
    }

    auto satchel_module::DrawDialog(const module_render_context& context) -> void
    {
        const auto bounds = this->DialogBounds(); const auto content = context.DrawTooltip(bounds);
        ui::config_controls controls{context.Design(), nullptr, context.NativeAssets()};
        if (this->config_text_) controls.SetTextSettings(*this->config_text_);
        if (this->ItemMenuOpen())
        {
            const auto& design = context.Design(); const auto& colors = design.GetTheme().colors;
            const auto title = this->dialog_ == dialog::actions ? this->action_name_ : this->dialog_ == dialog::destinations ? "Move to bag" : "Equipment slot";
            const ui::rect header{bounds.x + 12, bounds.y + 6, bounds.width - 24, 22};
            const auto fitted = ui::FitText(design, title, header.width, controls.Style(design.GetTheme().caption_text));
            controls.Text(fitted.c_str(), header, design.GetTheme().caption_text);
            const auto separator = [&](const float y) { design.FillRect({bounds.x + 10, y, bounds.width - 20, 1}, (colors.border & 0xFFFFFF) | 0x50000000); };
            separator(bounds.y + item_menu_header - 1);
            if (this->dialog_ == dialog::actions) { separator(this->PickBounds(5).y - 4); separator(this->PickBounds(6).y - 4); }
            const auto first = this->pick_page_ * picker_rows;
            for (size_t i = first; i < std::min(first + picker_rows, this->picks_.size()); ++i)
            {
                const auto at = this->PickBounds(i % picker_rows); const auto& pick = this->picks_[i];
                controls.NavigationItem(at, pick.label.c_str(), this->navigation_focus_ && this->pick_index_ == i,
                    Contains(at, this->hover_x_, this->hover_y_), pick.available);
            }
            if (this->navigation_focus_ && this->pick_index_ < this->picks_.size())
                design.DrawFocusFrame(this->PickBounds(this->pick_index_ % picker_rows), this->focus_style_);
            if (this->picks_.size() > picker_rows)
            {
                const auto pages = std::to_string(this->pick_page_ + 1) + " / " + std::to_string((this->picks_.size() + picker_rows - 1) / picker_rows);
                controls.Text(pages.c_str(), {bounds.x + 12, bounds.y + bounds.height - 24, bounds.width - 24, 20}, design.GetTheme().caption_text);
            }
            return;
        }
        if (this->dialog_ == dialog::characters)
        {
            const auto first = this->pick_page_ * picker_rows;
            for (size_t index = first; index < std::min(first + picker_rows, this->picks_.size()); ++index)
            {
                const auto at = this->PickBounds(index % picker_rows);
                controls.NavigationItem(at, this->picks_[index].label.c_str(), this->picks_[index].value == this->character_,
                    Contains(at, this->hover_x_, this->hover_y_));
            }
            if (this->navigation_focus_ && this->pick_index_ < this->picks_.size())
                context.Design().DrawFocusFrame(this->PickBounds(this->pick_index_ % picker_rows), this->focus_style_);
            if (this->picks_.size() > picker_rows)
            {
                const auto pages = "Page " + std::to_string(this->pick_page_ + 1) + "/" + std::to_string((this->picks_.size() + picker_rows - 1) / picker_rows);
                auto profile = this->text_[1]; profile.style.alignment = ui::text_alignment::center;
                const ui::rect at{bounds.x + 8, bounds.y + bounds.height - 26, bounds.width - 16, 20};
                context.DrawText(pages.c_str(), at, at, profile);
            }
            return;
        }
        const char* title = this->dialog_ == dialog::slips ? "Owned storage slips" :
            this->dialog_ == dialog::destinations ? "Move to bag" : this->dialog_ == dialog::equipment ? "Equipment slot" : this->dialog_ == dialog::drop ? "Drop this item?" :
            this->dialog_ == dialog::quantity ? "Quantity" : this->dialog_ == dialog::price ? "Bazaar price per item (0 removes)" : "Item actions";
        context.DrawText(title, {content.x, bounds.y + 9, content.width, 24}, content, this->text_[0]);
        if (this->dialog_ == dialog::quantity || this->dialog_ == dialog::price)
        {
            const auto maximum = this->dialog_ == dialog::price ? 99999999u : this->action_item_.count - (static_cast<satchel_action>(this->action_) == satchel_action::split ? 1 : 0);
            uint32_t value{}; const bool valid = Number(this->editor_.Editor().Text(), value) && value <= maximum && (this->dialog_ == dialog::price || value > 0);
            controls.TextInput(this->PickBounds(0), this->editor_, !valid, "Enter a number", "Up/Down adjusts");
            this->DrawButton(this->PickBounds(1), "Cancel", this->pick_index_ == 1, context);
            this->DrawButton(this->PickBounds(2), "Confirm", this->pick_index_ == 2, context);
            if (this->navigation_focus_) context.Design().DrawFocusFrame(this->PickBounds(this->pick_index_), this->focus_style_);
            return;
        }
        const auto first = this->pick_page_ * picker_rows;
        for (size_t index = first; index < std::min(first + picker_rows, this->picks_.size()); ++index)
        {
            const auto at = this->PickBounds(index % picker_rows); const auto& pick = this->picks_[index];
            auto text = this->text_[1]; text.style.alignment = ui::text_alignment::center;
            if (!pick.available) text = TintTextProfile(text, context.Design().GetTheme().colors.muted);
            context.DrawButton(at, pick.label.c_str(), this->pick_index_ == index || Contains(at, this->hover_x_, this->hover_y_), text);
        }
        if (this->navigation_focus_ && this->pick_index_ < this->picks_.size()) context.Design().DrawFocusFrame(this->PickBounds(this->pick_index_ % picker_rows), this->focus_style_);
        if (this->picks_.size() > picker_rows)
        {
            const std::string pages = "Page " + std::to_string(this->pick_page_ + 1) + "/" + std::to_string((this->picks_.size() + picker_rows - 1) / picker_rows) + "  |  Left/Right or wheel";
            const ui::rect at{content.x, bounds.y + bounds.height - 25, content.width, 20}; context.DrawText(pages.c_str(), at, at, this->text_[1]);
        }
    }

    auto satchel_module::RenderOverlay(const module_render_context& context) -> void
    {
        if (!this->opened_ || !this->state_.enabled || context.IsEditing()) return;
        if (this->dialog_ != dialog::none) { this->DrawDialog(context); return; }
        if (this->navigation_focus_ && this->InputFocused() && !this->editor_.Focused())
        {
            auto focus = this->FocusBounds();
            if (this->region_ == region::bags || this->region_ == region::slips) focus = Intersect(focus, this->SidebarBounds());
            if (focus.width > 0 && focus.height > 0) context.Design().DrawFocusFrame(this->state_.transform.Map(focus), this->focus_style_);
        }
        if (this->dragging_ && this->down_item_.id)
        {
            const auto transform = context.Design().Transform(this->state_.transform);
            if (auto* texture = context.Icons() ? context.Icons()->Item(this->down_item_.id) : nullptr)
                context.Design().DrawTexture(texture, {this->hover_x_ - this->icon_size_ * 0.5f, this->hover_y_ - this->icon_size_ * 0.5f, this->icon_size_, this->icon_size_}, 0xCCFFFFFF);
            return;
        }
        const bool showHelp = this->help_open_ || Contains(this->HelpBounds(), this->hover_x_, this->hover_y_) ||
            (this->navigation_focus_ && this->InputFocused() && this->region_ == region::help);
        const bool showCharacter = Contains(this->CharacterBounds(), this->hover_x_, this->hover_y_) ||
            (this->navigation_focus_ && this->InputFocused() && this->region_ == region::character);
        const bool showMinimize = Contains(this->MinimizeBounds(), this->hover_x_, this->hover_y_) ||
            (this->navigation_focus_ && this->InputFocused() && this->region_ == region::minimize);
        if (showHelp || showCharacter || showMinimize)
        {
            const auto* snapshot = this->Snapshot();
            const auto character = snapshot && !snapshot->name.empty() ? snapshot->name + (this->character_.empty() ? " (live)" : " (saved)") : "Choose character";
            const std::vector<std::string> hints = showMinimize ? std::vector<std::string>{this->minimized_ ? "Restore Satchel" : "Minimize to title bar"} :
                showHelp ? std::vector<std::string>{"Right-click: actions", "Double-click: transfer", "Drag to bag: move"} :
                std::vector<std::string>{"Character inventories", character};
            const auto& design = context.Design(); const auto& viewport = context.Viewport();
            auto profile = this->text_[1]; profile.style.alignment = ui::text_alignment::left;
            profile.offset_x = profile.offset_y = 0; profile.placement = 0;
            const float lineHeight = design.TextHeight(profile.style) + 4;
            float width = 0;
            for (const auto& hint : hints) width = std::max(width, design.MeasureText(hint.c_str(), profile.style));
            width = std::min(width + 26, std::max(80.0f, viewport.width - 16));
            const float height = lineHeight * std::size(hints) + 26;
            const auto anchor = this->state_.transform.Map(showMinimize ? this->MinimizeBounds() : showHelp ? this->HelpBounds() : this->CharacterBounds());
            const float x = std::clamp(anchor.x + anchor.width - width, viewport.x + 8,
                std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
            const float y = std::clamp(anchor.y + anchor.height + 6, viewport.y + 8,
                std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
            const auto content = context.DrawTooltip({x, y, width, height});
            float top = content.y;
            for (const auto& hint : hints)
            {
                context.DrawText(hint.c_str(), {content.x, top, content.width, lineHeight}, content, profile);
                top += lineHeight;
            }
            return;
        }
        if (this->minimized_ || !this->tooltips_ || this->editor_.Focused()) return;
        if (this->navigation_focus_ && this->InputFocused() && this->region_ == region::grid)
        { if (const auto* cell = this->CurrentCell()) this->DrawTooltip(cell->item, this->CellBounds(this->focus_cell_), context); }
        else if (const auto index = this->ItemAt(this->hover_x_, this->hover_y_); index && !this->cells_[*index].empty)
            this->DrawTooltip(this->cells_[*index].item, this->CellBounds(*index), context);
    }

    auto satchel_module::DrawTooltip(const satchel_item& item, const ui::rect& localAnchor, const module_render_context& context) -> void
    {
        const auto anchor = this->state_.transform.Map(localAnchor);
        const auto* resources = this->core_ ? this->core_->GetResourceManager() : nullptr;
        const auto* resource = resources ? resources->GetItemById(item.id) : nullptr;
        const auto& viewport = context.Viewport(); const auto& design = context.Design();
        const float width = std::min(365.0f, std::max(80.0f, viewport.width - 16));
        const float contentWidth = width - 26;
        auto title = this->text_[0]; title.style.alignment = ui::text_alignment::left; title.offset_x = title.offset_y = 0; title.placement = 0;
        auto body = this->text_[3]; body.style.alignment = ui::text_alignment::left; body.offset_x = body.offset_y = 0; body.placement = 0;
        auto caption = this->text_[1]; caption.style.alignment = ui::text_alignment::left; caption.offset_x = caption.offset_y = 0; caption.placement = 0;
        auto accent = TintTextProfile(body, design.GetTheme().colors.gold);
        struct line { std::string text; const module_text_profile* style; float height; };
        std::vector<line> lines;
        const auto append = [&](const std::string& value, const module_text_profile& style) {
            std::string current;
            const auto emit = [&] { if (!current.empty()) { lines.push_back({current, &style, design.TextHeight(style.style) + 4}); current.clear(); } };
            for (size_t start = 0; start < value.size();)
            {
                const auto end = value.find_first_of(" \r\n", start);
                const auto word = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
                if (!word.empty())
                {
                    const auto candidate = current.empty() ? word : current + " " + word;
                    if (!current.empty() && design.MeasureText(candidate.c_str(), style.style) > contentWidth) emit();
                    if (!current.empty()) current += ' ';
                    current += word;
                }
                if (end == std::string::npos) break;
                if (value[end] == '\n') emit();
                start = end + 1;
            }
            emit();
        };
        append(this->Name(item.id), title);
        std::string state;
        if (resource)
        {
            if (resource->Flags & 0x8000) state += "Rare  ";
            if ((resource->Flags & 0x4000) || item.extra[0] == 2 || item.extra[0] == 3) state += "Ex  ";
        }
        if (item.extra[0] == 2 || item.extra[0] == 3) state += "Augmented  ";
        if (item.equipped) state += "Equipped  ";
        if (item.price) state += "Bazaar: " + std::to_string(item.price) + " gil each";
        if (!state.empty()) append(state, accent);
        if (resource)
        {
            const auto* description = resource->Description[2] ? resource->Description[2] : resource->Description[0];
            if (description && *description) append(FromGameText(description), body);
            if (resource->Slots)
            {
                std::string level = "Lv. " + std::to_string(resource->Level);
                if (resource->ItemLevel) level += "  |  Item level " + std::to_string(resource->ItemLevel);
                if (resource->SuperiorLevel) level += "  |  Superior " + std::to_string(resource->SuperiorLevel);
                append(level, caption);
                constexpr const char* jobs[]{"", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG", "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"};
                std::string jobNames;
                for (size_t job = 1; job < std::size(jobs); ++job) if (resource->Jobs & (1u << job)) { if (!jobNames.empty()) jobNames += ' '; jobNames += jobs[job]; }
                if (!jobNames.empty()) append(jobNames, caption);
                std::string slots;
                for (size_t slot = 0; slot < equipment_slot_names.size(); ++slot)
                    if (resource->Slots & (1u << slot)) { if (!slots.empty()) slots += ", "; slots += equipment_slot_names[slot]; }
                if (!slots.empty()) append(slots, caption);
            }
            if (resource->MaxCharges && item.extra[0] == 1)
                append(resource->MaxCharges == 255 ? "Unlimited charges" : "Charges: " + std::to_string(item.extra[1]) + "/" + std::to_string(resource->MaxCharges), accent);
            if (resource->Type == 4 || resource->Type == 5) for (const auto& augment : Augments(item)) append(augment, accent);
        }
        if (this->slip_ >= 0) append("Stored on Slip " + std::to_string(this->slip_ + 1) + "  |  Read-only", caption);
        else
        {
            const std::string bag = item.bag < satchel_bag_names.size() ? satchel_bag_names[item.bag] : "Bag";
            append(bag + "  |  Slot " + std::to_string(item.slot) + "  |  Quantity " + std::to_string(item.count) + (this->ReadOnly() ? "  |  Read-only" : ""), caption);
        }
        append("Item #" + std::to_string(item.id) + (resource && resource->StackSize > 1 ? "  |  Stack size " + std::to_string(resource->StackSize) : ""), caption);
        if (!this->character_.empty()) if (const auto* snapshot = this->Snapshot()) append(Observed(snapshot->observed), caption);
        float height = 26;
        for (const auto& line : lines) height += line.height;
        height = std::min(height, std::max(60.0f, viewport.height - 16));
        float x = anchor.x + anchor.width + 10;
        if (x + width > viewport.x + viewport.width - 8) x = anchor.x - width - 10;
        x = std::clamp(x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8));
        const float y = std::clamp(anchor.y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8));
        const auto content = context.DrawTooltip({x, y, width, height});
        float top = content.y;
        for (const auto& line : lines)
        {
            if (top + line.height > content.y + content.height) break;
            context.DrawText(line.text.c_str(), {content.x, top, content.width, line.height}, content, *line.style);
            top += line.height;
        }
    }
}
