#include "localization/localization.hpp"
#include "ui/text_fit.hpp"
#include "ui/game_icon_cache.hpp"
#include "actionbars_module.hpp"
#include "config/config_controls.hpp"
#include "ui/recast_icons.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>

namespace xiui
{
    namespace
    {
        constexpr float rail = 28, trailing = 24, crossbar_gap = 32, caption_height = 20, bank_header = 32;
        constexpr float pi = 3.14159265359f;
        auto Contains(const ui::rect& bounds, const float x, const float y) -> bool
        { return bounds.width > 0 && bounds.height > 0 && x >= bounds.x && y >= bounds.y && x < bounds.x + bounds.width && y < bounds.y + bounds.height; }

        auto Padding(const actionbar_presentation& bar, const ui::container_style& container) -> float
        { return bar.background ? container.padding + 3 : 2; }
        auto DiamondWidth(const actionbar_presentation& bar) -> float { return bar.size * 3 + bar.gap * 2; }
        auto LabelHeight(const actionbar_presentation& bar) -> float { return bar.labels ? (bar.label_wrap ? 32.0f : 16.0f) : 0.0f; }
        // Top-to-bottom slot offset; -1 keeps an empty center row for button prompts.
        auto DiamondSpan(const actionbar_presentation& bar) -> float
        {
            const float row = bar.size + (bar.gap_y < 0 ? bar.gap : bar.gap_y) + LabelHeight(bar);
            return bar.diamond_gap < 0 ? row * 2 : bar.size + LabelHeight(bar) + bar.diamond_gap;
        }
        auto DiamondHeight(const actionbar_presentation& bar) -> float { return DiamondSpan(bar) + bar.size + LabelHeight(bar); }
        auto DiamondGap(const actionbar_presentation& bar) -> float { return bar.diamond_spacing < 0 ? std::max(8.0f, bar.gap * 2 + 4) : bar.diamond_spacing; }
        auto GroupWidth(const actionbar_presentation& bar) -> float { return DiamondWidth(bar) * 2 + DiamondGap(bar); }
        auto GridHeight(const actionbar_presentation& bar) -> float
        {
            const auto columns = static_cast<size_t>(std::clamp(bar.columns, 1.0f, 12.0f)), slots = static_cast<size_t>(std::clamp(bar.slots, 1.0f, 12.0f));
            const auto rows = (slots + columns - 1) / columns;
            return static_cast<float>(rows) * (bar.size + (bar.labels ? (bar.label_wrap ? 32 : 16) : 0)) + static_cast<float>(rows - 1) * (bar.gap_y < 0 ? bar.gap : bar.gap_y);
        }
        auto Alpha(const D3DCOLOR color, const float opacity) -> D3DCOLOR
        { return (color & 0x00FFFFFF) | (static_cast<D3DCOLOR>(static_cast<float>(color >> 24) * std::clamp(opacity, 0.0f, 1.0f)) << 24); }
        auto Multiply(const D3DCOLOR first, const D3DCOLOR second) -> D3DCOLOR
        {
            D3DCOLOR result{};
            for (uint32_t shift = 0; shift < 32; shift += 8)
                result |= ((((first >> shift) & 0xFF) * ((second >> shift) & 0xFF) + 127) / 255) << shift;
            return result;
        }
        auto RoundedBorder(const ui::design_system& design, const ui::rect& bounds, const float radius, const float width, const D3DCOLOR color) -> void
        {
            if (width <= 0 || (color >> 24) == 0) return;
            if (radius <= 0) { design.BorderRect(bounds, color, width); return; }
            ui::focus_frame_style frame{}; frame.color = color; frame.thickness = width; frame.radius = radius;
            frame.padding = 0; frame.glow = frame.shadow = false;
            design.DrawFocusFrame(bounds, frame);
        }
        auto CompactBinding(std::string text) -> std::string
        {
            for (const auto& [from, to] : {std::pair{"Ctrl+", "C-"}, {"Alt+", "A-"}, {"Shift+", "S-"}, {"Num ", "N"}})
                if (const auto at = text.find(from); at != std::string::npos) text.replace(at, std::char_traits<char>::length(from), to);
            return text;
        }
        auto SlotText(const module_render_context& context, const std::string& text, const ui::rect& bounds,
            module_text_profile profile, const int anchor, const float x, const float y, const float opacity) -> void
        {
            if (text.empty()) return;
            const auto& design = context.Design(); const int position = std::clamp(anchor, 0, 8);
            const int column = position % 3, row = position / 3;
            profile.offset_x += x; profile.offset_y += y;
            profile.style.alignment = column == 0 ? ui::text_alignment::left : column == 1 ? ui::text_alignment::center : ui::text_alignment::right;
            if (opacity < 1) profile = TintTextProfile(profile, Alpha(0xFFFFFFFF, opacity));
            const float height = design.TextHeight(profile.style);
            const ui::rect at{bounds.x + 2, bounds.y + static_cast<float>(row) * (bounds.height - height) / 2, std::max(1.0f, bounds.width - 4), height};
            const auto label = xiui::ui::FitText(design, text, at.width, profile.style);
            context.DrawText(label.c_str(), at, bounds, profile);
        }
        auto CooldownSweep(const ui::design_system& design, const ui::rect& bounds, const float ratio, const float radius, const D3DCOLOR color) -> void
        {
            if (ratio <= 0 || bounds.width <= 0 || bounds.height <= 0 || (color >> 24) == 0) return;
            if (ratio >= 1) { design.FillRoundedRect(bounds, color, radius); return; }
            const ui::point center{bounds.x + bounds.width / 2, bounds.y + bounds.height / 2};
            const float curve = std::clamp(radius, 0.0f, std::min(bounds.width, bounds.height) / 2);
            std::array<ui::point, 74> arc{};
            const auto count = static_cast<size_t>(std::clamp(std::ceil(ratio * 72), 1.0f, 72.0f));
            for (size_t index = 0; index <= count; ++index)
            {
                const float angle = -pi / 2 + ratio * pi * 2 * static_cast<float>(index) / static_cast<float>(count);
                const float x = std::cos(angle), y = std::sin(angle), ax = std::abs(x), ay = std::abs(y);
                float edge = std::min(ax > 0.0001f ? bounds.width / (2 * ax) : bounds.width * 10000, ay > 0.0001f ? bounds.height / (2 * ay) : bounds.height * 10000);
                const float cornerX = bounds.width / 2 - curve, cornerY = bounds.height / 2 - curve;
                if (curve > 0 && ax * edge > cornerX && ay * edge > cornerY)
                {
                    const float projection = ax * cornerX + ay * cornerY;
                    edge = projection + std::sqrt(std::max(0.0f, projection * projection - cornerX * cornerX - cornerY * cornerY + curve * curve));
                }
                arc[index] = {center.x + x * edge, center.y + y * edge};
            }
            design.DrawTriangleFan(center, std::span<const ui::point>{arc.data(), count + 1}, color, color);
        }
        auto DrawLock(const ui::design_system& design, const ui::rect& bounds, const bool locked, const D3DCOLOR color) -> void
        {
            const float x = bounds.x + bounds.width / 2 - 5, y = bounds.y + bounds.height / 2 - 5;
            design.BorderRect({x + (locked ? 2.0f : 5.0f), y, 7, 7}, color, 1);
            design.FillRoundedRect({x, y + 5, 12, 8}, color, 1);
            design.FillRect({x + 5, y + 7, 2, 4}, 0xD0151820);
        }
    }

    auto actionbars_module::Layout(const size_t index) -> void
    {
        auto& bar = this->bars_[index]; const float pad = Padding(bar, this->container_);
        if (index == action_bar_count)
        {
            bar.state.bounds.width = GroupWidth(bar) * 2 + bar.group_spacing + pad * 2;
            bar.state.bounds.height = bank_header + DiamondHeight(bar) + (bar.heading ? caption_height : 0) + pad * 2;
        }
        else
        {
            const auto columns = static_cast<size_t>(std::clamp(bar.columns, 1.0f, 12.0f)), slots = static_cast<size_t>(std::clamp(bar.slots, 1.0f, 12.0f));
            const float width = static_cast<float>(std::min(columns, slots)) * (bar.size + bar.gap) - bar.gap;
            bar.state.bounds.width = rail + width + trailing + pad * 2;
            bar.state.bounds.height = std::max(38.0f, GridHeight(bar)) + (bar.heading ? caption_height : 0) + pad * 2;
        }
        bar.state.minimum_width = bar.state.bounds.width;
        bar.state.minimum_height = bar.state.bounds.height;
    }

    auto actionbars_module::BarVisible(const size_t index) const -> bool
    {
        if (index >= this->bars_.size() || !this->available_[index]) return false;
        const bool crossbarPreview = this->crossbar_preview_ && (this->view_ == action_bar_count || this->dialog_ != dialog::none && this->selected_.surface == action_surface::crossbar);
        if (index == action_bar_count && crossbarPreview) return true;
        if (index == action_bar_count && this->crossbar_visibility_ != 0 && this->crossbar_alpha_ <= 0 && this->active_bank_ == crossbar_bank::none && !this->browsing_) return false;
        return this->bars_[index].state.enabled && (index != action_bar_count || this->inactive_ || this->active_bank_ != crossbar_bank::none || this->browsing_ && this->selected_.surface == action_surface::crossbar);
    }

    auto actionbars_module::CrossbarHalf(const size_t bank) const -> size_t
    {
        if (bank == 6) return this->active_bank_ == crossbar_bank::right_left ? 1 : 0;
        return bank == 0 || bank == 2 || bank == 4 ? 0 : 1;
    }

    auto actionbars_module::CrossbarGroups(void) const -> std::vector<size_t>
    {
        const bool preview = this->crossbar_preview_ && (this->view_ == action_bar_count || this->dialog_ != dialog::none && this->selected_.surface == action_surface::crossbar);
        if (this->active_bank_ == crossbar_bank::none && !preview) return {0, 1};
        const auto bank = this->active_bank_ == crossbar_bank::none ? static_cast<size_t>(std::clamp(this->selected_bank_, 0, static_cast<int>(action_crossbar_banks - 1))) : BankIndex(this->active_bank_);
        return this->CrossbarHalf(bank) == 0 ? std::vector<size_t>{bank, 1} : std::vector<size_t>{0, bank};
    }

    auto actionbars_module::SlotBounds(const address& value) const -> ui::rect
    {
        const auto& bar = this->Presentation(value); const auto& bounds = bar.state.bounds;
        const float pad = Padding(bar, this->container_);
        if (value.surface == action_surface::hotbar)
        {
            const auto columns = static_cast<size_t>(std::clamp(bar.columns, 1.0f, 12.0f));
            const float y = bounds.y + pad + (std::max(38.0f, GridHeight(bar)) - GridHeight(bar)) / 2;
            return {bounds.x + pad + rail + static_cast<float>(value.slot % columns) * (bar.size + bar.gap), y + static_cast<float>(value.slot / columns) * (bar.size + (bar.gap_y < 0 ? bar.gap : bar.gap_y) + (bar.labels ? (bar.label_wrap ? 32 : 16) : 0)), bar.size, bar.size};
        }
        constexpr float x[]{1, 2, 1, 0}, y[]{0, 1, 2, 1};
        const auto position = value.slot % 4;
        const float groupX = bounds.x + pad + static_cast<float>(this->CrossbarHalf(value.bar)) * (GroupWidth(bar) + bar.group_spacing);
        return {groupX + (value.slot >= 4 ? DiamondWidth(bar) + DiamondGap(bar) : 0) + x[position] * (bar.size + bar.gap),
            bounds.y + pad + bank_header + y[position] * DiamondSpan(bar) / 2, bar.size, bar.size};
    }

    auto actionbars_module::BarNumberBounds(const size_t index) const -> ui::rect
    {
        const auto& bar = this->bars_[index]; const auto& bounds = bar.state.bounds; const auto pad = Padding(bar, this->container_);
        if (index == action_bar_count) return {bounds.x + bounds.width / 2 - 11, bounds.y + pad + bank_header + (DiamondHeight(bar) - 58) / 2, 22, 18};
        return {bounds.x + pad, bounds.y + pad, 22, 18};
    }
    auto actionbars_module::LockBounds(const size_t index) const -> ui::rect
    { const auto number = this->BarNumberBounds(index); return {number.x, number.y + 20, number.width, 18}; }
    auto actionbars_module::MenuBounds(const size_t index) const -> ui::rect
    {
        const auto& bar = this->bars_[index]; const auto& bounds = bar.state.bounds; const auto pad = Padding(bar, this->container_);
        if (index == action_bar_count) { const auto number = this->BarNumberBounds(index); return {number.x, number.y + 40, 22, 18}; }
        return {bounds.x + bounds.width - pad - 20, bounds.y + pad + (std::min(std::max(38.0f, GridHeight(bar)), bar.size) - 18) / 2, 20, 18};
    }
    auto actionbars_module::PaletteBounds(const size_t index) const -> ui::rect
    {
        const auto& bar = this->bars_[index]; const auto& bounds = bar.state.bounds; const auto pad = Padding(bar, this->container_);
        if (!bar.heading) return {};
        if (index == action_bar_count) return {bounds.x + bounds.width / 2 - 80, bounds.y + bounds.height - pad - caption_height + 1, 160, 18};
        const auto width = std::min(180.0f, bounds.width - pad * 2 - rail - trailing);
        return {bounds.x + pad + rail, bounds.y + bounds.height - pad - caption_height + 1, width, 18};
    }

    auto actionbars_module::HitTest(const float x, const float y) const -> std::optional<hit>
    {
        for (size_t index = this->bars_.size(); index-- > 0;)
        {
            if (!this->BarVisible(index)) continue;
            const auto& bar = this->bars_[index];
            if (!Contains(bar.state.transform.Map(bar.state.bounds), x, y)) continue;
            const bool crossbar = index == action_bar_count;
            const address header{crossbar ? action_surface::crossbar : action_surface::hotbar, crossbar ? static_cast<size_t>(this->selected_bank_) : index, 0};
            for (const auto& [bounds, id] : {std::pair{this->BarNumberBounds(index), 5}, {this->LockBounds(index), 4}, {this->MenuBounds(index), 2}, {this->PaletteBounds(index), 1}})
                if (Contains(bar.state.transform.Map(bounds), x, y)) return hit{header, bar.state.transform.Map(bounds), id};
            const auto groups = crossbar ? this->CrossbarGroups() : std::vector<size_t>{index};
            for (const auto group : groups)
                for (size_t slot = 0; slot < (crossbar ? action_crossbar_slots : static_cast<size_t>(bar.slots)); ++slot)
                {
                    const address value{header.surface, group, slot}; const auto cell = this->SlotBounds(value);
                    if (!Contains(bar.state.transform.Map(cell), x, y)) continue;
                    const bool assigning = !bar.locked || this->shift_drag_ || this->library_drag_.has_value() || this->placing_;
                    if (this->store_ && this->store_->Resolve(value.surface, value.bar, value.slot).Empty() && !bar.empty && !assigning && !this->crossbar_preview_) continue;
                    return hit{value, bar.state.transform.Map(cell), 0};
                }
        }
        return {};
    }

    auto actionbars_module::DrawSlot(const address& value, const ui::rect& cell, const bool active, const module_render_context& context,
        const float opacity, const float yOffset, const bool outgoing, const int inactive, const slot_pass pass) -> void
    {
        if (!this->store_) return;
        const auto& profiles = this->TextFor(value);
        const auto& bounds = cell;
        const ui::rect contentBounds{cell.x, cell.y + yOffset, cell.width, cell.height};
        const auto& slot = this->store_->Resolve(value.surface, value.bar, value.slot); const auto& bar = this->Presentation(value);
        const auto& style = bar.style;
        const bool assigning = !bar.locked || this->shift_drag_ || context.IsEditing() || this->library_drag_.has_value() || this->placing_;
        if (slot.Empty() && !bar.empty && !assigning) return;
        const auto& design = context.Design();
        const bool hovered = !outgoing && this->BarPointerAvailable(this->hover_x_, this->hover_y_) && Contains(context.Design().Transform().Map(cell), this->hover_x_, this->hover_y_) && (!this->pointer_down_ || this->library_drag_ || this->placing_ || this->dragging_);
        const auto key = static_cast<uint32_t>((value.surface == action_surface::crossbar ? 256 : 0) + value.bar * 16 + value.slot);
        const bool held = !outgoing && value.surface == action_surface::crossbar && this->held_slots_[value.bar][value.slot];
        const bool mousePressed = this->bar_pointer_down_ && this->down_header_ == 0 && this->pressed_ == value && !this->dragging_;
        const bool pressed = !outgoing && style.press_highlight && (held || mousePressed);
        const auto flash = this->flashes_.find(key); const bool flashing = !outgoing && style.activation_flash && flash != this->flashes_.end() && flash->second > this->now_;
        const bool otherBank = inactive >= 0 ? inactive != 0 : value.surface == action_surface::crossbar && this->active_bank_ != crossbar_bank::none && !active;
        const float frameIntensity = otherBank ? std::clamp(style.inactive_opacity, 0.0f, 1.0f) : 1.0f;
        const float intensity = opacity * frameIntensity * (value.surface == action_surface::crossbar && !assigning && !this->crossbar_preview_ ? this->crossbar_alpha_ : 1.0f);
        const float border = std::clamp(style.border_width, 0.0f, 4.0f);
        const int frame = std::clamp(style.frame, 0, actionbar_slot_style::window_frame);
        const bool nativeWindow = frame == actionbar_slot_style::window_frame;
        const float radius = std::clamp(nativeWindow ? 3.0f : style.radius, 0.0f, std::min(bounds.width, bounds.height) / 2);
        ui::container_style slotWindow{this->container_.opacity * intensity, border * 2.5f, 0, false, false};
        const bool background = pass == slot_pass::all || pass == slot_pass::background;
        const bool foreground = pass == slot_pass::all || pass == slot_pass::foreground;
        const auto drawFrame = [&] {
            if (!slot.Empty() && style.sheen && frame == 0)
            {
                const float trim = std::max(2.0f, radius);
                if (bounds.width > trim * 2) design.FillRect({bounds.x + trim, bounds.y + 1, bounds.width - trim * 2, 1}, Alpha(0x80D8D0BA, frameIntensity));
            }
            if (!slot.Empty() && frame == 0 && bounds.height > radius * 2 + 2)
                design.FillRect({bounds.x + bounds.width - 2, bounds.y + radius + 1, 1, bounds.height - radius * 2 - 2}, Alpha(0x50000000, frameIntensity));
            if (nativeWindow) { slotWindow.border = border > 0; context.DrawContainerFrame(bounds, slotWindow); }
            else if (frame < 3) RoundedBorder(design, bounds, radius, border, Alpha(style.border, frameIntensity));
            if (pressed || flashing || style.hover_highlight && hovered)
                RoundedBorder(design, bounds, radius, std::max(border, pressed || flashing ? 2.0f : 1.0f),
                    Alpha(pressed || flashing ? style.pressed_color : style.hover_color, frameIntensity));
        };
        const auto drawPrediction = [&] {
            if (this->skillchain_predictions_ && this->skillchains_ != nullptr && !outgoing && this->core_ != nullptr && this->skillchains_->Current(this->now_) != nullptr && this->Inspect(value).learned)
            {
                const auto prediction = slot.kind == action_kind::macro ?
                    this->skillchains_->PredictSlot(this->store_->ExpandMacro(slot), this->core_->GetResourceManager(), this->now_) :
                    this->skillchains_->PredictSlot(slot, this->core_->GetResourceManager(), this->now_);
                if (prediction.Active())
                {
                    const auto color = prediction.chain != skillchain_property::none ? SkillchainColor(prediction.chain) : 0xFFB9DFFF;
                    RoundedBorder(design, bounds, radius, std::max(border, 2.0f), Alpha(color, intensity));
                    const float badge = std::min(16.0f, bounds.width * 0.45f);
                    if (prediction.chain != skillchain_property::none && context.Icons() != nullptr)
                    {
                        auto name = std::string{SkillchainName(prediction.chain)};
                        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        const auto hint = context.Icons()->ActionIcon("skillchain/" + name);
                        if (hint.texture != nullptr) design.DrawTexture(hint.texture, {bounds.x + bounds.width - badge, bounds.y, badge, badge}, Alpha(0xFFFFFFFF, intensity), hint.uv);
                    }
                    if (prediction.burst)
                    {
                        const auto label = TintTextProfile(profiles[5], Alpha(color, intensity));
                        context.DrawText(i18n::Text("MB"), {bounds.x + 1, bounds.y + 1, bounds.width - 2, design.TextHeight(label.style)}, bounds, label);
                    }
                }
            }
        };
        if (background && !nativeWindow && style.shadow && (frame < 2 || !slot.Empty()))
        {
            const ui::rect shadow{bounds.x + 1, bounds.y + 2, bounds.width, bounds.height};
            if (frame == 2) RoundedBorder(design, shadow, radius, std::max(1.0f, border), Alpha(0x90000000, frameIntensity));
            else design.FillRoundedRect(shadow, Alpha(0x90000000, frameIntensity), radius);
        }
        if (background && nativeWindow) context.DrawContainer(bounds, slotWindow);
        else if (background && (frame < 2 || slot.Empty() && assigning)) design.FillRoundedRect(bounds, Alpha(slot.Empty() ? style.empty_fill : style.fill, frameIntensity), radius);
        if (slot.Empty() || pass == slot_pass::background || pass == slot_pass::foreground)
        {
            if (foreground) { drawFrame(); drawPrediction(); }
            return;
        }
        const auto& availability = this->Inspect(value);
        const bool usable = availability.usable;
        const auto tint = Alpha(Multiply(style.icon_tint, otherBank ? style.inactive_tint : !usable ? style.unavailable_tint : 0xFFFFFFFF), style.icon_opacity * intensity);
        const float inset = std::clamp(style.icon_inset, 0.0f, std::max(0.0f, (std::min(bounds.width, bounds.height) - 1) / 2));
        const ui::rect art{bounds.x + inset, bounds.y + inset, bounds.width - inset * 2, bounds.height - inset * 2};
        const float artRadius = std::max(0.0f, radius - inset);
        const float pressScale = value.surface == action_surface::crossbar && this->crossbar_animations_ && !outgoing ?
            this->button_motion_[value.bar][value.slot].Value(this->now_) : 1;
        const ui::rect iconArt{art.x + art.width * (1 - pressScale) / 2, art.y + art.height * (1 - pressScale) / 2 + yOffset, art.width * pressScale, art.height * pressScale};
        const auto drawArt = [&](IDirect3DTexture8* texture, const ui::rect& uv) {
            if (pressScale == 1 && yOffset == 0) { design.DrawTextureRoundedRect(texture, art, uv, artRadius, tint); return; }
            const auto clip = design.Clip(bounds);
            design.DrawTextureRoundedRect(texture, iconArt, uv, artRadius, tint);
        };
        bool drawn = false; auto* icons = context.Icons();
        if (icons)
        {
            auto iconName = slot.icon.empty() ? availability.icon : slot.icon;
            if (iconName.empty() && slot.id != 0)
            {
                if (slot.kind == action_kind::spell) iconName = "spell:" + std::to_string(slot.id);
                else if (slot.kind == action_kind::ability || slot.kind == action_kind::pet || slot.kind == action_kind::weapon_skill) iconName = "ability:" + std::to_string(slot.id);
            }
            if (!iconName.empty())
            {
                const auto icon = icons->ActionIcon(iconName);
                if (icon.texture) { drawArt(icon.texture, icon.uv); drawn = true; }
            }
            if (!drawn && (slot.kind == action_kind::item || slot.kind == action_kind::equip))
                if (auto* texture = icons->Item(slot.id)) { drawArt(texture, {0, 0, 1, 1}); drawn = true; }
            if (!drawn && (slot.kind == action_kind::spell || slot.kind == action_kind::ability || slot.kind == action_kind::pet || slot.kind == action_kind::weapon_skill))
            {
                const auto uv = ui::RecastIconUV(slot.kind == action_kind::spell, slot.id, this->store_->Context().job);
                if (auto* texture = icons->RecastAtlas(); texture && uv.width > 0) { drawArt(texture, uv); drawn = true; }
            }
        }
        if (!drawn)
        {
            const auto clip = design.Clip(bounds);
            const auto glyph = slot.kind == action_kind::macro || slot.kind == action_kind::native_macro ? ui::icon_glyph::commands : slot.kind == action_kind::pet ? ui::icon_glyph::pet : ui::icon_glyph::play;
            const float glyphInset = std::min(5.0f, std::min(iconArt.width, iconArt.height) / 5);
            design.DrawIcon({iconArt.x + glyphInset, iconArt.y - yOffset + glyphInset, iconArt.width - glyphInset * 2, iconArt.height - glyphInset * 2}, glyph, tint);
        }
        std::string timer;
        if (bar.cooldowns && availability.remaining > 0)
        {
            const auto cached = this->inspected_.find(key);
            const float elapsed = cached != this->inspected_.end() && this->now_ >= cached->second.at ? static_cast<float>(this->now_ - cached->second.at) / 1000 : 0;
            const float remaining = std::max(0.0f, availability.remaining - elapsed);
            const float ratio = availability.total > 0 ? std::clamp(remaining / availability.total, 0.0f, 1.0f) : 1;
            const auto cooldownColor = Alpha(style.cooldown_color, intensity);
            if (style.cooldown_style == 0) CooldownSweep(design, art, ratio, artRadius, cooldownColor);
            else if ((style.cooldown_style == 1 || style.cooldown_style == 2) && ratio > 0)
            {
                const float height = art.height * ratio;
                const auto clip = design.Clip({art.x, style.cooldown_style == 1 ? art.y + art.height - height : art.y, art.width, height});
                design.FillRoundedRect(art, cooldownColor, artRadius);
            }
            if (style.cooldown_text && remaining > 0)
            {
                char valueText[24]{};
                if (remaining >= 60) std::snprintf(valueText, sizeof(valueText), "%dm", static_cast<int>(std::ceil(remaining / 60)));
                else std::snprintf(valueText, sizeof(valueText), "%d", static_cast<int>(std::ceil(remaining)));
                timer = valueText;
            }
        }
        if (pressed) design.FillRoundedRect(art, Alpha(style.pressed_color, 0.22f * intensity), artRadius);
        if (flashing) design.FillRoundedRect(art, Alpha(style.pressed_color, 0.12f * intensity), artRadius);
        if (foreground) { drawFrame(); drawPrediction(); }
        if (!timer.empty()) SlotText(context, timer, contentBounds, profiles[3], style.timer_anchor, style.timer_x, style.timer_y, intensity);
        if (bar.bindings && value.surface == action_surface::hotbar)
        {
            const auto binding = this->Binding(value);
            if (binding.key)
            {
                SlotText(context, CompactBinding(this->BindingLabel(binding)), bounds, profiles[2], style.binding_anchor, style.binding_x, style.binding_y, intensity);
            }
        }
        if (bar.resources)
        {
            std::string resource;
            if (availability.maximum_charges > 1) resource = std::to_string(availability.charges);
            else if (slot.kind == action_kind::item) resource = std::to_string(availability.item_count);
            if (!resource.empty())
            {
                SlotText(context, resource, contentBounds, profiles[5], style.count_anchor, style.count_x, style.count_y, intensity);
            }
        }
        if (style.mp_cost && availability.mp > 0)
            SlotText(context, std::to_string(availability.mp), contentBounds, profiles[6], style.cost_anchor, style.cost_x, style.cost_y, intensity);
        if (bar.labels)
        {
            const auto name = slot.label.empty() ? availability.name.empty() ? slot.name : availability.name : slot.label;
            const auto profile = intensity < 1 ? TintTextProfile(profiles[1], Alpha(0xFFFFFFFF, intensity)) : profiles[1];
            std::string first = name, second;
            const float labelWidth = bounds.width + bar.gap - 2;
            if (bar.label_wrap && design.MeasureText(name.c_str(), profile.style) > labelWidth)
            {
                size_t split = name.find_last_of(' ');
                while (split != std::string::npos && design.MeasureText(name.substr(0, split).c_str(), profile.style) > labelWidth) split = split ? name.find_last_of(' ', split - 1) : std::string::npos;
                if (split != std::string::npos) { first = name.substr(0, split); second = name.substr(split + 1); }
            }
            const float textHeight = design.TextHeight(profile.style);
            const ui::rect labelBounds{contentBounds.x - bar.gap / 2, contentBounds.y + contentBounds.height + 1, contentBounds.width + bar.gap, textHeight * (bar.label_wrap ? 2 : 1)};
            const auto label = xiui::ui::FitText(design, first, labelWidth, profile.style);
            context.DrawText(label.c_str(), {labelBounds.x, labelBounds.y, labelBounds.width, textHeight}, labelBounds, profile);
            if (!second.empty()) { const auto next = xiui::ui::FitText(design, second, labelWidth, profile.style); context.DrawText(next.c_str(), {labelBounds.x, labelBounds.y + textHeight, labelBounds.width, textHeight}, labelBounds, profile); }
        }
    }

    auto actionbars_module::RenderBar(const size_t index, const module_render_context& context) -> void
    {
        if (index >= this->bars_.size()) return;
        this->viewport_ = context.Viewport(); this->Layout(index);
        auto& bar = this->bars_[index]; auto& bounds = bar.state.bounds;
        auto profiles = bar.custom_text ? bar.text : this->text_;
        const float visibility = index == action_bar_count && !context.IsEditing() && !this->crossbar_preview_ ? this->crossbar_alpha_ : 1;
        if (visibility < 1) for (auto& profile : profiles) profile = TintTextProfile(profile, Alpha(0xFFFFFFFF, visibility));
        if (bounds.x < 0 || bounds.y < 0)
        {
            const auto viewport = context.Viewport();
            bounds.x = std::max(viewport.x + 8, viewport.x + (viewport.width - bounds.width) / 2);
            bounds.y = index == action_bar_count ? std::max(viewport.y + 8, viewport.y + viewport.height - bounds.height - 40) : std::max(viewport.y + 8, viewport.y + viewport.height - bounds.height - 160 - static_cast<float>(index) * 52);
        }
        const auto& design = context.Design(); const auto& colors = design.GetTheme().colors;
        design.IncludeContentBounds(bounds);
        if (bar.background) { auto container = this->container_; container.opacity *= visibility; context.DrawContainer(bounds, container); }
        const bool crossbar = index == action_bar_count;
        const auto number = this->BarNumberBounds(index), lock = this->LockBounds(index), menu = this->MenuBounds(index);
        auto numberProfile = profiles[2]; numberProfile.style.alignment = ui::text_alignment::center;
        const auto numberText = crossbar ? std::string{"C"} : std::to_string(index + 1);
        if (Contains(number, this->hover_x_, this->hover_y_)) design.FillRoundedRect(number, Alpha(colors.control, 0.5f), 3);
        context.DrawText(numberText.c_str(), number, number, TintTextProfile(numberProfile, colors.muted));
        if (Contains(lock, this->hover_x_, this->hover_y_)) design.FillRoundedRect(lock, Alpha(colors.control, 0.5f), 3);
        DrawLock(design, lock, bar.locked, Contains(lock, this->hover_x_, this->hover_y_) ? colors.text : Alpha(colors.muted, 0.7f));
        design.DrawIcon({menu.x + 3, menu.y + 2, menu.width - 6, menu.height - 4}, ui::icon_glyph::more, Contains(menu, this->hover_x_, this->hover_y_) ? colors.text : Alpha(colors.muted, 0.55f));
        const auto paletteBounds = this->PaletteBounds(index);
        if (paletteBounds.width > 0)
        {
            const auto surface = crossbar ? action_surface::crossbar : action_surface::hotbar;
            const auto bank = crossbar ? static_cast<size_t>(this->selected_bank_) : index;
            const auto* palette = this->store_ ? this->store_->ActivePalette(surface, bank) : nullptr;
            auto profile = TintTextProfile(profiles[1], Contains(paletteBounds, this->hover_x_, this->hover_y_) ? colors.text : Alpha(colors.muted, 0.8f));
            profile.style.alignment = crossbar ? ui::text_alignment::center : ui::text_alignment::left;
            const auto label = xiui::ui::FitText(design, palette ? palette->name : "Default", paletteBounds.width - 12, profile.style);
            context.DrawText(label.c_str(), paletteBounds, paletteBounds, profile);
            const float textWidth = design.MeasureText(label.c_str(), profile.style);
            const float arrowX = crossbar ? paletteBounds.x + (paletteBounds.width + textWidth) / 2 + 4 : paletteBounds.x + textWidth + 4;
            design.DrawIcon({arrowX, paletteBounds.y + 6, 7, 7}, ui::icon_glyph::chevron_down, Alpha(colors.muted, 0.7f));
        }
        if (!crossbar)
        {
            for (size_t slot = 0; slot < static_cast<size_t>(bar.slots); ++slot)
            { const address value{action_surface::hotbar, index, slot}; this->DrawSlot(value, this->SlotBounds(value), true, context); }
            return;
        }
        const float pad = Padding(bar, this->container_);
        const auto backend = this->controller_status_.backend != controller_backend::none ? this->controller_status_.backend :
            this->setup_backend_ == 0 ? controller_backend::xinput : controller_backend::direct_input;
        const int theme = this->prompt_theme_ == 0 ? backend == controller_backend::xinput ? 1 : 4 : this->prompt_theme_;
        const auto& mapping = this->controller_status_.crossbar_mapping_known ? this->controller_status_.crossbar_mapping :
            backend == controller_backend::xinput ? this->controller_settings_.xinput : this->controller_settings_.direct_input;
        const auto drawPrompt = [&](const crossbar_visuals::button_prompt& prompt, const ui::rect& at, const float opacity) {
            const auto glyph = context.Icons() ? context.Icons()->ActionIcon(prompt.icon) : ui::game_icon_view{};
            if (glyph.texture) design.DrawTexture(glyph.texture, at, Alpha(0xFFFFFFFF, opacity), glyph.uv);
            else
            {
                auto profile = TintTextProfile(profiles[2], Alpha(0xFFFFFFFF, opacity));
                profile.style.alignment = ui::text_alignment::center;
                const auto label = xiui::ui::FitText(design, prompt.label, at.width + 4, profile.style);
                context.DrawText(label.c_str(), {at.x - 2, at.y, at.width + 4, at.height}, at, profile);
            }
        };
        const float slide = 20 * bar.size / 48;
        for (const auto bank : this->CrossbarGroups())
        {
            const bool preview = this->crossbar_preview_ && (this->view_ == action_bar_count || this->dialog_ != dialog::none && this->selected_.surface == action_surface::crossbar);
            const bool active = this->active_bank_ != crossbar_bank::none ? BankIndex(this->active_bank_) == bank : preview && static_cast<size_t>(this->selected_bank_) == bank;
            const auto half = this->CrossbarHalf(bank);
            const float groupX = bounds.x + pad + static_cast<float>(half) * (GroupWidth(bar) + bar.group_spacing);
            if (this->prompts_)
            {
                const bool ordered = bank == 2 || bank == 3 || bank == 6, twice = bank == 4 || bank == 5;
                const size_t first = bank == 6 ? half : bank == 1 || bank == 3 || bank == 5 ? 1 : 0;
                const float centerX = groupX + GroupWidth(bar) / 2, centerY = bounds.y + pad + bank_header / 2;
                for (size_t step = 0; step < (ordered ? 2u : 1u); ++step)
                {
                    const size_t trigger = step == 0 ? first : 1 - first;
                    const auto prompt = crossbar_visuals::Trigger(trigger != 0, trigger == 0 ? mapping.left_trigger : mapping.right_trigger, backend, theme);
                    const float scale = this->crossbar_animations_ ? this->trigger_motion_[trigger].Value(this->now_) : 1;
                    const float strength = this->crossbar_animations_ ? std::clamp((scale - 1) / 0.12f, 0.0f, 1.0f) : (this->triggers_ & (1u << trigger)) != 0 ? 1.0f : 0.0f;
                    const bool wide = prompt.icon == "controller/shared/l1" || prompt.icon == "controller/shared/l2" || prompt.icon == "controller/shared/r1" || prompt.icon == "controller/shared/r2";
                    const float width = (wide || prompt.icon.empty() ? 42.0f : 24.0f) * scale, height = 24 * scale;
                    const float x = centerX + (ordered ? (step == 0 ? -28.0f : 28.0f) : twice ? -8.0f : 0);
                    drawPrompt(prompt, {x - width / 2, centerY - height / 2, width, height}, visibility * (0.53f + strength * 0.47f));
                }
                if (ordered || twice)
                {
                    auto profile = profiles[2]; profile.style.alignment = ui::text_alignment::center;
                    const ui::rect at{centerX + (twice ? 12.0f : -6.0f), centerY - 7, twice ? 24.0f : 12.0f, 16};
                    context.DrawText(twice ? "x2" : ">", at, at, TintTextProfile(profile, Alpha(0xFFFFFFFF, active ? 1 : 0.6f)));
                }
            }
            auto& motion = this->bank_motion_[half];
            motion.Select(bank, this->now_, slide, this->crossbar_animations_ && !context.IsEditing(), this->active_bank_ != crossbar_bank::none && !active);
            const bool transitioning = motion.changing && this->now_ < motion.changed_at + 500;
            if (transitioning)
                for (size_t slot = 0; slot < action_crossbar_slots; ++slot)
                {
                    const address value{action_surface::crossbar, bank, slot};
                    this->DrawSlot(value, this->SlotBounds(value), active, context, 1, 0, false, motion.target_inactive ? 1 : 0, slot_pass::background);
                }
            {
                const auto clip = design.Clip({groupX - 1, bounds.y + pad + bank_header - 1, GroupWidth(bar) + 2, DiamondHeight(bar) + 2});
                for (size_t layerIndex = 0; layerIndex <= action_crossbar_banks; ++layerIndex)
                {
                    const size_t layerBank = layerIndex == action_crossbar_banks ? bank : layerIndex;
                    if (layerIndex != action_crossbar_banks && layerBank == bank) continue;
                    const auto layer = motion.Value(layerBank, this->now_, slide);
                    const bool outgoing = layerBank != bank;
                    if (layer.opacity < 0.01f) continue;
                    const float offsetX = (static_cast<float>(half) - static_cast<float>(this->CrossbarHalf(layerBank))) * (GroupWidth(bar) + bar.group_spacing);
                    for (size_t slot = 0; slot < action_crossbar_slots; ++slot)
                    {
                        const address value{action_surface::crossbar, layerBank, slot};
                        auto at = this->SlotBounds(value); at.x += offsetX;
                        this->DrawSlot(value, at, active, context, layer.opacity, layer.y, outgoing, layer.inactive ? 1 : 0,
                            transitioning ? slot_pass::content : slot_pass::all);
                    }
                    if (this->prompts_ && bar.bindings && bar.button_icons)
                        for (size_t diamond = 0; diamond < 2; ++diamond)
                        {
                            auto top = this->SlotBounds({action_surface::crossbar, layerBank, diamond * 4}); top.x += offsetX;
                            const float centerX = top.x + bar.size / 2;
                            const float centerY = top.y + DiamondSpan(bar) / 2 + bar.size / 2 + layer.y;
                            const float iconSize = std::max(4.0f, std::min({bar.prompt_size, bar.size / 2, (bar.size / 2 + bar.gap - 3) / 1.07f}));
                            const float offset = iconSize / 2 + 2;
                            constexpr float x[]{0, 1, 0, -1}, y[]{-1, 0, 1, 0};
                            for (size_t position = 0; position < 4; ++position)
                            {
                                const size_t slot = diamond * 4 + position;
                                const float scale = this->crossbar_animations_ && !outgoing ? this->button_motion_[layerBank][slot].Value(this->now_) : 1;
                                const float size = iconSize * scale;
                                const ui::rect at{centerX + x[position] * (offset + bar.prompt_gap_x - 8) - size / 2, centerY + y[position] * (offset + bar.prompt_gap_y - 2) - size / 2, size, size};
                                drawPrompt(crossbar_visuals::Button(mapping.slots[slot], backend, theme, slot), at, visibility * layer.opacity * (layer.inactive || this->active_bank_ == crossbar_bank::none && !active ? 0.5f : 1.0f));
                            }
                        }
                }
            }
            if (transitioning)
                for (size_t slot = 0; slot < action_crossbar_slots; ++slot)
                {
                    const address value{action_surface::crossbar, bank, slot};
                    this->DrawSlot(value, this->SlotBounds(value), active, context, 1, 0, false, motion.target_inactive ? 1 : 0, slot_pass::foreground);
                }
            if (active) design.FillRect({groupX + 4, std::min(bounds.y + bounds.height - 1, bounds.y + pad + bank_header + DiamondHeight(bar) + 2), GroupWidth(bar) - 8, 1}, Alpha(colors.control_selected, 0.5f));
        }
    }

    auto actionbars_module::DrawSlotDrag(const module_render_context& context) -> void
    {
        if (!this->dragging_) return;
        const auto& bar = this->Presentation(this->pressed_);
        this->DrawSlot(this->pressed_, {this->hover_x_ - bar.size / 2, this->hover_y_ - bar.size / 2, bar.size, bar.size}, true, context);
        const auto hit = this->BarPointerAvailable(this->hover_x_, this->hover_y_) ? this->HitTest(this->hover_x_, this->hover_y_) : std::nullopt;
        if (hit && hit->header == 0 && (!this->Presentation(hit->slot).locked || this->shift_drag_)) context.Design().DrawFocusFrame(hit->bounds, this->focus_style_);
    }

    auto actionbars_module::DrawTooltip(const address& value, const ui::rect& anchor, const module_render_context& context) -> void
    {
        if (!this->store_ || !this->Presentation(value).tooltips) return;
        const auto& profiles = this->TextFor(value);
        const auto slot = this->store_->ExpandMacro(this->store_->Resolve(value.surface, value.bar, value.slot));
        if (slot.Empty()) return;
        const auto& info = this->Inspect(value); const auto& design = context.Design();
        std::vector<std::string> lines{slot.label.empty() ? info.name.empty() ? slot.name : info.name : slot.label};
        lines.push_back(std::string{i18n::Text(ActionKindName(slot.kind))} + (slot.target.empty() || slot.kind == action_kind::equip || slot.kind == action_kind::macro || slot.kind == action_kind::command ? "" : "  " + slot.target));
        std::string resources;
        if (info.mp) resources += std::to_string(info.mp) + " MP  ";
        if (info.tp) resources += std::to_string(info.tp) + " TP  ";
        if (info.maximum_charges > 1) resources += i18n::Format("%u/%u charges", info.charges, info.maximum_charges);
        if (slot.kind == action_kind::item) resources += i18n::Format("%u in inventory", info.item_count);
        if (!resources.empty()) lines.push_back(resources);
        if (!info.reason.empty()) lines.emplace_back(i18n::Text(info.reason.c_str()));
        if (slot.kind == action_kind::macro || slot.kind == action_kind::command)
        {
            for (size_t index = 0; index < std::min(size_t{3}, slot.lines.size()); ++index) lines.push_back(slot.lines[index]);
            if (slot.lines.size() > 3) lines.push_back(i18n::Format("+ %zu more lines", slot.lines.size() - 3));
        }
        if (slot.kind == action_kind::native_macro) lines.push_back(i18n::Format("Book %u, set %u, %s %u", static_cast<unsigned>(slot.native.book), static_cast<unsigned>(slot.native.set), slot.native.alt ? "Alt" : "Ctrl", static_cast<unsigned>(slot.native.slot)));
        const auto binding = this->Binding(value); if (binding.key) lines.push_back(this->BindingLabel(binding));
        lines.emplace_back(i18n::Text(this->Presentation(value).locked ? "Click to activate  |  Shift-drag to rearrange" : "Click to activate  |  Drag to rearrange"));
        float width = 220;
        for (const auto& line : lines) width = std::max(width, design.MeasureText(line.c_str(), profiles[4].style) + 24);
        const auto viewport = context.Viewport().width > 0 ? context.Viewport() : ui::rect{0, 0, 1280, 720};
        width = std::min(width, std::min(420.0f, viewport.width - 16));
        const float pitch = std::max(18.0f, design.TextHeight(profiles[4].style) + 3), height = 18 + static_cast<float>(lines.size()) * pitch;
        float y = anchor.y - height - 8; if (y < viewport.y + 8) y = anchor.y + anchor.height + 8;
        const ui::rect bounds{std::clamp(anchor.x, viewport.x + 8, std::max(viewport.x + 8, viewport.x + viewport.width - width - 8)), std::clamp(y, viewport.y + 8, std::max(viewport.y + 8, viewport.y + viewport.height - height - 8)), width, height};
        context.DrawTooltip(bounds);
        for (size_t index = 0; index < lines.size(); ++index)
        {
            const auto profile = index == 0 ? TintTextProfile(profiles[4], design.GetTheme().colors.gold) : profiles[4];
            const auto line = xiui::ui::FitText(design, lines[index], width - 24, profile.style);
            context.DrawText(line.c_str(), {bounds.x + 12, bounds.y + 9 + static_cast<float>(index) * pitch, width - 24, pitch}, bounds, profile);
        }
    }
}
