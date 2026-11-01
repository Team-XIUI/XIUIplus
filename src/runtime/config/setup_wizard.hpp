#pragma once

#include "design_system.hpp"

#include <array>
#include <bitset>

namespace xiui::ui
{
    enum class setup_control : uint8_t { next, back, skip, enable, disable, settings, castbar, recast, buffs, pet, experience, hotbars, chat_native, chat_combined, none };

    struct setup_page
    {
        const char* title;
        const char* description;
        const char* tip;
    };

    inline constexpr std::array setup_pages{
        setup_page{"Welcome to XIUI", "XIUI gives FFXI a movable, customizable HUD. Use the parts you like and keep the game's HUD for the rest.",
            "We'll show you the basics. You can change any of these choices later."},
        setup_page{"Make it yours", "Your changes save automatically to your current profile. Profiles let you keep different layouts without starting over.",
            "Open /xiui edit and use the + beside the profile name to create one. Use the profile menu to switch. Edit mode also lets you move and resize your HUD."},
        setup_page{"Player Bar", "Your HP, MP and TP in one place, with room for your name, job and combat timers.",
            "Use XIUI's bar, or the game's status panel in the bottom right."},
        setup_page{"Chat", "Choose how XIUI displays your chat. Keep the game's windows separate or combine them into one log, using the game's filters.",
            "Combined merges both native logs into one. Follow keeps the game's window count and each window's routing."},
        setup_page{"Target Bar", "Your selected target's health and distance, with cast progress and status details.",
            "Use XIUI's target bar, or keep the game's target box."},
        setup_page{"Party List", "Your party's health, MP, TP and status effects. Select a member from the list to target them.",
            "The full party appears when you join one. Choose the look later in Party settings."},
        setup_page{"Minimap", "A small map of the area, with your position and nearby markers.",
            "Keep it on for a quick look around. Your normal map still works."},
        setup_page{"Enemy List", "Nearby enemies, their health and what they're casting. Select a row to target that enemy.",
            "Sample enemies are shown here so you can see the layout."},
        setup_page{"A few more to try", "Cast timers, cooldowns, buffs and more are available too. Turn on anything you want below.",
            "Open settings for the full module list. Reopen this guide any time with /xiui setup."}
    };

    struct setup_session
    {
        bool pending{}, open{}, show_focus{true};
        size_t step{};
        setup_control focus{setup_control::next}, pressed{setup_control::none};
        uint32_t pointer_buttons{};
        uint32_t revealed{};
        std::bitset<256> key_releases{};
    };

    struct setup_item
    {
        setup_control id;
        rect bounds;
        const char* label;
        bool selected{};
    };
}
