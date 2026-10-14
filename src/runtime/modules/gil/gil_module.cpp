#include "gil_module.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    auto FormatGil(const int64_t amount, const bool sign) -> std::string
    {
        const auto magnitude = amount < 0 ? uint64_t{0} - static_cast<uint64_t>(amount) : static_cast<uint64_t>(amount);
        auto number = std::to_string(magnitude);
        for (size_t at = number.size(); at > 3;) { at -= 3; number.insert(at, ","); }
        return (amount < 0 ? "-" : sign ? "+" : "") + number;
    }

    auto gil_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {module_text_profile{theme.meter_text}, module_text_profile{theme.caption_text}};
        this->text_[0].style.scale = 0.85f;
        this->text_[1].style.scale = 0.72f;
    }

    auto gil_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds;
        auto* source = this->source_;
        *this = {};
        this->state_.bounds = bounds;
        this->source_ = source;
        this->ApplyTextDefaults(theme);
    }

    auto gil_module::Load(const settings_store& store) -> void
    {
        this->state_.bounds = store.ReadRect("layout.gil", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", &this->state_.enabled}, {"hide_menu", &this->hide_menu_},
            {"tracking", &this->tracking_}, {"icon", &this->icon_}, {"icon_right", &this->icon_right_},
            {"right_align", &this->right_align_}, {"background", &this->background_}})
            *value = store.ReadBool("gil", key, *value);
        this->mode_ = std::clamp(store.ReadInt("gil", "tracking_mode", 0), 0, 1);
        this->selected_text_ = std::clamp(store.ReadInt("gil", "selected_text", 0), 0, 1);
        const auto number = [&](const char* key, float fallback, float minimum, float maximum) {
            const auto value = store.ReadFloat("gil", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->icon_size_ = number("icon_size", 24, 12, 64);
        this->spacing_ = number("line_spacing", 2, 0, 24);
        this->container_.opacity = number("opacity", 1, 0, 1);
        this->container_.padding = number("padding", 6, 0, 24);
        this->positive_ = store.ReadArgb("gil", "positive", this->positive_);
        this->negative_ = store.ReadArgb("gil", "negative", this->negative_);
        this->text_[0] = LoadTextProfile(store, "text.gil.balance", this->text_[0]);
        this->text_[1] = LoadTextProfile(store, "text.gil.tracking", this->text_[1]);
        if (this->source_ != nullptr) this->source_->LoadSession(store);
        this->cached_mode_ = -1;
    }

    auto gil_module::Save(const settings_store& store) const -> void
    {
        store.WriteRect("layout.gil", this->state_.bounds);
        for (const auto& [key, value] : {std::pair{"enabled", this->state_.enabled}, {"hide_menu", this->hide_menu_},
            {"tracking", this->tracking_}, {"icon", this->icon_}, {"icon_right", this->icon_right_},
            {"right_align", this->right_align_}, {"background", this->background_}})
            store.WriteBool("gil", key, value);
        store.WriteInt("gil", "tracking_mode", this->mode_);
        store.WriteInt("gil", "selected_text", this->selected_text_);
        store.WriteFloat("gil", "icon_size", this->icon_size_);
        store.WriteFloat("gil", "line_spacing", this->spacing_);
        store.WriteFloat("gil", "opacity", this->container_.opacity);
        store.WriteFloat("gil", "padding", this->container_.padding);
        store.WriteArgb("gil", "positive", this->positive_);
        store.WriteArgb("gil", "negative", this->negative_);
        SaveTextProfile(store, "text.gil.balance", this->text_[0]);
        SaveTextProfile(store, "text.gil.tracking", this->text_[1]);
        if (this->source_ != nullptr) this->source_->SaveSession(store);
    }

    auto gil_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Current gil and optional session earnings, including gil spent."},
            {"Layout", "Native gil icon, alignment and line spacing."},
            {"Appearance", "Optional native DAT container and earnings colors."},
            {"Text", "Independent balance and tracking styles, effects and offsets."}};
        return sections;
    }

    auto gil_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        return section == (index <= reset ? size_t{0} : index <= spacing ? size_t{1} : index <= negative ? size_t{2} : size_t{3});
    }

    auto gil_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* modes[]{"Session net", "Gil per hour"};
        static constexpr const char* profiles[]{"Gil balance", "Session tracking"};
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Show the gil tracker.", &this->state_.enabled);
        case hide_menu: return module_option::Toggle("Hide with menus", "Hide the tracker while a native menu is open.", &this->hide_menu_);
        case tracking: return module_option::Toggle("Show tracking", "Show gil gained or spent since login or the last reset.", &this->tracking_);
        case mode: return module_option::Choice("Tracking display", "Net gil change, or the hourly rate for the entire session.", &this->mode_, modes, 2);
        case reset:
        {
            auto option = module_option::Action("Reset tracking", "Start a new session at your current balance.", "Reset");
            option.available = this->source_ != nullptr && this->source_->Session().ready;
            return option;
        }
        case icon: return module_option::Toggle("Gil icon", "Use the game's original gil item icon from its DAT resources.", &this->icon_);
        case icon_right: return module_option::Toggle("Icon on right", "Place the gil icon to the right of the balance.", &this->icon_right_);
        case right_align: return module_option::Toggle("Right align", "Anchor the balance and tracking to the right edge as their lengths change.", &this->right_align_);
        case icon_size: return module_option::Slider("Icon size", "Gil icon size in pixels.", &this->icon_size_, 12, 64, 1);
        case spacing: return module_option::Slider("Line spacing", "Pixels between the balance and tracking lines.", &this->spacing_, 0, 24, 1);
        case background: return module_option::Toggle("Container", "Use the shared native Window Type container.", &this->background_, module_option_group::appearance);
        case opacity: return module_option::Slider("Container opacity", "Opacity of the native container and dark backing.", &this->container_.opacity, 0, 1, 0.05f, module_option_group::appearance);
        case padding: return module_option::Slider("Container padding", "Space inside the native frame.", &this->container_.padding, 0, 24, 1, module_option_group::appearance);
        case positive: return module_option::Color("Positive change", "Tint for net earnings; retains the tracking text gradient and effects.", &this->positive_, module_option_group::colors);
        case negative: return module_option::Color("Negative change", "Tint for net spending; retains the tracking text gradient and effects.", &this->negative_, module_option_group::colors);
        case selected_text: return module_option::Element("Selected text", "Style each line independently, including X/Y offsets.", &this->selected_text_, profiles, 2, module_option_group::text);
        default: return TextProfileOption(index - base_options, this->text_[this->selected_text_]);
        }
    }

    auto gil_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == reset && this->source_ != nullptr) this->source_->ResetSession(::GetTickCount64());
        this->cached_mode_ = -1;
    }

    auto gil_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->state_.enabled && this->source_ != nullptr && this->source_->Session().ready &&
            (!this->hide_menu_ || !this->source_->MenuOpen());
    }

    auto gil_module::UpdateText(const uint64_t now, const bool preview) -> void
    {
        const bool live = this->source_ != nullptr && this->source_->Session().ready;
        const auto balance = live ? this->source_->Session().balance : preview ? 1234567u : 0u;
        const auto net = live ? this->source_->Session().Net() : preview ? int64_t{12500} : 0;
        if (balance != this->cached_balance_)
        {
            this->balance_text_ = FormatGil(balance);
            this->cached_balance_ = balance;
        }
        if (net != this->cached_net_ || this->cached_mode_ != this->mode_ || (this->mode_ == 1 && now >= this->next_rate_))
        {
            const auto value = this->mode_ == 0 ? net : live ? this->source_->Session().PerHour(now) : preview ? int64_t{25000} : 0;
            this->tracking_text_ = FormatGil(value, true) + (this->mode_ == 1 ? "/hr" : "");
            this->cached_net_ = net;
            this->cached_mode_ = this->mode_;
            this->next_rate_ = now + 3000;
        }
    }

    auto gil_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        this->UpdateText(::GetTickCount64(), context.IsEditing());
        const auto& design = context.Design();
        auto amountStyle = this->text_[0];
        auto trackingStyle = TintTextProfile(this->text_[1], this->cached_net_ < 0 ? this->negative_ : this->positive_);
        if (this->right_align_)
        {
            amountStyle.style.alignment = ui::text_alignment::right;
            trackingStyle.style.alignment = ui::text_alignment::right;
        }
        const float textWidth = std::ceil(std::max(design.MeasureText(this->balance_text_.c_str(), amountStyle.style),
            this->tracking_ ? design.MeasureText(this->tracking_text_.c_str(), trackingStyle.style) : 0.0f)) + 4;
        const float amountHeight = std::max(16.0f, context.Design().TextHeight(amountStyle.style) + 4);
        const float trackingHeight = std::max(14.0f, context.Design().TextHeight(trackingStyle.style) + 4);
        const float iconWidth = this->icon_ ? this->icon_size_ + 4 : 0;
        const float rowHeight = std::max(amountHeight, this->icon_ ? this->icon_size_ : 0.0f);
        const float inset = this->background_ ? this->container_.edge_size + this->container_.padding : 0;
        const float width = textWidth + iconWidth + inset * 2;
        // A right-aligned tracker keeps its right edge fixed as digits change.
        if (this->right_align_) this->state_.bounds.x += this->state_.bounds.width - width;
        this->state_.bounds.width = width;
        this->state_.bounds.height = inset * 2 + rowHeight + (this->tracking_ ? this->spacing_ + trackingHeight : 0);
        if (this->background_) context.DrawContainer(this->state_.bounds, this->container_);
        const float x = this->state_.bounds.x + inset, y = this->state_.bounds.y + inset;
        const ui::rect amount{x + (this->icon_right_ ? 0 : iconWidth), y + (rowHeight - amountHeight) * 0.5f, textWidth, amountHeight};
        context.DrawText(this->balance_text_.c_str(), amount, amount, amountStyle);
        if (this->icon_ && context.Icons() != nullptr)
            if (auto* texture = context.Icons()->Item(65535))
                design.DrawTexture(texture, {x + (this->icon_right_ ? textWidth + 4 : 0), y + (rowHeight - this->icon_size_) * 0.5f, this->icon_size_, this->icon_size_}, 0xFFFFFFFF);
        if (this->tracking_)
        {
            const ui::rect at{amount.x, y + rowHeight + this->spacing_, textWidth, trackingHeight};
            context.DrawText(this->tracking_text_.c_str(), at, at, trackingStyle);
        }
    }
}
