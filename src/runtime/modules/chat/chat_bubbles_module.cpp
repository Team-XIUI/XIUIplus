#include "chat_bubbles_module.hpp"
#include "chat_theme.hpp"
#include "chat/chat_encoding.hpp"
#include "chat/chat_text.hpp"
#include "utf8.hpp"
#include "world_projection.hpp"
#include "world/entity_render_position.hpp"
#include "Memory.h"

#include <cmath>
#include <cstring>
#include <iterator>

namespace
{
    constexpr uint64_t entrance_ms = 180, exit_ms = 240;
    auto Ease(const float progress) -> float
    { const float t = std::clamp(progress, 0.0f, 1.0f); return t * t * (3 - 2 * t); }

    auto FadeBubbleText(const xiui::module_text_profile& profile, const float opacity) -> xiui::module_text_profile
    {
        const auto fade = [opacity](const D3DCOLOR color) { return (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * opacity) << 24); };
        auto result = xiui::TintTextProfile(profile, fade(0xFFFFFFFF));
        result.style.shadow.color = fade(result.style.shadow.color);
        result.style.stroke.color = fade(result.style.stroke.color);
        result.style.glow.color = fade(result.style.glow.color);
        result.style.sheen.color = fade(result.style.sheen.color);
        return result;
    }
}

namespace xiui
{
    auto chat_bubbles_module::Motion(const bubble& value, const uint64_t now) -> bubble_motion
    {
        const float remaining = Ease(static_cast<float>(value.expires - std::min(now, value.expires)) / exit_ms);
        if (value.retiring) return {value.exit_opacity * remaining, value.exit_offset - (1 - remaining) * 6};
        const float entrance = Ease(static_cast<float>(now - std::min(now, value.started)) / entrance_ms);
        return {entrance * remaining, (1 - entrance) * 6 - (1 - remaining) * 6};
    }

    auto chat_bubbles_module::Retire(bubble value, const uint64_t now) -> void
    {
        const auto motion = Motion(value, now);
        if (motion.opacity <= 0) return;
        value.retiring = true;
        value.exit_opacity = motion.opacity;
        value.exit_offset = motion.offset_y;
        value.expires = now + exit_ms;
        if (this->outgoing_.size() >= 12) this->outgoing_.erase(this->outgoing_.begin());
        this->outgoing_.push_back(std::move(value));
    }

    auto chat_bubbles_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {theme.body_text};
        this->text_.style.top_color = this->text_.style.bottom_color = 0xFFFFFFFF;
        this->text_.style.alignment = ui::text_alignment::left;
    }

    auto chat_bubbles_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto* colors = this->colors_;
        *this = {};
        this->colors_ = colors;
        this->ApplyTextDefaults(theme);
    }

    auto chat_bubbles_module::Load(const settings_store& store) -> void
    {
        this->Clear();
        this->state_.enabled = store.ReadBool(this->Id(), "enabled", true);
        for (size_t i = 0; i < this->channels_.size(); ++i)
            this->channels_[i] = store.ReadBool(this->Id(), bubble_channel_keys[i], true);
        this->range_ = std::clamp(store.ReadFloat(this->Id(), "range", 30), 5.0f, 100.0f);
        this->duration_ = std::clamp(store.ReadFloat(this->Id(), "duration", 8), 2.0f, 30.0f);
        this->width_ = std::clamp(store.ReadFloat(this->Id(), "width", 360), 120.0f, 640.0f);
        this->offset_y_ = std::clamp(store.ReadFloat(this->Id(), "offset_y", 28), 0.0f, 160.0f);
        this->opacity_ = std::clamp(store.ReadFloat(this->Id(), "opacity", 0), 0.0f, 1.0f);
        if (store.ReadInt(this->Id(), "appearance_version", 0) < 2)
        {
            if (this->width_ == 320) this->width_ = 360;
            if (this->offset_y_ == 20) this->offset_y_ = 28;
            if (this->opacity_ == 0.9f) this->opacity_ = 0;
        }
        this->padding_ = std::clamp(store.ReadFloat(this->Id(), "padding", 10), 2.0f, 24.0f);
        this->chat_colors_ = store.ReadBool(this->Id(), "chat_colors", true);
        this->text_ = LoadTextProfile(store, "text.chat.bubbles", this->text_);
    }

    auto chat_bubbles_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool(this->Id(), "enabled", this->state_.enabled);
        for (size_t i = 0; i < this->channels_.size(); ++i)
            store.WriteBool(this->Id(), bubble_channel_keys[i], this->channels_[i]);
        store.WriteFloat(this->Id(), "range", this->range_);
        store.WriteFloat(this->Id(), "duration", this->duration_);
        store.WriteFloat(this->Id(), "width", this->width_);
        store.WriteFloat(this->Id(), "offset_y", this->offset_y_);
        store.WriteFloat(this->Id(), "opacity", this->opacity_);
        store.WriteInt(this->Id(), "appearance_version", 2);
        store.WriteFloat(this->Id(), "padding", this->padding_);
        store.WriteBool(this->Id(), "chat_colors", this->chat_colors_);
        SaveTextProfile(store, "text.chat.bubbles", this->text_);
    }

    auto chat_bubbles_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Overhead bubble visibility, nearby range and lifetime.", "general"},
            {"Channels", "Choose which received chat channels appear over their speakers.", "channels"},
            {"Appearance", "Bubble width, spacing, background and channel colors.", "appearance"},
            {"Text", "Bubble font, size, colors and effects.", "text"}};
        return sections;
    }

    auto chat_bubbles_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount() || section >= this->ConfigSections().size()) return false;
        return section == (index >= text_start ? 3 : index >= channel_start ? 1 : index >= 3 ? 2 : 0);
    }

    auto chat_bubbles_module::ContentOption(const size_t index) -> module_option
    {
        if (index >= text_start) return TextProfileOption(index - text_start, this->text_);
        if (index >= channel_start)
            return module_option::Toggle(bubble_channel_names[index - channel_start],
                "Show received messages above nearby speakers. NPC choices remain in the native menu.", &this->channels_[index - channel_start]);
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Show speech bubbles above nearby players and NPCs.", &this->state_.enabled).Ordered(-50);
        case 1: return module_option::Slider("Range", "Maximum distance to a loaded speaker, in yalms.", &this->range_, 5, 100, 1);
        case 2: return module_option::Slider("Duration", "Seconds to show a message. Pending NPC dialogue stays visible until advanced.", &this->duration_, 2, 30, 1);
        case 3: return module_option::Slider("Maximum width", "Wrap long messages within this bubble width.", &this->width_, 120, 640, 4).Ordered(100);
        case 4: return module_option::Slider("Head spacing", "Extra pixels above the speaker's head.", &this->offset_y_, 0, 160, 1).Ordered(110);
        case 5: return module_option::Slider("Background opacity", "Opacity of the native window background. Zero hides the background, border and tail.", &this->opacity_, 0, 1, 0.05f).Ordered(120);
        case 6: return module_option::Slider("Padding", "Space around bubble text.", &this->padding_, 2, 24, 1).Ordered(130);
        default: return module_option::Toggle("Chat colors", "Follow chat's channel and inline color preferences, preserving bubble text effects.", &this->chat_colors_).Ordered(140);
        }
    }

    auto chat_bubbles_module::OnContentOptionChanged(size_t) -> void
    {
        if (!this->state_.enabled) this->Clear();
        std::erase_if(this->bubbles_, [this](const auto& value) { return !this->channels_[static_cast<size_t>(value.channel)]; });
        std::erase_if(this->outgoing_, [this](const auto& value) { return !this->channels_[static_cast<size_t>(value.channel)]; });
    }

    auto chat_bubbles_module::Clear(void) -> void
    {
        this->bubbles_.clear();
        this->outgoing_.clear();
        this->npc_id_ = this->npc_index_ = 0;
        this->last_sequence_ = 0;
        this->dialogue_active_ = false;
    }

    auto chat_bubbles_module::SpeakerAvailable(const IEntity& entities, const uint32_t player, const uint32_t index, const uint32_t id) const -> bool
    {
        const auto limit = entities.GetEntityMapSize();
        if (player == 0 || player >= limit || index == 0 || index >= limit || id == 0 || entities.GetServerId(index) != id) return false;
        const auto flags = entities.GetRenderFlags0(index);
        if ((flags & 0x200) == 0 || (flags & 0x4000) != 0 || entities.GetActorPointer(index) == 0) return false;
        const float x = entities.GetLocalPositionX(index) - entities.GetLocalPositionX(player);
        const float y = entities.GetLocalPositionY(index) - entities.GetLocalPositionY(player);
        const float z = entities.GetLocalPositionZ(index) - entities.GetLocalPositionZ(player);
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && x * x + y * y + z * z <= this->range_ * this->range_;
    }

    auto chat_bubbles_module::FindSpeaker(IAshitaCore* core, const std::string_view name, const bool npc) const -> uint32_t
    {
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* entities = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* target = memory != nullptr ? memory->GetTarget() : nullptr;
        if (entities == nullptr || party == nullptr || name.empty()) return 0;
        const auto player = party->GetMemberTargetIndex(0);
        std::string encoded;
        if (!ToGameText(name, encoded)) return 0;
        uint32_t best{};
        float distance = this->range_ * this->range_;
        for (uint32_t index = 1; index < std::min(entities->GetEntityMapSize(), 4096u); ++index)
        {
            const auto id = entities->GetServerId(index);
            if (id == 0 || ((entities->GetSpawnFlags(index) & 1) == 0) != npc) continue;
            const auto* raw = entities->GetName(index);
            if (raw == nullptr || strnlen(raw, 28) != encoded.size() || _strnicmp(raw, encoded.data(), encoded.size()) != 0 ||
                !this->SpeakerAvailable(*entities, player, index, id)) continue;
            if (npc && target != nullptr && target->GetTargetIndex(0) == index) return index;
            const float x = entities->GetLocalPositionX(index) - entities->GetLocalPositionX(player);
            const float y = entities->GetLocalPositionY(index) - entities->GetLocalPositionY(player);
            const float z = entities->GetLocalPositionZ(index) - entities->GetLocalPositionZ(player);
            const float next = x * x + y * y + z * z;
            if (next <= distance) { distance = next; best = index; }
        }
        return best;
    }

    auto chat_bubbles_module::Receive(IAshitaCore* core, const chat_line& line) -> void
    {
        if (!this->state_.enabled || line.sequence == this->last_sequence_) return;
        this->last_sequence_ = line.sequence;
        auto message = ParseBubbleMessage(line);
        if (!message || !this->channels_[static_cast<size_t>(message->channel)]) return;
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* entities = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (entities == nullptr || party == nullptr || player == nullptr || player->GetIsZoning()) return;
        const auto zone = party->GetMemberZone(0);
        if (zone != this->zone_) { this->Clear(); this->zone_ = zone; this->last_sequence_ = line.sequence; }
        const bool npc = message->channel == bubble_channel::npc;
        const auto index = message->continuation ? (this->dialogue_active_ ? this->npc_index_ : 0) : this->FindSpeaker(core, message->speaker, npc);
        if (index == 0 || index >= entities->GetEntityMapSize()) return;
        const auto id = entities->GetServerId(index);
        if (!this->SpeakerAvailable(*entities, party->GetMemberTargetIndex(0), index, id) || (message->continuation && id != this->npc_id_)) return;
        if (npc) { this->npc_id_ = id; this->npc_index_ = index; }
        this->Add(line, std::move(*message), index, id, ::GetTickCount64());
    }

    auto chat_bubbles_module::Add(const chat_line& line, bubble_message message, const uint32_t index, const uint32_t id, const uint64_t now) -> void
    {
        auto found = std::find_if(this->bubbles_.begin(), this->bubbles_.end(), [id](const auto& value) { return value.id == id; });
        if (found == this->bubbles_.end())
        {
            if (this->bubbles_.size() >= 12) this->bubbles_.erase(this->bubbles_.begin());
            this->bubbles_.push_back({});
            found = std::prev(this->bubbles_.end());
        }
        const bool continuation = message.continuation && found->channel == bubble_channel::npc && !found->spans.empty() && now < found->expires;
        if (!continuation && !found->spans.empty())
        {
            this->Retire(std::move(*found), now);
            *found = {};
        }
        if (continuation)
        {
            size_t length{}; for (const auto& span : found->spans) length += span.text.size();
            if (length > 2048) found->spans.clear();
            else found->spans.push_back({" ", -1});
            found->spans.insert(found->spans.end(), std::make_move_iterator(message.spans.begin()), std::make_move_iterator(message.spans.end()));
        }
        else found->spans = std::move(message.spans);
        found->id = id; found->index = index; found->mode = line.mode; found->color = line.color;
        found->channel = message.channel; found->prompt = line.dialogue_prompt;
        if (!continuation) found->started = now;
        found->expires = now + static_cast<uint64_t>(this->duration_ * 1000);
        found->cache_font = -1;
    }

    auto chat_bubbles_module::Update(IAshitaCore* core, const bool dialogueActive, const uint64_t now) -> void
    {
        this->core_ = core;
        this->dialogue_active_ = dialogueActive;
        if (!dialogueActive) this->npc_id_ = this->npc_index_ = 0;
        const auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        const auto* entities = memory != nullptr ? memory->GetEntity() : nullptr;
        const auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        const auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        if (!this->state_.enabled || entities == nullptr || party == nullptr || player == nullptr || player->GetIsZoning() ||
            player->GetMainJob() == 0 || party->GetMemberZone(0) != this->zone_)
        { this->Clear(); return; }
        const auto expired = [&](auto& value) {
            if (!this->channels_[static_cast<size_t>(value.channel)] || !this->SpeakerAvailable(*entities, party->GetMemberTargetIndex(0), value.index, value.id)) return true;
            if (!value.retiring && dialogueActive && value.id == this->npc_id_ && value.prompt)
                value.expires = now + static_cast<uint64_t>(this->duration_ * 1000);
            return now >= value.expires;
        };
        std::erase_if(this->bubbles_, expired);
        std::erase_if(this->outgoing_, expired);
    }

    auto chat_bubbles_module::RebuildRows(bubble& value, const ui::design_system& design, const float width) const -> void
    {
        const auto style = ChatThemeStyle(design, this->text_.style);
        if (value.cache_font == style.font && value.cache_width == width && value.cache_scale == style.scale && value.cache_spacing == style.letter_spacing) return;
        value.rows.clear();
        bubble_row row{};
        const auto finish = [&] { value.rows.push_back(std::move(row)); row = {}; };
        const auto append = [&](const std::string& text, const int color, const float measured) {
            if (!row.spans.empty() && row.spans.back().color_index == color)
            { row.spans.back().text += text; row.spans.back().width += measured; }
            else row.spans.push_back({text, color, measured});
            row.width += measured;
        };
        for (const auto& span : value.spans)
            for (size_t at = 0; at < span.text.size();)
            {
                if (span.text[at] == '\n') { finish(); ++at; continue; }
                auto end = span.text.find_first_of(" \t\n", at);
                if (end == at) ++end;
                if (end == std::string::npos) end = span.text.size();
                auto token = span.text.substr(at, end - at); at = end;
                std::replace(token.begin(), token.end(), '\t', ' ');
                const float measured = MeasureChatText(design, token, style) + ChatThemeTracking(style);
                if (row.width > 0 && row.width + measured > width) finish();
                if (row.width == 0 && token == " ") continue;
                if (measured <= width) append(token, span.color_index, measured);
                else for (size_t pos = 0; pos < token.size();)
                {
                    const auto begin = pos; NextUtf8(token, pos);
                    const auto glyph = token.substr(begin, pos - begin);
                    const float glyphWidth = MeasureChatText(design, glyph, style) + ChatThemeTracking(style);
                    if (row.width > 0 && row.width + glyphWidth > width) finish();
                    append(glyph, span.color_index, glyphWidth);
                }
            }
        if (!row.spans.empty()) finish();
        if (value.prompt && !value.rows.empty())
        {
            const float markerWidth = MeasureChatText(design, "\xE2\x96\xBC", style) + 2;
            if (value.rows.back().width + markerWidth > width) value.rows.push_back({});
            value.rows.back().width += markerWidth;
        }
        if (value.rows.size() > 8) value.rows.erase(value.rows.begin(), value.rows.end() - 8);
        value.cache_font = style.font; value.cache_width = width; value.cache_scale = style.scale; value.cache_spacing = style.letter_spacing;
    }

    auto chat_bubbles_module::Render(const hud_snapshot& snapshot, const module_render_context& context) -> void
    {
        const auto screen = context.Design().Transform({});
        if (!this->ShouldRender(snapshot) || !this->text_.visible || this->core_ == nullptr) return;
        const auto* memory = this->core_->GetMemoryManager();
        const auto* entities = memory != nullptr ? memory->GetEntity() : nullptr;
        if (entities == nullptr) return;
        const auto& viewport = context.Viewport();
        D3DVIEWPORT8 projectionViewport{static_cast<DWORD>(viewport.x), static_cast<DWORD>(viewport.y), static_cast<DWORD>(viewport.width), static_cast<DWORD>(viewport.height), 0, 1};
        const float innerWidth = std::max(32.0f, std::min(this->width_, viewport.width - 16) - this->padding_ * 2);
        const float lineHeight = context.Design().TextHeight(this->text_.style) + 2;
        const auto now = ::GetTickCount64();
        static const auto clientCode = Ashita::Memory::GetModuleBase("FFXiMain.dll");
        static const auto clientSize = Ashita::Memory::GetModuleSize("FFXiMain.dll");
        for (auto* values : {&this->outgoing_, &this->bubbles_})
        for (auto& value : *values)
        {
            const auto motion = Motion(value, now);
            if (motion.opacity <= 0) continue;
            this->RebuildRows(value, context.Design(), innerWidth);
            if (value.rows.empty()) continue;
            const auto index = value.index;
            if (index == 0 || index >= entities->GetEntityMapSize() || entities->GetServerId(index) != value.id) continue;
            std::array<float, 3> head{};
            if (!ReadEntityRenderPosition(entities->GetActorPointer(index), reinterpret_cast<uintptr_t>(entities->GetRawEntity(index)),
                clientCode, clientSize, head)) continue;
            const float modelScale = entities->GetModelSize(index);
            const float scale = std::isfinite(modelScale) && modelScale > 0 ? std::clamp(modelScale, 0.5f, 3.0f) : 1.0f;
            const auto race = entities->GetRace(index);
            const float height = (race == 5 || race == 6 ? 1.0f : race == 3 || race == 4 || race == 8 ? 2.1f : 1.7f) * scale;
            head[1] -= height;
            if (!ProjectWorldPoint(head, snapshot.scene_view, snapshot.scene_projection, projectionViewport, value.x, value.y)) continue;
            const auto transform = context.Design().Transform(ui::hud_transform::At(value.x, value.y, this->state_.transform.x, this->state_.transform.y));
            float contentWidth{}; for (const auto& row : value.rows) contentWidth = std::max(contentWidth, row.width);
            const bool prompt = value.prompt && !value.retiring && this->dialogue_active_ && value.id == this->npc_id_;
            contentWidth = std::min(innerWidth, contentWidth);
            const float width = contentWidth + this->padding_ * 2;
            const float heightPixels = lineHeight * static_cast<float>(value.rows.size()) + this->padding_ * 2;
            const float bottom = value.y - this->offset_y_ - 8 + motion.offset_y;
            if (bottom - heightPixels < viewport.y + 4) continue;
            const ui::rect bounds{std::clamp(value.x - width * 0.5f, viewport.x + 4, viewport.x + viewport.width - width - 4), bottom - heightPixels, width, heightPixels};
            const float fade = motion.opacity;
            if (this->opacity_ > 0)
            {
                const ui::container_style container{this->opacity_ * fade, 3, this->padding_, false, true};
                context.DrawContainer(bounds, container);
                const auto tailColor = (context.Containers() != nullptr ? context.Containers()->BackgroundColor() : 0xFF202030) & 0xFFFFFF;
                const float tailX = std::clamp(value.x, bounds.x + 8, bounds.x + bounds.width - 8);
                context.Design().DrawTriangle(tailX - 6, bottom - 1, tailX + 6, bottom - 1, tailX, bottom + 7,
                    tailColor | (static_cast<uint32_t>(this->opacity_ * fade * 255) << 24));
            }
            auto y = bounds.y + this->padding_;
            for (size_t rowIndex = 0; rowIndex < value.rows.size(); ++rowIndex, y += lineHeight)
            {
                float x = bounds.x + this->padding_;
                for (const auto& span : value.rows[rowIndex].spans)
                {
                    D3DCOLOR color = 0xFFFFFFFF;
                    if (this->chat_colors_)
                    {
                        const auto base = snapshot.chat_colors.available ? snapshot.chat_colors.modes[value.mode & 255] : value.color;
                        const auto inlineColor = snapshot.chat_colors.available && span.color_index >= 0 && static_cast<size_t>(span.color_index) < snapshot.chat_colors.colors.size() ? snapshot.chat_colors.colors[span.color_index] : 0;
                        color = this->colors_ != nullptr ? this->colors_->Resolve(value.mode, ChatThemeColor(value.mode, base, context.Design().IsXiv1Theme()), inlineColor) : inlineColor != 0 ? inlineColor : ChatThemeColor(value.mode, base, context.Design().IsXiv1Theme());
                    }
                    auto profile = FadeBubbleText(TintTextProfile(this->text_, color), fade);
                    profile.placement = 0; profile.style.alignment = ui::text_alignment::left;
                    const ui::rect textBounds{x, y, bounds.x + bounds.width - this->padding_ - x, lineHeight};
                    DrawThemedChatText(context, span.text, textBounds, profile);
                    x += span.width;
                }
                if (prompt && rowIndex + 1 == value.rows.size())
                {
                    const float bounce = (1 - std::cos(static_cast<float>(now % 900) * (6.2831853f / 900))) * 1.5f;
                    const ui::rect marker{x + 2, y - bounce, std::max(0.0f, bounds.x + bounds.width - x - 4), lineHeight};
                    auto profile = FadeBubbleText(this->text_, fade);
                    profile.placement = 0; profile.style.alignment = ui::text_alignment::left;
                    DrawThemedChatText(context, "\xE2\x96\xBC", marker, profile);
                }
            }
        }
    }
}
