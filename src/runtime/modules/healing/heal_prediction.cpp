#include "heal_prediction.hpp"
#include "ashita_compat.hpp"

#include <algorithm>
#include <cmath>

namespace xiui
{
    namespace
    {
        struct cure_segment { float limit, divisor, constant, power; };
        constexpr cure_segment cures[][6]{
            {{20,4,10,0}, {40,1.3333f,15,20}, {125,8.5f,30,40}, {200,15,40,125}, {600,20,40,200}},
            {{70,1,60,40}, {125,5.5f,90,70}, {200,7.5f,100,125}, {400,10,110,200}, {700,20,130,400}},
            {{125,2.2f,130,70}, {200,75.0f/65,155,125}, {300,2.5f,220,200}, {700,5,260,300}},
            {{200,1,270,70}, {300,2,400,200}, {400,10.0f/7,450,300}, {700,2.5f,520,400}},
            {{150,.7f,450,80}, {190,1.25f,550,150}, {260,70.0f/38,582,190}, {300,2,620,260}, {500,2.5f,640,300}, {700,10.0f/3,720,500}},
            {{210,1.5f,600,90}, {300,.9f,680,210}, {400,10.0f/7,780,300}, {500,2.5f,850,400}, {700,5.0f/3,890,500}}
        };
        constexpr float minimums[]{10,60,130,270,450,600}, caps[]{65,145,340,640,780,1010};
        auto Supported(const uint32_t spell) -> bool { return spell >= 1 && spell <= 11; }
        auto Valid(const party_member& member) -> bool
        { return member.active && member.in_zone && !member.preview && member.id != 0 && member.hp > 0 && member.hp_max >= member.hp; }
    }

    auto EstimateHeal(const uint32_t spell, const healing_stats& stats, const float bonus) -> uint32_t
    {
        if (!Supported(spell) || stats.mind > 2000 || stats.vitality > 2000 || stats.skill > 2000 || !std::isfinite(bonus)) return 0;
        float base{};
        if (spell <= 6)
        {
            const auto power = static_cast<float>(stats.mind / 2 + stats.vitality / 4 + stats.skill);
            base = caps[spell - 1];
            for (const auto& segment : cures[spell - 1])
                if (segment.limit > 0 && power < segment.limit)
                { base = (power - segment.power) / segment.divisor + segment.constant; break; }
        }
        else
        {
            const float power = static_cast<float>(3 * stats.mind + stats.vitality + 3 * (stats.skill / 5));
            float divisor{}, constant{};
            switch (spell)
            {
            case 7: divisor = power > 170 ? 35.6666f : power > 110 ? 2.0f : 1.0f; constant = power > 170 ? 87.62f : power > 110 ? 47.5f : 20.0f; break;
            case 8: divisor = power > 300 ? 15.6666f : power > 180 ? 2.0f : 1.0f; constant = power > 300 ? 180.43f : power > 180 ? 115.0f : 70.0f; break;
            case 9: divisor = power > 460 ? 6.5f : power > 220 ? 2.0f : .6666f; constant = power > 460 ? 354.6666f : power > 220 ? 275.0f : 165.0f; break;
            case 10: divisor = power > 560 ? 2.8333f : power > 320 ? 1.0f : .6666f; constant = power > 560 ? 591.2f : power > 320 ? 410.0f : 330.0f; break;
            default: divisor = power > 780 ? 2.667f : 1.0f; constant = power > 780 ? 814.0f : 570.0f; break;
            }
            base = power / 2 / divisor + constant;
        }
        const auto tier = spell <= 6 ? spell - 1 : spell - 6;
        auto result = std::floor(std::max(base, minimums[tier]));
        result = std::floor(result * (1 + std::clamp(bonus, 0.0f, 100.0f) / 100));
        if (stats.rapture) result = std::floor(result * 1.5f);
        if (stats.divine_seal) result *= 2;
        return static_cast<uint32_t>(result);
    }

    auto HealRecipient(const player_cast_state& cast, const uint32_t selectedSpell, const uint32_t selectedTarget) -> uint32_t
    {
        if (cast.Active()) return !cast.item && cast.interrupted == 0 && Supported(cast.id) ? cast.target_id : 0;
        return Supported(selectedSpell) ? selectedTarget : 0;
    }

    auto heal_prediction_source::Update(IAshitaCore* core, const party_source& party,
        const player_cast_source& cast, const cast_cost_source& selection, const uint64_t now) -> void
    {
        this->Clear();
        auto* memory = core != nullptr ? core->GetMemoryManager() : nullptr;
        auto player = memory != nullptr ? xiui::ashita::View(memory->GetPlayer()) : nullptr;
        auto target = memory != nullptr ? xiui::ashita::View(memory->GetTarget()) : nullptr;
        auto entities = memory != nullptr ? xiui::ashita::View(memory->GetEntity()) : nullptr;
        if (player == nullptr || target == nullptr || entities == nullptr || !party.Available() || party.InCutscene() || player->GetIsZoning() ||
            player->GetLoginStatus() != static_cast<uint8_t>(Ashita::FFXI::Enums::LoginStatus::LoggedIn)) return;
        const auto& members = party.Members();
        if (!Valid(members[0])) return;
        const auto& casting = cast.State();
        const auto& selected = selection.State();
        const bool selecting = !casting.Active() && selection.MPPreview(now).Fresh(now) && selected.EnoughResources() &&
            (!selected.recast_known || selected.remaining <= 0);
        const auto spell = casting.Active() ? casting.id : selecting ? selected.id : 0;
        uint32_t selectedTarget{};
        if (selecting)
        {
            const bool subtarget = target->GetIsSubTargetActive() != 0;
            const auto index = target->GetTargetIndex(subtarget ? 1 : 0);
            if (index > 0 && index < entities->GetEntityMapSize()) selectedTarget = entities->GetServerId(index);
            if (subtarget && std::none_of(members.begin(), members.end(), [selectedTarget](const auto& member) { return member.id == selectedTarget && selectedTarget != 0; }))
                for (const auto& member : members)
                    if (member.subtarget) { selectedTarget = member.id; break; }
        }
        const auto recipient = HealRecipient(casting, selecting ? spell : 0, selectedTarget);
        const auto center = std::find_if(members.begin(), members.end(), [&](const auto& member) { return member.id == recipient && Valid(member); });
        if (recipient == 0 || center == members.end()) return;
        const int mind = player->GetStat(5) + player->GetStatModifier(5), vitality = player->GetStat(2) + player->GetStatModifier(2);
        if (mind <= 0 || vitality <= 0) return;
        healing_stats stats{static_cast<uint32_t>(mind), static_cast<uint32_t>(vitality), player->GetCombatSkill(33).GetSkill()};
        if (const auto* buffs = player->GetBuffs())
            for (size_t i = 0; i < 32; ++i)
            { stats.divine_seal |= buffs[i] == 78; stats.rapture |= buffs[i] == 364; }
        const auto amount = EstimateHeal(spell, stats, this->bonus_);
        if (amount == 0) return;
        const auto position = [&](const party_member& member) {
            return std::array<float, 3>{entities->GetLocalPositionX(member.index), entities->GetLocalPositionY(member.index), entities->GetLocalPositionZ(member.index)};
        };
        for (size_t i = 0; i < members.size(); ++i)
        {
            const auto& member = members[i];
            if (!Valid(member)) continue;
            if (member.id != recipient)
            {
                if (spell <= 6 || member.slot / 6 != center->slot / 6 || !member.entity_valid || !center->entity_valid) continue;
                const auto a = position(member), b = position(*center);
                float squared{};
                for (size_t axis = 0; axis < 3; ++axis) squared += (a[axis] - b[axis]) * (a[axis] - b[axis]);
                if (!std::isfinite(squared) || squared > 100) continue;
            }
            this->entries_[i] = {member.id, amount, member.hp, member.hp_max, member.hp_percent, member.name};
        }
        this->primary_ = recipient; this->sampled_ = now;
    }

    auto heal_prediction_source::Find(const uint32_t id, const uint64_t now) const -> const heal_prediction*
    {
        if (id == 0 || now < this->sampled_ || now - this->sampled_ > 250) return nullptr;
        for (const auto& entry : this->entries_) if (entry.id == id) return &entry;
        return nullptr;
    }
    auto heal_prediction_source::Load(const settings_store& store) -> void
    {
        const auto bonus = store.ReadFloat("healing_prediction", "bonus", 0);
        this->bonus_ = std::isfinite(bonus) ? std::clamp(bonus, 0.0f, 100.0f) : 0;
        this->Clear();
    }
    auto heal_prediction_source::Save(const settings_store& store) const -> void { store.WriteFloat("healing_prediction", "bonus", this->bonus_); }
    auto heal_prediction_source::BonusOption(void) -> module_option
    { return module_option::Slider("Healing potency bonus (%)", "Shared by all heal previews. Set your total gear potency bonus; hidden weather and received-healing bonuses remain approximate.", &this->bonus_, 0, 100, 1).Ordered(83); }
    auto heal_preview_settings::Load(const settings_store& store, const char* section) -> void
    {
        this->enabled = store.ReadBool(section, "heal_prediction", true); this->color = store.ReadArgb(section, "heal_prediction_color", 0xD066FF99);
        this->overheal = store.ReadBool(section, "heal_prediction_overheal", true);
        this->overheal_color = store.ReadArgb(section, "heal_prediction_overheal_color", 0xFFFF5FC8);
        if (store.ReadInt(section, "heal_prediction_overheal_style_version", 0) < 1 && this->overheal_color == 0xD0FF80C0)
            this->overheal_color = 0xFFFF5FC8;
    }
    auto heal_preview_settings::Save(const settings_store& store, const char* section) const -> void
    {
        store.WriteBool(section, "heal_prediction", this->enabled); store.WriteArgb(section, "heal_prediction_color", this->color);
        store.WriteBool(section, "heal_prediction_overheal", this->overheal);
        store.WriteInt(section, "heal_prediction_overheal_style_version", 1);
        store.WriteArgb(section, "heal_prediction_overheal_color", this->overheal_color);
    }
    auto heal_preview_settings::Option(const size_t index, heal_prediction_source* source) -> module_option
    {
        if (index == 0) return module_option::Toggle("Heal prediction", "Show approximate Cure / Curaga healing beyond current HP while selecting or casting your spell.", &this->enabled).Ordered(82);
        if (index == 1) return module_option::Color("Heal prediction color", "Color of the estimated HP gain.", &this->color).Ordered(590, true);
        if (index == 3) return module_option::Toggle("Overheal prediction", "Show wasted healing as a striped pink overlay on existing HP. Segment length represents the excess heal relative to maximum HP.", &this->overheal).Ordered(84);
        if (index == 4) return module_option::Color("Overheal color", "Base color of the striped overlay for estimated healing that exceeds missing HP.", &this->overheal_color).Ordered(591);
        if (source != nullptr) return source->BonusOption();
        auto option = module_option::Toggle("Healing data unavailable", "Healing source is not connected.", &this->enabled);
        option.available = false; return option;
    }
    auto heal_preview_settings::Ranges(const heal_prediction_source* source, const uint32_t id, const uint64_t now, const float ratio) const -> std::array<ui::meter_range, 2>
    {
        const auto* entry = this->enabled && source != nullptr ? source->Find(id, now) : nullptr;
        if (entry == nullptr) return {};
        return {HealPreviewRange(*entry, this->color, ratio), this->overheal ? OverhealPreviewRange(*entry, this->overheal_color, ratio) : ui::meter_range{}};
    }
    auto HealPreviewRange(const heal_prediction& entry, const D3DCOLOR color, const float ratio) -> ui::meter_range
    {
        if (entry.maximum == 0 || entry.hp == 0 || entry.hp >= entry.maximum || entry.amount == 0 || !std::isfinite(ratio)) return {};
        const float start = std::clamp(ratio >= 0 ? ratio : entry.percent / 100.0f, 0.0f, 1.0f);
        const float end = std::min(1.0f, start + static_cast<float>(entry.amount) / entry.maximum);
        return {start, end, MeterGradientTop(color), color, true};
    }

    auto OverhealPreviewRange(const heal_prediction& entry, const D3DCOLOR color, const float ratio) -> ui::meter_range
    {
        if (entry.maximum == 0 || entry.hp == 0 || entry.hp > entry.maximum || !std::isfinite(ratio)) return {};
        const auto missing = entry.maximum - entry.hp;
        if (entry.amount <= missing) return {};
        const float end = std::clamp(ratio >= 0 ? ratio : entry.percent / 100.0f, 0.0f, 1.0f);
        const float excess = static_cast<float>(entry.amount - missing) / entry.maximum;
        return {std::max(0.0f, end - excess), end, MeterGradientTop(color), color, true, true};
    }
}
