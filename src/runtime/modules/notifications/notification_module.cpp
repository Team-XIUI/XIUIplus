#include "notification_module.hpp"
#include "settings/text_style_settings.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    auto PostSamples(xiui::notification_center& center, const uint64_t now) -> void
    {
        center.Post({xiui::notification_kind::experience, 0, 125, "XP"}, now);
        center.Post({xiui::notification_kind::item, 4096, 2, "Fire crystal"}, now);
        center.Post({xiui::notification_kind::skill, 3, 0.3, "Sword"}, now);
        center.Post({xiui::notification_kind::party_invite, 0, 0, "Party Invitation"}, now);
        center.Post({xiui::notification_kind::trade_invite, 0, 0, "Trade Request"}, now);
        center.Post({xiui::notification_kind::key_item, 0, 0, "Key Item: Limit breaker"}, now);
        center.Post({xiui::notification_kind::gil, 65535, 250, "gil"}, now);
        center.Post({xiui::notification_kind::treasure, 4096, 1, "Treasure: Fire crystal"}, now);
    }
}

namespace xiui
{
    auto notification_module::ResetDefaults(const ui::theme&) -> void
    {
        const auto bounds = this->state_.bounds;
        *this = notification_module{};
        this->state_.bounds = bounds;
    }

    auto notification_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("notifications", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.notifications", this->state_.bounds);
        this->screen_anchor_ = store.ReadBool("notifications", "screen_anchor", store.ReadBool("notifications", "follow_player", true));
        this->offset_x_ = std::clamp(store.ReadFloat("notifications", "offset_x", 0.0f), -500.0f, 500.0f);
        this->offset_y_ = std::clamp(store.ReadFloat("notifications", "offset_y", 0.0f), -400.0f, 200.0f);
        if (store.ReadInt("notifications", "anchor_version", 0) == 0 && this->offset_y_ == -48.0f)
            this->offset_y_ = 0.0f;
        this->style_.duration = std::clamp(store.ReadFloat("notifications", "duration", 3.2f), 1.0f, 8.0f);
        this->style_.rise = std::clamp(store.ReadFloat("notifications", "rise", 36.0f), 0.0f, 120.0f);
        auto text = InheritGlobalTextStyle(store, this->style_);
        text.scale = std::clamp(store.ReadFloat("notifications", "text_scale", text.scale), 0.5f, ui::text_max_scale);
        text.font = ui::FontIndex(store.ReadInt("notifications", "font", text.font));
        static_cast<ui::text_style&>(this->style_) = LoadTextStyle(store, "text.notifications", text);
        this->style_.visible_count = std::clamp(store.ReadFloat("notifications", "visible_count", 5.0f), 1.0f, 8.0f);
        const float spacing = store.ReadFloat("notifications", "row_spacing", 4.0f);
        this->style_.row_spacing = std::clamp(std::isfinite(spacing) ? spacing : 4.0f, 0.0f, 64.0f);
        this->style_.items = store.ReadBool("notifications", "items", true);
        this->style_.experience = store.ReadBool("notifications", "experience", true);
        this->style_.skills = store.ReadBool("notifications", "skills", true);
        this->style_.party_invites = store.ReadBool("notifications", "party_invites", true);
        this->style_.trade_invites = store.ReadBool("notifications", "trade_invites", true);
        this->style_.key_items = store.ReadBool("notifications", "key_items", this->style_.items);
        this->style_.gil = store.ReadBool("notifications", "gil", this->style_.items);
        this->style_.treasure = store.ReadBool("notifications", "treasure", this->style_.items);
        this->style_.background = store.ReadBool("notifications", "container", false);
        this->anchor_ = std::clamp(store.ReadInt("notifications", "anchor_direction", 1), 0, 1);
        this->style_.item_color = store.ReadArgb("notifications", "item_color", this->style_.item_color);
        this->style_.experience_color = store.ReadArgb("notifications", "experience_color", this->style_.experience_color);
        this->style_.skill_color = store.ReadArgb("notifications", "skill_color", this->style_.skill_color);
        this->style_.party_color = store.ReadArgb("notifications", "party_color", this->style_.item_color);
        this->style_.trade_color = store.ReadArgb("notifications", "trade_color", this->style_.item_color);
        this->style_.key_item_color = store.ReadArgb("notifications", "key_item_color", this->style_.item_color);
        this->style_.gil_color = store.ReadArgb("notifications", "gil_color", this->style_.item_color);
        this->style_.treasure_color = store.ReadArgb("notifications", "treasure_color", this->style_.item_color);
    }

    auto notification_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("notifications", "enabled", this->state_.enabled);
        store.WriteRect("layout.notifications", this->state_.bounds);
        store.WriteBool("notifications", "screen_anchor", this->screen_anchor_);
        store.WriteInt("notifications", "anchor_version", 2);
        store.WriteFloat("notifications", "offset_x", this->offset_x_);
        store.WriteFloat("notifications", "offset_y", this->offset_y_);
        store.WriteFloat("notifications", "duration", this->style_.duration);
        store.WriteFloat("notifications", "rise", this->style_.rise);
        store.WriteFloat("notifications", "text_scale", this->style_.scale);
        store.WriteInt("notifications", "font", this->style_.font);
        SaveTextStyle(store, "text.notifications", this->style_);
        store.WriteFloat("notifications", "visible_count", this->style_.visible_count);
        store.WriteFloat("notifications", "row_spacing", this->style_.row_spacing);
        store.WriteBool("notifications", "items", this->style_.items);
        store.WriteBool("notifications", "experience", this->style_.experience);
        store.WriteBool("notifications", "skills", this->style_.skills);
        store.WriteBool("notifications", "party_invites", this->style_.party_invites);
        store.WriteBool("notifications", "trade_invites", this->style_.trade_invites);
        store.WriteBool("notifications", "key_items", this->style_.key_items);
        store.WriteBool("notifications", "gil", this->style_.gil);
        store.WriteBool("notifications", "treasure", this->style_.treasure);
        store.WriteBool("notifications", "container", this->style_.background);
        store.WriteInt("notifications", "anchor_direction", this->anchor_);
        store.WriteArgb("notifications", "item_color", this->style_.item_color);
        store.WriteArgb("notifications", "experience_color", this->style_.experience_color);
        store.WriteArgb("notifications", "skill_color", this->style_.skill_color);
        store.WriteArgb("notifications", "party_color", this->style_.party_color);
        store.WriteArgb("notifications", "trade_color", this->style_.trade_color);
        store.WriteArgb("notifications", "key_item_color", this->style_.key_item_color);
        store.WriteArgb("notifications", "gil_color", this->style_.gil_color);
        store.WriteArgb("notifications", "treasure_color", this->style_.treasure_color);
    }

    auto notification_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* anchors[]{"Anchored left", "Anchored right"};
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show the notification types selected under Events.", &this->state_.enabled);
        case 1: return module_option::Toggle("Screen anchor", "Stable position left of screen center.", &this->screen_anchor_);
        case 2: return module_option::Toggle("Items", "New items across all bags.", &this->style_.items);
        case 3: return module_option::Toggle("Experience", "XP and limit point gains.", &this->style_.experience);
        case 4: return module_option::Toggle("Skill gains", "Combat and magic skill ups.", &this->style_.skills);
        case 5: return module_option::Slider("Spacing", "Gap between notification rows in pixels, including edit previews.", &this->style_.row_spacing, 0.0f, 64.0f, 1.0f, module_option_group::appearance);
        case 6: return module_option::Slider("Duration", "Seconds before regular notifications disappear. Party invites stay until answered or their 90-second timer expires.", &this->style_.duration, 1.0f, 8.0f, 0.1f, module_option_group::effects);
        case 7: return module_option::Slider("Visible rows", "Maximum visible notifications.", &this->style_.visible_count, 1.0f, 8.0f, 1.0f, module_option_group::appearance);
        case 8: return module_option::Slider("Rise", "Upward travel in pixels.", &this->style_.rise, 0.0f, 120.0f, 1.0f, module_option_group::effects);
        case 9: return TextSizeOption("Text size (px)", "Notification font size in pixels.", &this->style_.scale);
        case 10: return ModuleWidthOption(this->state_);
        case 11: return module_option::Color("XP color", "Experience text.", &this->style_.experience_color, module_option_group::colors);
        case 12: return module_option::Color("Item color", "Item gain text.", &this->style_.item_color, module_option_group::colors);
        case 13: return module_option::Color("Skill color", "Skill gain text.", &this->style_.skill_color, module_option_group::colors);
        case 14: return module_option::Toggle("Container background", "Native themed frame and background, fading away from the text's anchored edge.", &this->style_.background, module_option_group::appearance);
        case 15: return module_option::Choice("Anchored direction", "Right: enter from the right and fade the container left. Left: enter from the left and fade the container right.", &this->anchor_, anchors, 2, module_option_group::appearance);
        case 16: return module_option::Toggle("Party / alliance invites", "Keep incoming invitations visible with a countdown until answered or expired.", &this->style_.party_invites);
        case 17: return module_option::Toggle("Trade requests", "Show incoming requests to trade.", &this->style_.trade_invites);
        case 18: return module_option::Toggle("Key items", "Show newly obtained key items.", &this->style_.key_items);
        case 19: return module_option::Toggle("Gil", "Show positive gil gains after inventory loading completes.", &this->style_.gil);
        case 20: return module_option::Toggle("Treasure pool", "Show new items that remain available in the treasure pool.", &this->style_.treasure);
        case 21: return module_option::Color("Party invite color", "Party and alliance invitation text.", &this->style_.party_color, module_option_group::colors);
        case 22: return module_option::Color("Trade request color", "Incoming trade request text.", &this->style_.trade_color, module_option_group::colors);
        case 23: return module_option::Color("Key item color", "New key-item text.", &this->style_.key_item_color, module_option_group::colors);
        case 24: return module_option::Color("Gil color", "Gil-gain text.", &this->style_.gil_color, module_option_group::colors);
        case 25: return module_option::Color("Treasure color", "New treasure-pool item text.", &this->style_.treasure_color, module_option_group::colors);
        case 26: return module_option::Dropdown("Font", "Font face for notification text.", &this->style_.font, ui::font_names, ui::font_count);
        default: return {};
        }
    }

    auto notification_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Enable notifications; each event type has its own switch."},
            {"Events", "Choose item, progression, skill, invitation, currency and treasure notifications independently."},
            {"Layout", "Screen anchor, growth direction, width, visible rows and spacing."},
            {"Appearance", "Optional fading native container, text size and separate colors for each event type."},
            {"Animation", "Notification lifetime and upward movement."}};
        return sections;
    }

    auto notification_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (section >= this->ConfigSections().size() || index >= this->OptionCount()) return false;
        if (index == 0) return section == 0;
        if ((index >= 2 && index <= 4) || (index >= 16 && index <= 20)) return section == 1;
        if (index == 1 || index == 5 || index == 7 || index == 10 || index == 15) return section == 2;
        if (index == 6 || index == 8) return section == 4;
        return section == 3;
    }

    auto notification_module::ShouldRender(const hud_snapshot&) const -> bool
    {
        return this->state_.enabled && std::any_of(this->center_.Entries().begin(), this->center_.Entries().end(),
            [this](const auto& entry) { return notification_renderer::KindEnabled(entry.event.kind, this->style_); });
    }

    auto notification_module::Demo(const uint64_t now) -> void
    {
        PostSamples(this->center_, now);
    }

    auto notification_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const uint64_t now = ::GetTickCount64();
        auto area = this->state_.bounds;
        auto style = this->style_;
        style.alignment = this->anchor_ == 0 ? ui::text_alignment::left : ui::text_alignment::right;
        if (this->screen_anchor_)
        {
            style.center_vertically = true;
        }
        if (context.IsEditing())
        {
            notification_center preview;
            PostSamples(preview, now > 400 ? now - 400 : 0);
            area.y += 78.0f;
            notification_renderer::Draw(context.Design(), preview, area, style, now, context.Icons(), &context);
            return;
        }
        if (this->screen_anchor_)
        {
            const auto& viewport = context.Viewport();
            if (viewport.width <= 0.0f || viewport.height <= 0.0f)
                return;
            area = notification_renderer::ScreenBounds(viewport, area, this->offset_x_, this->offset_y_, style.alignment);
        }
        else
            area.y += 78.0f;
        notification_renderer::Draw(context.Design(), this->center_, area, style, now, context.Icons(), &context);
    }
} // namespace xiui
