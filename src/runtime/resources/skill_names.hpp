#pragma once

#include "Ashita.h"

#include <string>

namespace xiui::resources
{
    inline auto SkillName(const uint32_t id) -> std::string
    {
        using skill = Ashita::FFXI::Enums::SkillType;
        switch (static_cast<skill>(id))
        {
        case skill::HandToHand: return "Hand-to-Hand";
        case skill::Dagger: return "Dagger";
        case skill::Sword: return "Sword";
        case skill::GreatSword: return "Great Sword";
        case skill::Axe: return "Axe";
        case skill::GreatAxe: return "Great Axe";
        case skill::Scythe: return "Scythe";
        case skill::Polarm: return "Polearm";
        case skill::Katana: return "Katana";
        case skill::GreatKatana: return "Great Katana";
        case skill::Club: return "Club";
        case skill::Staff: return "Staff";
        case skill::AutomatonMelee: return "Automaton Melee";
        case skill::AutomatonRanged: return "Automaton Ranged";
        case skill::AutomatonMagic: return "Automaton Magic";
        case skill::Archery: return "Archery";
        case skill::Marksmanship: return "Marksmanship";
        case skill::Throwing: return "Throwing";
        case skill::Guard: return "Guarding";
        case skill::Evasion: return "Evasion";
        case skill::Shield: return "Shield";
        case skill::Parry: return "Parrying";
        case skill::Divine: return "Divine Magic";
        case skill::Healing: return "Healing Magic";
        case skill::Enhancing: return "Enhancing Magic";
        case skill::Enfeebling: return "Enfeebling Magic";
        case skill::Elemental: return "Elemental Magic";
        case skill::Dark: return "Dark Magic";
        case skill::Summoning: return "Summoning Magic";
        case skill::Ninjutsu: return "Ninjutsu";
        case skill::Singing: return "Singing";
        case skill::String: return "String Instrument";
        case skill::Wind: return "Wind Instrument";
        case skill::BlueMagic: return "Blue Magic";
        case skill::Geomancy: return "Geomancy";
        case skill::Handbell: return "Handbell";
        case skill::Fishing: return "Fishing";
        case skill::Woodworking: return "Woodworking";
        case skill::Smithing: return "Smithing";
        case skill::Goldsmithing: return "Goldsmithing";
        case skill::Clothcraft: return "Clothcraft";
        case skill::Leathercraft: return "Leathercraft";
        case skill::Bonecraft: return "Bonecraft";
        case skill::Alchemy: return "Alchemy";
        case skill::Cooking: return "Cooking";
        case skill::Synergy: return "Synergy";
        case skill::ChocoboDigging: return "Chocobo Digging";
        default: return "Skill " + std::to_string(id);
        }
    }
} // namespace xiui::resources
