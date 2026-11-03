#include "native_hud_controller.hpp"

#include <cstdio>
#include <cstring>

namespace
{
    auto check(bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "native target check failed: %s\n", message);
        return condition;
    }

    struct target_fixture
    {
        Ashita::FFXI::targetwindow_t window{};
        std::array<uint8_t, 0xA0> primitive{};

        target_fixture(void)
        {
            window.m_pParentMCD = reinterpret_cast<uintptr_t>(primitive.data());
            const auto owner = reinterpret_cast<uintptr_t>(&window);
            std::memcpy(primitive.data() + 0x0C, &owner, sizeof(owner));
            primitive[0x69] = primitive[0x6A] = 1;
            window.FrameChildrenOffsetX = 120;
            window.FrameChildrenOffsetY = 400;
            window.IconPositionX = 125;
            window.IconPositionY = 405;
            window.m_AnkX = 640;
            window.m_AnkY = 200;
            window.m_SubAnkX = 800;
            window.m_SubAnkY = 250;
        }
    };
}

namespace xiui
{
    struct native_menu_test
    {
        static inline int calls{};
        static auto __fastcall Position(void* self, void*, const int x, const int y) -> void
        {
            const std::array<int16_t, 2> coordinates{static_cast<int16_t>(x), static_cast<int16_t>(y)};
            auto* bytes = static_cast<uint8_t*>(self);
            std::array<int16_t, 4> frame{}; std::memcpy(frame.data(), bytes + 0x3A, sizeof(frame));
            const std::array<int16_t, 4> moved{coordinates[0], coordinates[1], static_cast<int16_t>(x + frame[2] - frame[0]), static_cast<int16_t>(y + frame[3] - frame[1])};
            std::memcpy(bytes + 0x3A, moved.data(), sizeof(moved));
            std::memcpy(bytes + 0x52, coordinates.data(), sizeof(coordinates));
            ++calls;
        }

        static auto Anchors(void) -> bool
        {
            const auto link = [](auto& bytes, const size_t at, const auto address) { const auto p = reinterpret_cast<uintptr_t>(address); std::memcpy(bytes.data() + at, &p, 4); };
            struct panel
            {
                std::array<uint8_t, 0xD4> primitive{}, resource{};
                std::array<uint8_t, 0x180> owner{};
                std::array<uint8_t, 24> node{}, component_node{};
                std::array<uint8_t, 0x60> component{};
                std::array<uint8_t, 32> definition{};
                uintptr_t definition_pointer{};
                auto Height(const int16_t height) -> void
                {
                    int16_t top{}; std::memcpy(&top, primitive.data() + 0x3C, 2);
                    const int16_t bottom = top + height; std::memcpy(primitive.data() + 0x40, &bottom, 2);
                }
                auto Coordinates(void) const -> std::array<int16_t, 2>
                {
                    std::array<int16_t, 2> result{}; std::memcpy(result.data(), primitive.data() + 0x52, 4); return result;
                }
            } command, magic, details, money;
            const auto initialize = [&](panel& value, const char* name, const std::array<int16_t, 4>& frame, const uint8_t anchor)
            {
                link(value.primitive, 4, value.resource.data()); link(value.primitive, 0xC, value.owner.data());
                link(value.owner, 8, value.primitive.data()); link(value.node, 0x10, value.primitive.data());
                link(value.primitive, 0x14, value.component_node.data()); link(value.component_node, 0x10, value.component.data());
                value.definition_pointer = reinterpret_cast<uintptr_t>(value.definition.data());
                link(value.component, 0x30, &value.definition_pointer); value.definition[0x13] = anchor;
                std::memcpy(value.resource.data() + 0x46, name, 16);
                std::memcpy(value.primitive.data() + 0x3A, frame.data(), sizeof(frame));
                std::memcpy(value.primitive.data() + 0x42, frame.data(), sizeof(frame));
                std::memcpy(value.primitive.data() + 0x52, frame.data(), 4);
            };
            initialize(command, "menu    playermo", {16, 760, 128, 928}, 2);
            initialize(magic, "menu    magic   ", {130, 400, 450, 760}, 0);
            link(command.node, 0, magic.node.data());
            std::array<uint8_t, 0x84> manager{};
            link(manager, 0, command.node.data());
            const std::array<int16_t, 2> resolution{1920, 1080};
            std::memcpy(manager.data() + 0x80, resolution.data(), sizeof(resolution));
            native_menu_controller controller;
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            controller.state_.menu_available = true;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(command.Coordinates() == std::array<int16_t, 2>{300, 582} && magic.Coordinates() == std::array<int16_t, 2>{414, 390} &&
                controller.State().menu_count == 2 && controller.State().height == 360,
                "different-height command and magic panels share a bottom edge, retaining horizontal offsets")) return false;
            controller.Update({true, 400, 1000, 2560, 1440, true, 200});
            if (!check(command.Coordinates()[1] == 150 && magic.Coordinates()[1] == 150, "top anchoring aligns each panel's own top")) return false;
            const auto magicPointer = reinterpret_cast<uintptr_t>(magic.primitive.data());
            magic.Height(500);
            controller.BeforeDraw(magicPointer);
            if (!check(magic.Coordinates()[1] == 150, "growing a top-anchored list preserves its top edge")) return false;
            std::memcpy(magic.resource.data() + 0x46, "menu    ability ", 16); magic.definition[0x13] = 3;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(command.Coordinates()[1] == 582 && magic.Coordinates()[1] == 250, "ability lists with another DAT anchor share the selected bottom edge")) return false;
            magic.Height(140);
            controller.BeforeDraw(magicPointer);
            if (!check(magic.Coordinates()[1] == 610, "shrinking a bottom-anchored list preserves its bottom edge")) return false;
            controller.Shutdown();
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 760} && magic.Coordinates() == std::array<int16_t, 2>{130, 400},
                "shutdown restores both panels' distinct native positions")) return false;

            initialize(command, "menu    shop    ", {16, 48, 272, 238}, 0);
            initialize(magic, "menu    shopmain", {16, 256, 128, 296}, 0);
            initialize(details, "menu    iteminfo", {16, 240, 382, 296}, 0);
            initialize(money, "menu    money   ", {16, 240, 128, 296}, 0);
            link(magic.node, 0, details.node.data()); link(details.node, 0, money.node.data());
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position); controller.state_.menu_available = true;
            const auto shopCalls = calls;
            for (const char* name : {"menu    shop    ", "menu    shopbuy "})
            {
                std::memcpy(magic.resource.data() + 0x46, name, 16);
                for (const bool top : {false, true})
                {
                    controller.Update({true, 400, 1000, 2560, 1440, top, 200});
                    for (const auto* value : {&command, &magic, &details, &money})
                        controller.BeforeDraw(reinterpret_cast<uintptr_t>(value->primitive.data()));
                    if (!check(controller.State().menu_count == 0 && controller.State().menu_visible && calls == shopCalls &&
                        command.Coordinates() == std::array<int16_t, 2>{16, 48} && magic.Coordinates() == std::array<int16_t, 2>{16, 256} &&
                        details.Coordinates() == std::array<int16_t, 2>{16, 240} && money.Coordinates() == std::array<int16_t, 2>{16, 240},
                        "shop lists, details and gil retain native positions and input visibility under either command anchor")) return false;
                }
            }
            details.definition[0x13] = money.definition[0x13] = 2;
            controller.Update({true, 400, 1000, 2560, 1440});
            details.Height(120);
            controller.BeforeDraw(reinterpret_cast<uintptr_t>(details.primitive.data()));
            if (!check(controller.State().menu_count == 0 && calls == shopCalls && details.Coordinates()[1] == 240,
                "shop details stay native through resizing even with bottom-anchored frames")) return false;
            command.primitive[0x70] = magic.primitive[0x70] = 1;
            details.definition[0x13] = money.definition[0x13] = 0;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(controller.State().menu_count == 0 && !controller.State().menu_visible,
                "top-anchored details and gil stay excluded after the shop closes")) return false;
            details.definition[0x13] = 2;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(controller.State().menu_count == 1 && details.Coordinates() == std::array<int16_t, 2>{300, 630},
                "ordinary bottom-anchored item details remain supported outside shops")) return false;
            command.primitive[0x70] = magic.primitive[0x70] = 0;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(controller.State().menu_count == 0 && controller.State().menu_visible &&
                details.Coordinates() == std::array<int16_t, 2>{16, 240},
                "opening a shop restores previously moved item details to their native position")) return false;
            std::memcpy(magic.resource.data() + 0x46, "menu    shopmain", 16);
            for (const bool top : {false, true})
            {
                controller.Update({true, 400, 1000, 2560, 1440, top, 200});
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(magic.primitive.data()));
                if (!check(controller.State().menu_count == 1 && controller.State().menu_visible &&
                    controller.State().width == 112 && controller.State().height == 40 &&
                    magic.Coordinates() == std::array<int16_t, 2>{300, static_cast<int16_t>(top ? 150 : 710)} &&
                    command.Coordinates() == std::array<int16_t, 2>{16, 48} &&
                    details.Coordinates() == std::array<int16_t, 2>{16, 240} && money.Coordinates() == std::array<int16_t, 2>{16, 240},
                    "the main Buy/Sell choice follows the command anchor independently of native shop panels")) return false;
            }
            controller.Update({});
            if (!check(magic.Coordinates() == std::array<int16_t, 2>{16, 256},
                "disabling command movement restores the main Buy/Sell choice")) return false;
            initialize(magic, "menu    shopsell", {16, 888, 128, 928}, 2);
            for (const bool top : {false, true})
            {
                controller.Update({true, 400, 1000, 2560, 1440, top, 200});
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(magic.primitive.data()));
                if (!check(controller.State().menu_count == 1 && controller.State().menu_visible &&
                    magic.Coordinates() == std::array<int16_t, 2>{300, static_cast<int16_t>(top ? 150 : 710)} &&
                    command.Coordinates() == std::array<int16_t, 2>{16, 48} &&
                    details.Coordinates() == std::array<int16_t, 2>{16, 240} && money.Coordinates() == std::array<int16_t, 2>{16, 240},
                    "sell confirmation follows the command anchor without moving shop lists, details or gil")) return false;
            }
            controller.Update({});
            if (!check(magic.Coordinates() == std::array<int16_t, 2>{16, 888},
                "disabling command movement restores sell confirmation")) return false;
            magic.primitive[0x70] = money.primitive[0x70] = 1;
            for (const char* name : {"menu    equip   ", "menu    inspect "})
            {
                initialize(command, name, {16, 48, 198, 238}, 2);
                const auto nativeCalls = calls;
                for (const bool top : {false, true})
                {
                    controller.Update({true, 400, 1000, 2560, 1440, top, 200});
                    controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
                    controller.BeforeDraw(reinterpret_cast<uintptr_t>(details.primitive.data()));
                    if (!check(controller.State().menu_count == 0 && controller.State().menu_visible && calls == nativeCalls &&
                        command.Coordinates() == std::array<int16_t, 2>{16, 48} && details.Coordinates() == std::array<int16_t, 2>{16, 240},
                        "equipment, inspection and their item details stay native under either command anchor")) return false;
                }
                command.primitive[0x70] = 1;
                controller.Update({true, 400, 1000, 2560, 1440});
                if (!check(controller.State().menu_count == 1 && details.Coordinates() == std::array<int16_t, 2>{300, 630},
                    "item details remain movable outside equipment and inspection")) return false;
                command.primitive[0x70] = 0;
                controller.Update({true, 400, 1000, 2560, 1440});
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(details.primitive.data()));
                if (!check(controller.State().menu_count == 0 && details.Coordinates() == std::array<int16_t, 2>{16, 240},
                    "opening equipment or inspection restores managed details and leaves them native at draw time")) return false;
            }
            initialize(magic, "menu    comment ", {16, 872, 382, 928}, 2);
            magic.primitive[0x70] = 0;
            std::array<uint8_t, 20> commentVtable{};
            link(commentVtable, 0x10, &Position); link(magic.owner, 0, commentVtable.data());
            controller.comment_draw_ = reinterpret_cast<uintptr_t>(&Position);
            magic.owner[0x16] = 1;
            std::memcpy(magic.owner.data() + 0x95, "A real comment", 15);
            for (const bool top : {false, true})
            {
                controller.Update({true, 400, 1000, 2560, 1440, top, 200});
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(magic.primitive.data()));
                if (!check(controller.State().menu_count == 1 && controller.State().menu_visible &&
                    controller.State().width == 366 && controller.State().height == 56 &&
                    magic.Coordinates() == std::array<int16_t, 2>{300, static_cast<int16_t>(top ? 150 : 694)} &&
                    command.Coordinates() == std::array<int16_t, 2>{16, 48} && details.Coordinates() == std::array<int16_t, 2>{16, 240},
                    "bazaar comments follow the command anchor while inspection and its details stay native")) return false;
            }
            magic.owner[0x95] = 0;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())) && controller.State().menu_count == 0 &&
                controller.State().menu_visible && magic.Coordinates() == std::array<int16_t, 2>{16, 872},
                "empty viewed comments ignore stale bytes, leave command bounds and retain native input ownership")) return false;
            magic.owner[0x95 + 82] = 'X';
            if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())),
                "text on the third comment line becomes visible without waiting for a menu update")) return false;
            magic.owner[0x95 + 82] = 0;
            for (const uint8_t mode : std::array<uint8_t, 3>{0, 2, 3})
            {
                magic.owner[0x16] = mode;
                if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())),
                    "empty comment editors remain visible")) return false;
            }
            magic.owner[0x16] = 1;
            for (size_t line = 0; line < 3; ++line)
            {
                std::memset(magic.owner.data() + 0x95 + line * 41, ' ', 40);
                magic.owner[0x95 + line * 41 + 40] = 0;
            }
            if (!check(controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())),
                "whitespace-only viewed comments are hidden")) return false;
            controller.comment_draw_ = 0;
            if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())),
                "unsupported native comment readers leave the window visible")) return false;
            controller.Update({});
            if (!check(magic.Coordinates() == std::array<int16_t, 2>{16, 872},
                "disabling command movement restores bazaar comments")) return false;
            controller.Shutdown();
            return true;
        }

        static auto Run(void) -> bool
        {
            native_menu_controller controller{};
            std::array<uint8_t, 0xD4> primitive{}, owner{}, resource{};
            const auto pointer = reinterpret_cast<uintptr_t>(primitive.data());
            const auto ownerPointer = reinterpret_cast<uintptr_t>(owner.data()), resourcePointer = reinterpret_cast<uintptr_t>(resource.data());
            std::memcpy(primitive.data() + 4, &resourcePointer, 4);
            std::memcpy(primitive.data() + 0xC, &ownerPointer, 4);
            std::memcpy(owner.data() + 8, &pointer, 4);
            const std::array<int16_t, 2> original{16, 760};
            const std::array<int16_t, 4> frame{16, 760, 128, 928};
            std::memcpy(primitive.data() + 0x3A, frame.data(), sizeof(frame));
            std::memcpy(primitive.data() + 0x42, frame.data(), sizeof(frame));
            std::memcpy(primitive.data() + 0x52, original.data(), 4);
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            controller.menus_[0] = {pointer, ownerPointer, resourcePointer};
            controller.count_ = 1; controller.offset_x_ = 100; controller.anchor_y_ = 878;
            controller.layout_.enabled = true;
            const auto coordinates = [&] { std::array<int16_t, 2> result{}; std::memcpy(result.data(), primitive.data() + 0x52, 4); return result; };
            controller.BeforeDraw(pointer); controller.BeforeDraw(pointer);
            if (!check(coordinates() == std::array<int16_t, 2>{116, 710} && calls == 1, "menu movement uses native repositioning without accumulating offsets")) return false;
            const std::array<int16_t, 2> refreshed{20, 770};
            std::memcpy(primitive.data() + 0x52, refreshed.data(), 4);
            controller.BeforeDraw(pointer);
            if (!check(coordinates() == std::array<int16_t, 2>{116, 710}, "native chat shifts cannot move the configured menu anchor")) return false;
            controller.layout_.enabled = false; controller.BeforeDraw(pointer);
            if (!check(coordinates() == refreshed, "disabling movement restores the latest native position")) return false;
            controller.layout_.enabled = true; controller.BeforeDraw(pointer);
            controller.Move(controller.menus_[0], false);
            controller.Shutdown();
            if (!check(coordinates() == refreshed, "restoring a moved menu retains its latest native position")) return false;

            std::array<uint8_t, 0x84> manager{};
            std::array<uint8_t, 24> node{}, componentNode{};
            std::array<uint8_t, 0x60> component{};
            std::array<uint8_t, 32> definition{};
            const auto link = [](auto& bytes, const size_t at, const auto address) { const auto p = reinterpret_cast<uintptr_t>(address); std::memcpy(bytes.data() + at, &p, 4); };
            link(manager, 0, node.data()); link(node, 0x10, primitive.data());
            link(primitive, 0x14, componentNode.data()); link(componentNode, 0x10, component.data());
            uintptr_t definitionPointer = reinterpret_cast<uintptr_t>(definition.data());
            link(component, 0x30, &definitionPointer); definition[0x13] = 2;
            std::memcpy(resource.data() + 0x46, "menu    playermo", 16);
            const std::array<int16_t, 2> resolution{1920, 1080};
            std::memcpy(primitive.data() + 0x3A, frame.data(), sizeof(frame));
            std::memcpy(primitive.data() + 0x42, frame.data(), sizeof(frame));
            std::memcpy(primitive.data() + 0x52, original.data(), sizeof(original));
            std::memcpy(manager.data() + 0x80, resolution.data(), sizeof(resolution));
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            controller.state_.menu_available = true;
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(coordinates() == std::array<int16_t, 2>{300, 582} && controller.State().menu_count == 1 &&
                controller.State().reference_width == 1920, "menu groups translate from render pixels into the native UI resolution")) return false;
            for (const int y : {720, 640, 740, 760})
            {
                const std::array<int16_t, 2> shifted{300, static_cast<int16_t>(y)};
                std::memcpy(primitive.data() + 0x52, shifted.data(), sizeof(shifted));
                controller.BeforeDraw(pointer);
                controller.Update({true, 400, 1000, 2560, 1440});
                if (!check(coordinates() == std::array<int16_t, 2>{300, 582}, "repeated combat/chat layout changes retain the absolute anchor across draws and updates")) return false;
            }
            controller.Update({});
            if (!check(coordinates() == original, "disabling restores the latest native Y without retaining the managed X")) return false;
            controller.Update({true, 400, 1000, 2560, 1440});
            controller.Shutdown();
            if (!check(coordinates() == original, "unload restores menus still attached to the native list")) return false;
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            controller.state_.menu_available = true;
            std::memcpy(resource.data() + 0x46, "menu    logwin2 ", 16);
            controller.Update({true, 400, 1000, 2560, 1440});
            if (!check(controller.State().menu_count == 0 && coordinates() == original, "secondary chat never joins the command/details group")) return false;
            controller.Shutdown();

            uintptr_t tableOwner{}, table{};
            std::array<uintptr_t, 2> config{};
            std::array<uint32_t, 19 * 11> records{};
            table = reinterpret_cast<uintptr_t>(records.data()); config[1] = table;
            tableOwner = reinterpret_cast<uintptr_t>(config.data());
            controller.config_slot_ = reinterpret_cast<uintptr_t>(&tableOwner);
            auto* setting = records.data() + 18 * 11;
            setting[1] = 18; setting[2] = 7; setting[7] = 7;
            controller.Update({});
            if (!check(controller.State().window_type_available && controller.State().window_type == 7, "read Window Type from the validated native config record")) return false;
            setting[2] = 8; controller.Update({});
            return check(!controller.State().window_type_available, "invalid native theme selections fail closed");
        }
    };

    struct native_hud_target_test
    {
        static auto __stdcall MacroCallback(void* context, uint8_t book, uint8_t set, uint8_t slot, BOOL alt) -> BOOL
        {
            auto* calls = static_cast<uint32_t*>(context);
            if (book != 40 || set != 10 || slot != 9 || !alt) return FALSE;
            ++*calls; return TRUE;
        }

        static auto Fishing(void) -> bool
        {
            native_hud_controller controller;
            std::array<uint8_t, 0x80> owner{}, primitive{};
            std::array<uintptr_t, 11> table{};
            const auto ownerPointer = reinterpret_cast<uintptr_t>(owner.data());
            const auto primitivePointer = reinterpret_cast<uintptr_t>(primitive.data());
            const auto tablePointer = reinterpret_cast<uintptr_t>(table.data());
            std::memcpy(owner.data(), &tablePointer, 4); std::memcpy(owner.data() + 8, &primitivePointer, 4);
            std::memcpy(primitive.data() + 0x0C, &ownerPointer, 4);
            controller.slots_[static_cast<size_t>(native_hud_element::party)] = reinterpret_cast<uintptr_t>(&ownerPointer);
            controller.hidden_.store(NativeHudBit(native_hud_element::party));
            controller.party_update_ = table[10] = 1234;
            const auto stamina = [&](const int16_t value) { std::memcpy(owner.data() + 0x3A, &value, 2); };
            stamina(-1);
            if (!check(controller.ShouldHide(primitivePointer), "ordinary party rendering remains hidden while casting and waiting for a bite")) return false;
            if (!check(controller.FishingStaminaPercent() == -1, "inactive fishing does not expose a stamina value")) return false;
            for (const int16_t value : {100, 65, 0})
            {
                stamina(value);
                if (!check(controller.ShouldHide(primitivePointer) && controller.FishingStaminaPercent() == value,
                    "native party stays hidden while XIUI+ reads live fish stamina, including zero")) return false;
            }
            stamina(101);
            if (!check(controller.FishingStaminaPercent() == -1, "out-of-range native stamina is rejected")) return false;
            stamina(65);
            owner[0x40] = 1;
            if (!check(controller.ShouldHide(primitivePointer) && controller.FishingStaminaPercent() == -1,
                "alternate party preview mode cannot bypass hiding or masquerade as fishing")) return false;
            owner[0x40] = 0;
            table[10] = 4321;
            if (!check(controller.FishingStaminaPercent() == -1, "replaced native owner layouts are rejected")) return false;
            table[10] = 1234;
            std::memset(primitive.data() + 0x0C, 0, 4);
            if (!check(controller.FishingStaminaPercent() == -1, "detached native primitives are rejected")) return false;
            std::memcpy(primitive.data() + 0x0C, &ownerPointer, 4);
            controller.hidden_.store(0);
            return check(!controller.ShouldHide(primitivePointer) && controller.FishingStaminaPercent() == 65,
                "explicitly shown native party windows retain their own visibility choice");
        }

        static auto MacroDelay(void) -> bool
        {
            auto* image = static_cast<uint8_t*>(::VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
            if (!check(image != nullptr, "allocate native macro delay fixture")) return false;
            constexpr uint8_t ctrl[]{0x2B,0x46,0x10,0x3B,0xC3,0x0F,0x82,0x75,0x01,0,0,0x68,1,0,0,0,0xB9,2,0,0,0,0xC6,0x46,0x18,0,0x88,0x5E,0x0C};
            constexpr uint8_t alt[]{0x2B,0x46,0x10,0x3B,0xC3,0x72,0x79,0x68,3,0,0,0,0xB9,2,0,0,0,0xC6,0x46,0x18,0,0xC6,0x46,0x0C,2};
            const auto reset = [&] {
                std::memset(image, 0xCC, 0x1000);
                std::memcpy(image + 0x80, ctrl, sizeof(ctrl)); std::memcpy(image + 0x180, alt, sizeof(alt));
            };
            const auto original = [&] { return std::memcmp(image + 0x80, ctrl, sizeof(ctrl)) == 0 && std::memcmp(image + 0x180, alt, sizeof(alt)) == 0; };
            const auto patched = [&] {
                for (size_t i = 0; i < 6; ++i) if (image[0x85 + i] != 0x90) return false;
                return image[0x185] == 0x90 && image[0x186] == 0x90;
            };
            bool passed = true;
            {
                reset(); native_hud_controller host;
                host.module_ = reinterpret_cast<uintptr_t>(image); host.module_size_ = 0x1000; host.FindMacroDelay();
                host.Update(0);
                passed = check((host.AvailableMask() & native_instant_macro_palette) != 0 && original(), "both verified macro delays are available without changing default timing") && passed;
                host.Update(native_instant_macro_palette); host.Update(native_instant_macro_palette);
                passed = check(patched() && host.macro_delay_enabled_ && host.hidden_.load() == 0 && !host.macro_hook_enabled_,
                    "instant macro requests remove both delay branches without hiding or suppressing native macros") && passed;
                host.Update(0);
                passed = check(original() && !host.macro_delay_enabled_, "turning instant macros off restores both exact branches and leaves adjacent code intact") && passed;
                host.Update(native_instant_macro_palette); host.Update(native_instant_macro_palette | native_keyboard_macro_palette);
                passed = check(original() && !host.macro_delay_enabled_, "native macro replacement takes priority and restores the optional delay change") && passed;
                host.Update(native_instant_macro_palette); host.Shutdown();
                passed = check(original() && host.AvailableMask() == 0, "shutdown restores delay branches and clears discovery") && passed;
            }
            for (const bool both : {false, true})
            {
                reset(); std::memset(image + 0x85, 0x90, 6); if (both) std::memset(image + 0x185, 0x90, 2);
                native_hud_controller host;
                host.module_ = reinterpret_cast<uintptr_t>(image); host.module_size_ = 0x1000; host.FindMacroDelay();
                host.Update(native_instant_macro_palette);
                passed = check(patched() && !host.macro_delay_[0].owned && host.macro_delay_[1].owned == !both,
                    "existing macrofix patches remain unowned while only missing delay patches are applied") && passed;
                host.Shutdown();
                passed = check(image[0x85] == 0x90 && image[0x8A] == 0x90 && image[0x185] == (both ? 0x90 : 0x72),
                    "unload preserves external macrofix bytes and restores only XIUI+ changes") && passed;
            }
            for (int invalid = 0; invalid < 4; ++invalid)
            {
                reset();
                if (invalid == 0) std::memset(image + 0x180, 0xCC, sizeof(alt));
                if (invalid == 1) std::memcpy(image + 0x280, ctrl, sizeof(ctrl));
                if (invalid == 2) image[0x185] = 0xEB;
                if (invalid == 3) image[0x89] = 0x70;
                native_hud_controller host;
                host.module_ = reinterpret_cast<uintptr_t>(image); host.module_size_ = 0x1000; host.FindMacroDelay();
                host.Update(native_instant_macro_palette);
                passed = check((host.AvailableMask() & native_instant_macro_palette) == 0 && image[0x85] == 0x0F,
                    "missing, ambiguous, changed and out-of-bounds code leaves both native delay sites untouched") && passed;
            }
            {
                reset(); native_hud_controller host;
                host.module_ = reinterpret_cast<uintptr_t>(image); host.module_size_ = 0x1000; host.FindMacroDelay();
                host.Update(native_instant_macro_palette); image[0x85] = 0xCC; host.Update(native_instant_macro_palette);
                passed = check(!host.macro_delay_available_ && !host.macro_delay_enabled_ && image[0x85] == 0xCC && image[0x185] == 0x72,
                    "a later external code change disables instant macros, restores the other owned site and preserves foreign bytes") && passed;
            }
            ::VirtualFree(image, 0, MEM_RELEASE);
            return passed;
        }

        static auto Macros(void) -> bool
        {
            uint32_t calls{};
            native_macros runtimeMacros{}; runtimeMacros.SetRunner(&MacroCallback, &calls); runtimeMacros.Initialize();
            if (!check(runtimeMacros.Available() && runtimeMacros.Entry() == 0 && runtimeMacros.Run({40, 10, 9, true}) && calls == 1,
                "runtime initialization retains the host macro callback without scanning hooked client code")) return false;
            if (!check(!runtimeMacros.Run({0, 10, 9, true}) && !runtimeMacros.Run({40, 11, 9, true}) && calls == 1,
                "host macro callbacks retain native book and set bounds")) return false;

            auto* entry = static_cast<uint8_t*>(::VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
            if (!check(entry != nullptr, "allocate a bounded native entry fixture")) return false;
            std::memset(entry, 0x90, 0x1000);
            constexpr uint8_t body[]{0xFF, 0x01, 0xC2, 0x08, 0x00}; // inc [ecx]; ret 8
            constexpr uint8_t epilogue[]{0xC2, 0x08, 0x00};
            std::memcpy(entry, body, sizeof(body));
            std::memcpy(entry + 0x21, epilogue, sizeof(epilogue)); std::memcpy(entry + 0x40, epilogue, sizeof(epilogue));
            ::FlushInstructionCache(::GetCurrentProcess(), entry, 0x43);
            native_hud_controller host{};
            host.module_ = reinterpret_cast<uintptr_t>(entry); host.module_size_ = 0x1000; host.macro_entry_ = host.module_;
            host.native_macros_.SetRunEntry(host.macro_entry_);
            const auto nativeRun = reinterpret_cast<native_hud_controller::macro_run_fn>(entry);
            calls = 0; nativeRun(&calls, 1, 0);
            host.Update(native_keyboard_macro_palette);
            nativeRun(&calls, 1, 0); nativeRun(&calls, 2, 9);
            bool passed = check(host.macro_hook_enabled_ && calls == 1 && host.Status().find("macro guard active") != std::string::npos,
                "the requested macro guard blocks native starts even without HUD draw discovery or a logged-in fixture");
            reinterpret_cast<native_hud_controller::macro_run_fn>(host.native_macros_.Entry())(&calls, 2, 9);
            passed = check(calls == 2, "explicit XIUI+ macro references retain the original trampoline path") && passed;
            host.Update(0); nativeRun(&calls, 1, 0);
            passed = check(!host.macro_hook_enabled_ && calls == 3, "turning suppression off immediately restores native starts") && passed;
            host.Update(native_keyboard_macro_palette); host.Shutdown(); nativeRun(&calls, 1, 0);
            passed = check(calls == 4 && host.macro_entry_ == 0 && !host.macro_hook_created_, "shutdown removes the native guard and restores the original entry") && passed;
            ::VirtualFree(entry, 0, MEM_RELEASE);
            return passed;
        }

        static auto Run(void) -> bool
        {
            native_hud_controller controller{};
            target_fixture first{}, second{};
            const auto original = first.window;
            uintptr_t slot = reinterpret_cast<uintptr_t>(&first.window);
            controller.slots_[static_cast<size_t>(native_hud_element::target)] = reinterpret_cast<uintptr_t>(&slot);
            controller.hidden_ = NativeHudBit(native_hud_element::target);
            if (!check(!controller.ShouldHide(first.window.m_pParentMCD), "the target draw callback must continue")) return false;
            if (!check(first.window.FrameChildrenOffsetX == 30000 && first.window.IconPositionY == 30000 &&
                first.primitive[0x69] == 0 && first.primitive[0x6A] == 1,
                "hide the box frame and contents without disabling its callback")) return false;
            auto expected = original;
            expected.FrameChildrenOffsetX = expected.FrameChildrenOffsetY = expected.IconPositionX = expected.IconPositionY = 30000;
            if (!check(std::memcmp(&expected, &first.window, sizeof(expected)) == 0,
                "both arrow anchors and all other target fields remain untouched")) return false;
            controller.ShouldHide(first.window.m_pParentMCD);
            controller.Update(0);
            if (!check(std::memcmp(&original, &first.window, sizeof(original)) == 0 && first.primitive[0x69] == 1,
                "showing the box restores its original frame and coordinates")) return false;

            controller.hidden_ = NativeHudBit(native_hud_element::target);
            controller.ShouldHide(first.window.m_pParentMCD);
            first.window.FrameChildrenOffsetX = 150;
            controller.ShouldHide(first.window.m_pParentMCD);
            controller.Update(0);
            if (!check(first.window.FrameChildrenOffsetX == 150,
                "retain native coordinate updates that occur while the box is hidden")) return false;

            controller.hidden_ = NativeHudBit(native_hud_element::target);
            controller.ShouldHide(first.window.m_pParentMCD);
            slot = reinterpret_cast<uintptr_t>(&second.window);
            controller.ShouldHide(second.window.m_pParentMCD);
            first.window.FrameChildrenOffsetX = 999;
            second.window.IconPositionX = 777;
            controller.Update(0);
            if (!check(first.window.FrameChildrenOffsetX == 999 && second.window.IconPositionX == 777 &&
                second.window.FrameChildrenOffsetX == original.FrameChildrenOffsetX && second.primitive[0x69] == 1,
                "restore only the current owner and fields still carrying our hidden values")) return false;

            controller.slots_[static_cast<size_t>(native_hud_element::target)] = 0;
            controller.slots_[static_cast<size_t>(native_hud_element::player)] = reinterpret_cast<uintptr_t>(&slot);
            controller.hidden_ = NativeHudBit(native_hud_element::player);
            if (!check(controller.ShouldHide(second.window.m_pParentMCD), "other selected HUD elements still use full draw suppression")) return false;

            target_fixture chat{}, input{}, unrelated{};
            uintptr_t chatSlot = reinterpret_cast<uintptr_t>(&chat.window);
            uintptr_t inputSlot = reinterpret_cast<uintptr_t>(&input.window);
            controller.slots_[static_cast<size_t>(native_hud_element::chat1)] = reinterpret_cast<uintptr_t>(&chatSlot);
            controller.chat_input_slot_ = reinterpret_cast<uintptr_t>(&inputSlot);
            const auto originalInput = input;
            controller.hidden_ = NativeHudBit(native_hud_element::chat1);
            if (!check(controller.ShouldHide(chat.window.m_pParentMCD) && controller.ShouldHide(input.window.m_pParentMCD) &&
                !controller.ShouldHide(unrelated.window.m_pParentMCD), "main chat hiding suppresses both log and input, without affecting other windows")) return false;
            if (!check(std::memcmp(&input.window, &originalInput.window, sizeof(input.window)) == 0 && input.primitive == originalInput.primitive,
                "hiding chat input changes no native input state or callbacks")) return false;
            controller.hidden_ = NativeHudBit(native_hud_element::chat2) | NativeHudBit(native_hud_element::chatlog);
            if (!check(!controller.ShouldHide(input.window.m_pParentMCD), "secondary and expanded chat settings do not hide the main input")) return false;
            controller.hidden_ = 0;
            if (!check(!controller.ShouldHide(chat.window.m_pParentMCD) && !controller.ShouldHide(input.window.m_pParentMCD),
                "showing chat immediately restores the log and input")) return false;
            controller.hidden_ = NativeHudBit(native_hud_element::chat1);
            inputSlot = reinterpret_cast<uintptr_t>(&unrelated.window);
            if (!check(!controller.ShouldHide(input.window.m_pParentMCD) && controller.ShouldHide(unrelated.window.m_pParentMCD),
                "input suppression follows replacement owners and ignores stale primitives")) return false;
            inputSlot = 0;
            if (!check(!controller.ShouldHide(input.window.m_pParentMCD), "an unavailable input owner is never suppressed")) return false;
            std::array<target_fixture, 4> palettes{};
            std::array<uintptr_t, 4> paletteSlots{};
            for (size_t index = 0; index < palettes.size(); ++index)
            {
                paletteSlots[index] = reinterpret_cast<uintptr_t>(&palettes[index].window);
                controller.macro_palette_slots_[index] = reinterpret_cast<uintptr_t>(&paletteSlots[index]);
            }
            controller.hidden_ = 0;
            for (const auto& palette : palettes)
                if (!check(!controller.ShouldHide(palette.window.m_pParentMCD), "native macro palettes draw without an owned XIUI+ chord")) return false;
            controller.hidden_ = native_keyboard_macro_palette;
            for (const auto& palette : palettes)
            {
                const auto originalWindow = palette.window;
                const auto originalPrimitive = palette.primitive;
                if (!check(controller.ShouldHide(palette.window.m_pParentMCD) &&
                    std::memcmp(&originalWindow, &palette.window, sizeof(originalWindow)) == 0 && originalPrimitive == palette.primitive,
                    "owned XIUI+ chord suppresses all four native palette variants without mutating native state")) return false;
            }
            if (!check(!controller.ShouldHide(chat.window.m_pParentMCD) && !controller.ShouldHide(unrelated.window.m_pParentMCD),
                "transient palette suppression leaves chat and unrelated native menus visible")) return false;
            paletteSlots[0] = reinterpret_cast<uintptr_t>(&unrelated.window);
            if (!check(!controller.ShouldHide(palettes[0].window.m_pParentMCD) && controller.ShouldHide(unrelated.window.m_pParentMCD),
                "palette suppression follows the current owner and ignores stale palette primitives")) return false;
            paletteSlots[0] = 0;
            if (!check(!controller.ShouldHide(unrelated.window.m_pParentMCD), "missing palette owners are never suppressed")) return false;
            controller.Update(0);
            for (const auto& palette : palettes)
                if (!check(!controller.ShouldHide(palette.window.m_pParentMCD), "clearing chord ownership immediately restores native palette drawing")) return false;
            controller.Shutdown();
            return check(controller.chat_input_slot_ == 0 && controller.macro_palette_slots_ == std::array<uintptr_t, 4>{} &&
                !controller.ShouldHide(chat.window.m_pParentMCD),
                "shutdown clears input ownership and restores native drawing");
        }
    };
}

int main(int argc, char** argv)
{
    if (argc > 1 && std::strcmp(argv[1], "--macros-only") == 0)
    {
        if (!xiui::native_hud_target_test::Macros() || !xiui::native_hud_target_test::MacroDelay()) return 1;
        std::puts("Native macro suppression, instant palette timing, patch ownership and restoration passed."); return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--fishing-only") == 0)
    {
        if (!xiui::native_hud_target_test::Fishing()) return 1;
        std::puts("Native party suppression and fish-only gauge preservation passed."); return 0;
    }
    if (argc > 1 && std::strcmp(argv[1], "--menus-only") == 0)
    {
        if (!xiui::native_menu_test::Run() || !xiui::native_menu_test::Anchors()) return 1;
        std::puts("Native command-menu movement, top/bottom anchors, resizing and restoration passed.");
        return 0;
    }
    if (!xiui::native_hud_target_test::Run() || !xiui::native_hud_target_test::Macros() || !xiui::native_hud_target_test::MacroDelay() || !xiui::native_menu_test::Run() || !xiui::native_menu_test::Anchors()) return 1;
    std::puts("Native target preservation and chat log/input suppression checks passed.");
    return 0;
}
