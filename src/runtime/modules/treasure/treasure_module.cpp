#include "treasure_module.hpp"
#include "chat/chat_encoding.hpp"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <limits>

namespace xiui
{
    namespace
    {
        constexpr const char* text_sections[]{"text.treasure.title", "text.treasure.item", "text.treasure.details", "text.treasure.controls"};
        auto Inside(const ui::rect& rect, float x, float y) -> bool
        { return x >= rect.x && y >= rect.y && x < rect.x + rect.width && y < rect.y + rect.height; }
    }
    auto treasure_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.title_text}, module_text_profile{theme.body_text}, module_text_profile{theme.caption_text}, module_text_profile{theme.caption_text}};
        this->text_[1].style.scale = this->text_[2].style.scale = this->text_[3].style.scale = 0.8f;
    }
    auto treasure_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; auto* source = this->source_; auto* core = this->core_; auto* sounds = this->sounds_;
        *this = {}; this->state_.bounds = bounds; this->source_ = source; this->core_ = core; this->sounds_ = sounds; this->ApplyTextDefaults(theme);
    }
    auto treasure_module::Load(const settings_store& store) -> void
    {
        this->EndInput(); this->forced_ = this->details_ = false;
        this->state_.enabled = store.ReadBool("treasure", "enabled", true);
        this->automatic_ = store.ReadBool("treasure", "automatic", true); this->expanded_ = store.ReadBool("treasure", "expanded", true);
        this->state_.bounds = store.ReadRect("layout.treasure", this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, 440.0f, 1000.0f);
        this->container_.opacity = std::clamp(store.ReadFloat("treasure", "opacity", 0.95f), 0.0f, 1.0f);
        this->container_.padding = std::clamp(store.ReadFloat("treasure", "padding", 10), 0.0f, 24.0f);
        this->selected_text_ = std::clamp(store.ReadInt("treasure", "selected_text", 0), 0, 3);
        for (size_t i = 0; i < this->text_.size(); ++i) this->text_[i] = LoadTextProfile(store, text_sections[i], this->text_[i]);
    }
    auto treasure_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("treasure", "enabled", this->state_.enabled); store.WriteBool("treasure", "automatic", this->automatic_);
        store.WriteBool("treasure", "expanded", this->expanded_); store.WriteRect("layout.treasure", this->state_.bounds);
        store.WriteFloat("treasure", "opacity", this->container_.opacity); store.WriteFloat("treasure", "padding", this->container_.padding);
        store.WriteInt("treasure", "selected_text", this->selected_text_);
        for (size_t i = 0; i < this->text_.size(); ++i) SaveTextProfile(store, text_sections[i], this->text_[i]);
    }
    auto treasure_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{{"General", "Native treasure items and explicit lot/pass decisions.", "general"}, {"Appearance", "Size and native window styling.", "appearance"}, {"Text", "Independent title, item, details and button profiles.", "text"}};
        return sections;
    }
    auto treasure_module::ConfigSectionContains(size_t section, size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        return section == (index >= selected_text ? 2 : index >= width ? 1 : 0);
    }
    auto treasure_module::Option(size_t index) -> module_option
    {
        static constexpr const char* profiles[]{"Title", "Items", "Details and timers", "Buttons"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show the Treasure Pool module.", &this->state_.enabled).Ordered(-50);
        case open: return module_option::Action("Open pool", "Show the pool even when empty.", "Open");
        case automatic: return module_option::Toggle("Show automatically", "Appear when the native treasure pool contains items.", &this->automatic_);
        case expanded: return module_option::Toggle("Expanded rows", "Show the winning player, lot and estimated countdown beneath each item.", &this->expanded_);
        case width: return ModuleWidthOption(this->state_);
        case opacity: return module_option::Slider("Opacity", "Native window opacity.", &this->container_.opacity, 0, 1, 0.05f);
        case padding: return module_option::Slider("Padding", "Space inside the native window.", &this->container_.padding, 0, 24, 1);
        case selected_text: return module_option::Element("Selected text", "Choose the text element to style.", &this->selected_text_, profiles, 4);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]);
        }
    }
    auto treasure_module::OnOptionChanged(size_t index) -> void
    { if (index == open) this->Open(); if (!this->state_.enabled) this->EndInput(); }
    auto treasure_module::ShouldRender(const hud_snapshot&) const -> bool
    { return this->state_.enabled && (this->forced_ || (this->automatic_ && this->source_ != nullptr && this->source_->Ready() && this->source_->Count() != 0)); }
    auto treasure_module::Update(IAshitaCore* core, bool available) -> void
    {
        this->core_ = core; this->available_ = available && this->state_.enabled;
        if (!this->available_ || this->source_ == nullptr || !this->source_->Ready() || this->source_->Count() == 0) this->EndInput();
    }
    auto treasure_module::Contains(float x, float y) const -> bool
    { return this->ShouldRender({}) && Inside(this->state_.bounds, x, y); }
    auto treasure_module::SoundTarget(float x, float y) const -> uint64_t
    {
        if (!this->available_ || !this->Contains(x, y)) return 0;
        for (const auto& control : this->controls_)
            if (control.available && Inside(control.bounds, x, y)) return (control.generation << 8) ^ (control.slot << 4) ^ (static_cast<uint64_t>(control.kind) + 1);
        return 0;
    }
    auto treasure_module::BeginBrowse(void) -> bool
    {
        if (!this->available_ || !this->ShouldRender({}) || this->source_->Count() == 0 || this->controls_.empty()) return false;
        this->browse_ = true; this->focus_ = 0;
        if (!this->controls_[0].available) this->MoveFocus(1);
        this->selected_ = this->controls_[this->focus_].slot;
        return true;
    }
    auto treasure_module::MoveFocus(int delta, bool vertical) -> void
    {
        if (this->focus_ >= this->controls_.size()) return;
        const auto current = this->controls_[this->focus_];
        if (vertical)
        {
            float nearest = std::numeric_limits<float>::max(), score = nearest;
            size_t next = this->focus_;
            for (size_t i = 0; i < this->controls_.size(); ++i)
            {
                const auto& candidate = this->controls_[i];
                const float distance = (candidate.bounds.y - current.bounds.y) * delta;
                if (!candidate.available || distance <= 0 || distance > nearest) continue;
                const float candidateScore = (candidate.kind == current.kind ? 0.0f : 10000.0f) +
                    std::abs(candidate.bounds.x + candidate.bounds.width / 2 - current.bounds.x - current.bounds.width / 2);
                if (distance < nearest || candidateScore < score) { nearest = distance; score = candidateScore; next = i; }
            }
            this->focus_ = next;
        }
        else
            for (size_t step = 1; step <= this->controls_.size(); ++step)
            {
                const auto next = static_cast<size_t>((static_cast<int>(this->focus_ + this->controls_.size()) + delta * static_cast<int>(step)) % static_cast<int>(this->controls_.size()));
                if (this->controls_[next].available && this->controls_[next].bounds.y == current.bounds.y) { this->focus_ = next; break; }
            }
        this->selected_ = std::min(this->controls_[this->focus_].slot, size_t{9});
    }
    auto treasure_module::Batch(bool pass) -> void
    {
        if (!this->available_ || this->source_ == nullptr) return;
        const auto batch = this->source_->CaptureBatch(pass, ::GetTickCount64());
        this->SubmitBatch(batch);
    }
    auto treasure_module::SubmitBatch(const std::span<const treasure_action> batch) -> void
    {
        if (!this->available_ || this->source_ == nullptr) return;
        this->source_->ActBatch(this->core_, batch, ::GetTickCount64());
    }
    auto treasure_module::Activate(const control& control, const std::vector<treasure_action>* batch) -> void
    {
        if (!control.available || !this->available_ || this->source_ == nullptr) return;
        if (this->sounds_ != nullptr) this->sounds_->Select(::GetTickCount64());
        this->selected_ = std::min(control.slot, size_t{9});
        switch (control.kind)
        {
        case action::lot:
        case action::pass:
            this->source_->Act(this->core_, control.slot, control.generation, control.kind == action::pass, ::GetTickCount64());
            break;
        case action::lot_all: case action::pass_all:
            if (batch != nullptr) this->SubmitBatch(*batch);
            else this->Batch(control.kind == action::pass_all);
            break;
        case action::expand:
            this->expanded_ = !this->expanded_;
            this->changed_ = true;
            break;
        case action::details:
            this->details_ = !this->details_;
            break;
        case action::close:
            this->forced_ = false;
            this->EndInput();
            break;
        }
    }
    auto treasure_module::HandleControllerAction(controller_action action, bool repeated) -> bool
    {
        if (!this->browse_) return false;
        if (action == controller_action::cancel) this->EndInput();
        else if (action == controller_action::confirm && !repeated && this->focus_ < this->controls_.size()) this->Activate(this->controls_[this->focus_]);
        else if (action == controller_action::actions && !repeated) this->details_ = !this->details_;
        else if (action == controller_action::up || action == controller_action::down) this->MoveFocus(action == controller_action::up ? -1 : 1, true);
        else if (action == controller_action::left || action == controller_action::previous_tab) this->MoveFocus(-1);
        else if (action == controller_action::right || action == controller_action::next_tab) this->MoveFocus(1);
        return true;
    }
    auto treasure_module::HandleMouse(uint32_t message, float x, float y) -> bool
    {
        this->mouse_x_ = x;
        this->mouse_y_ = y;
        if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK)
        {
            if (!this->Contains(x, y)) return false;
            this->EndInput();
            this->pointer_down_ = true;
            for (const auto& control : this->controls_)
                if (control.available && Inside(control.bounds, x, y))
                {
                    this->pressed_ = pointer_press{control};
                    if (control.kind == action::lot_all || control.kind == action::pass_all)
                        this->pressed_->batch = this->source_->CaptureBatch(control.kind == action::pass_all, ::GetTickCount64());
                    break;
                }
            return true;
        }
        if (message == WM_LBUTTONUP && this->pointer_down_)
        {
            const auto pressed = std::move(this->pressed_);
            this->pressed_.reset();
            this->pointer_down_ = false;
            if (pressed && this->ShouldRender({}) && Inside(pressed->target.bounds, x, y))
                this->Activate(pressed->target, &pressed->batch);
            return true;
        }
        return this->Contains(x, y) && message == WM_MOUSEWHEEL;
    }
    auto treasure_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const bool preview = context.IsEditing();
        std::array<treasure_entry, treasure_source::capacity> previewEntries{};
        if (preview)
        {
            for (size_t i = 0; i < 3; ++i)
            {
                auto& entry = previewEntries[i];
                entry.item = 4096 + static_cast<uint32_t>(i);
                entry.count = 1;
                entry.winning_lot = 450;
                entry.winner = "Party member";
            }
        }
        const auto& entries = !preview && this->source_ != nullptr ? this->source_->Entries() : previewEntries;
        const auto count = static_cast<size_t>(std::count_if(entries.begin(), entries.end(), [](const auto& item) { return item.item != 0; }));
        const auto& design = context.Design(); const auto now = ::GetTickCount64();
        const float titleHeight = std::max(28.0f, design.TextHeight(this->text_[0].style) + 6);
        const float detailLine = std::max(18.0f, design.TextHeight(this->text_[2].style) + 4);
        const float buttonHeight = std::max(26.0f, design.TextHeight(this->text_[3].style) + 8);
        const float line = std::max({28.0f, buttonHeight, design.TextHeight(this->text_[1].style) + 4});
        const float rowHeight = line + (this->expanded_ ? detailLine : 0) + 6;
        if (count != 0 && entries[this->selected_].item == 0)
            this->selected_ = static_cast<size_t>(std::find_if(entries.begin(), entries.end(), [](const auto& item) { return item.item != 0; }) - entries.begin());
        const size_t historyLines = count != 0 ? std::min(size_t{4}, entries[this->selected_].history.size()) : 0;
        const bool error = !preview && this->source_ != nullptr && !this->source_->Error().empty();
        const float detailHeight = this->details_ && count != 0 ? 6 + detailLine * (1 + historyLines + (error ? 1 : 0)) : 0;
        this->state_.bounds.height = this->container_.padding * 2 + titleHeight + 4 + std::max(size_t{1}, count) * rowHeight + buttonHeight + detailHeight;
        const auto content = context.DrawContainer(this->state_.bounds, this->container_);
        const auto draw = [&](const std::string& value, const ui::rect& area, size_t profile) { context.DrawText(value.c_str(), area, area, this->text_[profile]); };
        draw("Treasure Pool (" + std::to_string(count) + ")", {content.x, content.y, content.width, titleHeight}, 0);
        std::optional<control> focused;
        if (this->browse_ && this->focus_ < this->controls_.size()) focused = this->controls_[this->focus_];
        this->controls_.clear();
        const auto button = [&](const char* label, ui::rect bounds, action kind, size_t slot, uint64_t generation, bool available) {
            available = available && this->available_ && !preview;
            auto profile = available ? this->text_[3] : TintTextProfile(this->text_[3], 0xFF888888);
            context.DrawButton(bounds, label, available && Inside(bounds, this->mouse_x_, this->mouse_y_), profile);
            this->controls_.push_back({bounds, kind, slot, generation, available});
        };
        float y = content.y + titleHeight + 4;
        if (count == 0) draw("The treasure pool is empty.", {content.x, y, content.width, rowHeight}, 2);
        bool lotAll{}, passAll{};
        for (size_t slot = 0; slot < entries.size(); ++slot)
        {
            const auto& entry = entries[slot]; if (entry.item == 0) continue;
            std::string name = "Item " + std::to_string(entry.item);
            if (auto* resources = this->core_ != nullptr ? this->core_->GetResourceManager() : nullptr)
                if (const auto* item = resources->GetItemById(entry.item); item != nullptr && item->Name[2] != nullptr) name = FromGameText(item->Name[2]);
            if (entry.count > 1) name += " x" + std::to_string(entry.count);
            this->controls_.push_back({{content.x, y, content.width - 112, rowHeight}, action::details, slot, entry.generation, !preview && this->available_});
            if (context.Icons() != nullptr)
                if (auto* icon = context.Icons()->Item(entry.item)) design.DrawTexture(icon, {content.x, y, 28, 28});
            draw(name, {content.x + 34, y, content.width - 146, line}, 1);
            const bool canLot = this->source_ != nullptr && this->source_->Plan(slot, entry.generation, false, now).has_value();
            const bool canPass = this->source_ != nullptr && this->source_->Plan(slot, entry.generation, true, now).has_value();
            lotAll |= canLot; passAll |= canPass;
            button("Lot", {content.x + content.width - 106, y, 48, buttonHeight}, action::lot, slot, entry.generation, canLot);
            button("Pass", {content.x + content.width - 54, y, 54, buttonHeight}, action::pass, slot, entry.generation, canPass);
            if (this->expanded_)
            {
                auto status = entry.lot == 65535 ? std::string{"Passed"} : entry.lot != 0 ? "Your lot: " + std::to_string(entry.lot) : entry.pending_until > now ? "Awaiting response" : "No decision";
                if (entry.winning_lot != 0) status += " | " + entry.winner + ": " + std::to_string(entry.winning_lot);
                if (entry.expires > now) { const auto seconds = (entry.expires - now + 999) / 1000; char time[32]{}; std::snprintf(time, sizeof(time), " | ~%llu:%02llu", seconds / 60, seconds % 60); status += time; }
                else status += " | Time unknown";
                draw(status, {content.x + 34, y + line, content.width - 34, detailLine}, 2);
            }
            y += rowHeight;
        }
        if (count == 0) y += rowHeight;
        const float buttonWidth = (content.width - 12) / 4;
        button("Lot all", {content.x, y, buttonWidth, buttonHeight}, action::lot_all, this->selected_, 0, lotAll);
        button("Pass all", {content.x + buttonWidth + 4, y, buttonWidth, buttonHeight}, action::pass_all, this->selected_, 0, passAll);
        button(this->expanded_ ? "Collapse" : "Expand", {content.x + (buttonWidth + 4) * 2, y, buttonWidth, buttonHeight}, action::expand, this->selected_, 0, true);
        button(count == 0 ? "Close" : "Details", {content.x + (buttonWidth + 4) * 3, y, buttonWidth, buttonHeight}, count == 0 ? action::close : action::details, this->selected_, 0, true);
        if (this->details_ && count != 0)
        {
            y += buttonHeight + 6; draw("Recent lots / passes (countdowns are estimates)", {content.x, y, content.width, detailLine}, 2);
            const auto& history = entries[this->selected_].history;
            for (size_t i = history.size() - historyLines; i < history.size(); ++i) { y += detailLine; draw(history[i], {content.x, y, content.width, detailLine}, 2); }
            if (error) draw(this->source_->Error(), {content.x, y + detailLine, content.width, detailLine}, 2);
        }
        if (this->browse_)
        {
            this->focus_ = std::min(this->focus_, this->controls_.size() - 1);
            if (focused)
                for (size_t i = 0; i < this->controls_.size(); ++i)
                    if (this->controls_[i].kind == focused->kind && this->controls_[i].slot == focused->slot && this->controls_[i].generation == focused->generation) { this->focus_ = i; break; }
            if (!this->controls_[this->focus_].available) this->MoveFocus(1);
            design.DrawFocusFrame(this->controls_[this->focus_].bounds, this->focus_style_);
        }
    }
}
