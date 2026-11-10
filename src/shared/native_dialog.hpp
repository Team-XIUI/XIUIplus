#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xiui
{
    enum class native_dialog : uint8_t
    {
        commands, actions, details, merchant, auction, bazaar, equipment, macros, other, main_menu,
        status, inventory, synthesis, party_menu, trade, search, linkshell, region, map, quests, missions,
        key_items, friends, native_config, mog_house, guide, emotes, count
    };
    inline constexpr std::array<const char*, static_cast<size_t>(native_dialog::count)> native_dialog_keys{
        "commands", "actions", "details", "merchant", "auction", "bazaar", "equipment", "macros", "other", "mainmenu",
        "statusmenu", "inventory", "synthesis", "partymenu", "trade", "search", "linkshell", "region", "map", "quests", "missions",
        "keyitems", "friends", "config", "moghouse", "guide", "emotes"};
    inline constexpr std::array<const char*, static_cast<size_t>(native_dialog::count)> native_dialog_labels{
        "Command Menu", "Spells and Abilities", "Item Details", "Merchants", "Auction House", "Bazaar Comments", "Equipment and Inspection", "Macro Palette", "Other Dialogs", "Main Menu",
        "Status Menus", "Inventory Menus", "Synthesis", "Party Menus", "Trade", "Search", "Linkshell", "Region Info", "Map", "Quests", "Missions",
        "Key Items", "Friends and Messages", "Game Configuration", "Mog House", "Help and Tutorials", "Emotes"};
}
