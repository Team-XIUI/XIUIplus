#include "blue_magic_module.hpp"
#include "ui/game_icon_cache.hpp"

#include <cmath>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace xiui
{
    auto blue_magic_module::Clear(void) -> void
    {
        this->alert_.Clear(); this->preview_active_ = false; this->audio_.Stop();
    }

    auto blue_magic_module::PlaySound(void) -> void
    {
        if (!this->audio_ready_)
        { this->audio_.Initialize(reinterpret_cast<HMODULE>(&__ImageBase), 601); this->audio_ready_ = true; }
        this->audio_.Play(0);
    }

    auto blue_magic_module::Update(const uint64_t now, const bool configOpen) -> void
    {
        this->now_ = now;
        const auto* local = this->source_ != nullptr && this->source_->Available() ? &this->source_->Members()[0] : nullptr;
        const auto id = local != nullptr && local->active && local->in_zone ? local->id : 0;
        const auto zone = id != 0 ? local->zone : 0;
        const auto job = id != 0 ? local->main_job : 0, subjob = id != 0 ? local->sub_job : 0;
        if (id != this->player_id_ || zone != this->zone_ || job != this->job_ || subjob != this->subjob_)
        {
            this->Clear(); this->player_id_ = id; this->zone_ = zone;
            this->job_ = static_cast<uint8_t>(job); this->subjob_ = static_cast<uint8_t>(subjob);
        }
        if (!this->state_.enabled) this->alert_.Clear();
        if (!configOpen) this->preview_active_ = false;
        if (this->preview_active_ && !BlueAlertFrame(now - this->preview_started_, this->duration_).visible) this->preview_active_ = false;
        if (id != 0 && this->state_.enabled && this->alert_.Tick(now, this->duration_) && this->sound_) this->PlaySound();
        this->state_.bounds.width = 720 * this->scale_; this->state_.bounds.height = 200 * this->scale_;
    }

    auto blue_magic_module::HandlePacket(IAshitaCore* core, const uint16_t id, const std::span<const uint8_t> data, const uint64_t now) -> void
    {
        if (id == 0xA || id == 0xB) { this->Clear(); return; }
        if (id != 0x29 || !this->state_.enabled || this->player_id_ == 0) return;
        const auto learned = DecodeBlueLearned(data, this->player_id_);
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto* player = memory != nullptr ? memory->GetPlayer() : nullptr;
        auto* party = memory != nullptr ? memory->GetParty() : nullptr;
        if (!learned || player == nullptr || party == nullptr ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn) ||
            party->GetMemberServerId(0) != this->player_id_ || party->GetMemberZone(0) != this->zone_) return;
        auto* resources = core->GetResourceManager();
        const auto* spell = resources != nullptr ? resources->GetSpellById(*learned) : nullptr;
        if (spell == nullptr || spell->Skill != static_cast<uint16_t>(Ashita::FFXI::Enums::SkillType::BlueMagic)) return;
        const auto* name = spell->Name[2] != nullptr && *spell->Name[2] != '\0' ? spell->Name[2] : spell->Name[0];
        if (name == nullptr || *name == '\0') return;
        this->alert_.Enqueue(*learned, now);
    }

    auto blue_magic_module::ResetDefaults(const ui::theme& theme) -> void
    {
        this->Clear(); this->state_.enabled = false; this->scale_ = 1; this->duration_ = 3; this->sound_ = true;
        this->ApplyTextDefaults(theme);
    }

    auto blue_magic_module::Load(const settings_store& settings) -> void
    {
        this->Clear();
        this->state_.bounds = settings.ReadRect("layout.bluemagic", this->state_.bounds);
        this->state_.enabled = settings.ReadBool("bluemagic", "enabled", false);
        this->sound_ = settings.ReadBool("bluemagic", "sound", this->sound_);
        const auto number = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = settings.ReadFloat("bluemagic", key, fallback);
            return std::clamp(std::isfinite(value) ? value : fallback, minimum, maximum);
        };
        this->scale_ = number("scale", 1, 0.25f, 2); this->duration_ = number("duration", 3, 1, 10);
        this->fallback_ = LoadTextProfile(settings, "text.bluemagic.fallback", this->fallback_);
        this->state_.bounds.width = 720 * this->scale_; this->state_.bounds.height = 200 * this->scale_;
    }

    auto blue_magic_module::Save(const settings_store& settings) const -> void
    {
        settings.WriteRect("layout.bluemagic", this->state_.bounds);
        settings.WriteBool("bluemagic", "enabled", this->state_.enabled); settings.WriteBool("bluemagic", "sound", this->sound_);
        settings.WriteFloat("bluemagic", "scale", this->scale_); settings.WriteFloat("bluemagic", "duration", this->duration_);
        SaveTextProfile(settings, "text.bluemagic.fallback", this->fallback_);
    }

    auto blue_magic_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "A local Blue Magic learned event plays the original XIUI+ banner and optional sound.", "general"},
            {"Layout", "Drag the banner in edit mode and adjust its size and hold time.", "layout"},
            {"Text", "Fallback label style, used if the embedded artwork is unavailable.", "text"}};
        return sections;
    }

    auto blue_magic_module::ContentConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (index >= base_options) return section == 2;
        if (index == scale || index == duration) return section == 1;
        return section == 0;
    }

    auto blue_magic_module::ContentOption(const size_t index) -> module_option
    {
        switch (index)
        {
        case enabled: return module_option::Toggle("Enabled", "Celebrate validated Blue Magic learned messages for your character.", &this->state_.enabled).Ordered(-50);
        case scale: return module_option::Slider("Banner scale", "Preserve the original artwork's aspect ratio.", &this->scale_, 0.25f, 2, 0.05f);
        case duration: return module_option::Slider("Hold time", "Seconds between the entrance pulse and fade.", &this->duration_, 1, 10, 0.5f);
        case sound: return module_option::Toggle("Sound", "Play the original learned sound once per accepted event.", &this->sound_);
        case preview: return module_option::Action("Preview alert", "Preview the animation without creating a learned event or playing audio.", "Preview");
        case test_sound: return module_option::Action("Test sound", "Play the embedded learned clip once.", "Play");
        default: return TextProfileOption(index - base_options, this->fallback_);
        }
    }

    auto blue_magic_module::OnContentOptionChanged(const size_t index) -> void
    {
        if (index == enabled && !this->state_.enabled) this->Clear();
        if (index == sound && !this->sound_) this->audio_.Stop();
        if (index == preview) { this->preview_active_ = true; this->preview_started_ = ::GetTickCount64(); }
        if (index == test_sound) this->PlaySound();
    }

    auto blue_magic_module::ShouldRender(const hud_snapshot&) const -> bool
    { return this->state_.enabled && (this->preview_active_ || this->alert_.Frame(this->now_, this->duration_).visible); }

    auto blue_magic_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        auto frame = this->alert_.Frame(this->now_, this->duration_);
        if (this->preview_active_) frame = BlueAlertFrame(this->now_ - this->preview_started_, this->duration_);
        else if (context.IsEditing() && !frame.visible) frame = BlueAlertFrame(1000, this->duration_);
        if (!frame.visible) return;
        const auto& bounds = this->state_.bounds;
        const auto height = bounds.height * frame.scale, width = height * 2;
        const ui::rect art{bounds.x + (bounds.width - width) / 2, bounds.y + (bounds.height - height) / 2, width, height};
        auto* texture = context.Icons() != nullptr ? context.Icons()->BlueMagicBanner() : nullptr;
        const auto alpha = static_cast<uint32_t>(frame.alpha * 255) << 24;
        if (texture != nullptr)
        {
            auto left = art, right = art;
            left.x -= bounds.width * frame.entrance; right.x += bounds.width * frame.entrance;
            context.Design().DrawTexture(texture, left, alpha | 0xFFFFFF);
            context.Design().DrawTexture(texture, right, (static_cast<uint32_t>(frame.alpha * 150) << 24) | 0xFFFFFF);
        }
        else context.DrawText("Blue Magic Learned", art, art, TintTextProfile(this->fallback_, alpha | 0xFFFFFF));
    }
}
