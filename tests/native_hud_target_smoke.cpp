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
        static inline native_menu_controller* drawing_controller{};
        static inline int draw_calls{};
        static auto __fastcall Draw(void* self, void*) -> void
        {
            ++draw_calls;
            drawing_controller->BeforeDraw(reinterpret_cast<uintptr_t>(self));
        }
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

        static auto __fastcall Frame(void* self, void*, int16_t* result) -> void
        { std::memcpy(result, static_cast<uint8_t*>(self) + 0x3A, 8); }

        static auto Windows(void) -> bool
        {
            const auto link = [](auto& bytes, const size_t at, const auto address) { const auto p = reinterpret_cast<uintptr_t>(address); std::memcpy(bytes.data() + at, &p, 4); };
            struct panel
            {
                std::array<uint8_t, 0xD4> primitive{}, resource{};
                std::array<uint8_t, 0x200> owner{};
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
            } command, magic, details, money, storage;
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
            std::array<uint8_t, 0x84> manager{};
            const std::array<int16_t, 2> resolution{1920, 1080};
            std::memcpy(manager.data() + 0x80, resolution.data(), sizeof(resolution));
            native_menu_controller controller;
            for (const char* choice : {"query   ", "guildsho", "prvdungp", "prvdunsi", "menuwind"})
                for (const char* tooltip : {"iteminfo", "itemxinf", "itemyinf"})
                {
                    command = {}; magic = {}; details = {};
                    initialize(command, "menu    playermo", {16, 760, 128, 928}, 2);
                    const auto choiceName = std::string{"menu    "} + choice, tooltipName = std::string{"menu    "} + tooltip;
                    initialize(magic, choiceName.c_str(), {1100, 600, 1500, 928}, 0);
                    initialize(details, tooltipName.c_str(), {350, 400, 950, 540}, 2);
                    link(manager, 0, command.node.data()); link(command.node, 0, magic.node.data()); link(magic.node, 0, details.node.data());
                    controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
                    controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
                    native_menu_layout layout{}; layout.viewport_width = 1920; layout.viewport_height = 1080;
                    const auto commands = native_hud_window_count + NativeMenuWindowIndex("playermo");
                    const auto choices = native_hud_window_count + NativeMenuWindowIndex(choice);
                    const auto description = native_hud_window_count + NativeMenuWindowIndex(tooltip);
                    layout.windows[commands] = {true, false, 700, 300}; controller.Update(layout);
                    controller.BeforeDraw(reinterpret_cast<uintptr_t>(magic.primitive.data())); controller.BeforeDraw(reinterpret_cast<uintptr_t>(details.primitive.data()));
                    if (!check(command.Coordinates() == std::array<int16_t, 2>{700, 300} && magic.Coordinates() == std::array<int16_t, 2>{1100, 600} &&
                        details.Coordinates() == std::array<int16_t, 2>{350, 400}, "moving Command Menu leaves NPC choices and item tooltips at their own native positions")) return false;
                    command.primitive[0x70] = 1;
                    layout.windows[choices] = {true, true, 1000, 500}; layout.windows[description] = {true, false, 100, 200};
                    controller.Update(layout);
                    if (!check(magic.Coordinates() == std::array<int16_t, 2>{1000, 500} && details.Coordinates() == std::array<int16_t, 2>{100, 200} &&
                        controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())) && !controller.ShouldHide(reinterpret_cast<uintptr_t>(details.primitive.data())) &&
                        controller.State().menu_visible, "custom placement and hiding preserve the NPC menu's native input ownership without affecting its tooltip")) return false;
                    magic.primitive[0x70] = 1; controller.Update(layout);
                    if (!check(!controller.State().menu_visible, "item descriptions alone do not claim native menu input")) return false;
                    controller.Update({});
                    if (!check(details.Coordinates() == std::array<int16_t, 2>{350, 400}, "clearing a tooltip's own placement restores its native position")) return false;
                    controller.Shutdown();
                }
            command = {}; magic = {}; details = {}; money = {};
            initialize(command, "menu    shop    ", {16, 48, 272, 238}, 0);
            initialize(magic, "menu    shopsell", {16, 256, 128, 296}, 2);
            initialize(details, "menu    iteminfo", {16, 240, 382, 296}, 0);
            initialize(money, "menu    aucmain ", {400, 50, 600, 400}, 0);
            link(manager, 0, command.node.data()); link(command.node, 0, magic.node.data()); link(magic.node, 0, details.node.data()); link(details.node, 0, money.node.data());
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            command.primitive[0x70] = magic.primitive[0x70] = details.primitive[0x70] = money.primitive[0x70] = 0;
            native_menu_layout independent{}; independent.viewport_width = 1920; independent.viewport_height = 1080;
            const auto merchant = native_hud_window_count + NativeMenuWindowIndex("shop    ");
            independent.windows[merchant] = {true, false, 300, 350};
            controller.Update(independent);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{300, 350} && magic.Coordinates() == std::array<int16_t, 2>{16, 256} &&
                details.Coordinates() == std::array<int16_t, 2>{16, 240} && money.Coordinates() == std::array<int16_t, 2>{400, 50},
                "moving a merchant item list leaves its menu, details and auction window independent")) return false;
            controller.Update(independent);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{300, 350}, "native custom positions do not accumulate offsets")) return false;
            independent.windows[merchant].hidden = true; controller.Update(independent);
            if (!check(controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())) &&
                !controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())) &&
                !controller.ShouldHide(reinterpret_cast<uintptr_t>(money.primitive.data())), "hiding a merchant item list leaves the other windows visible")) return false;
            link(command.owner, 8, magic.primitive.data());
            if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())), "replaced native owners are never hidden through stale snapshots")) return false;
            link(command.owner, 8, command.primitive.data());
            Position(command.primitive.data(), nullptr, 24, 60); controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
            controller.Update({});
            if (!check(command.Coordinates() == std::array<int16_t, 2>{24, 60} && magic.Coordinates() == std::array<int16_t, 2>{16, 256} &&
                details.Coordinates() == std::array<int16_t, 2>{16, 240}, "clearing native custom placement restores the latest game coordinates")) return false;
            controller.Update(independent); command.primitive[0x70] = 1; controller.Update(independent);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{24, 60}, "closing an attached custom-positioned native window restores its game coordinates")) return false;
            controller.Shutdown();
            initialize(command, "menu    gaugewin", {140, 580, 380, 640}, 0);
            command.primitive[0x70] = 1;
            const uintptr_t noMenus{}; std::memcpy(manager.data(), &noMenus, 4);
            uintptr_t playerOwner = reinterpret_cast<uintptr_t>(command.owner.data());
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position); controller.state_.menu_available = true;
            controller.hud_slots_[0] = reinterpret_cast<uintptr_t>(&playerOwner);
            controller.Update({});
            const auto geometry = controller.State().windows[0];
            if (!check(geometry.live && !geometry.visible && geometry.x == 140 && geometry.y == 580 && geometry.width == 240 && geometry.height == 60,
                "closed HUD owners supply real geometry even outside the active menu list")) return false;
            native_menu_layout hudPlacement{}; hudPlacement.viewport_width = 1920; hudPlacement.viewport_height = 1080;
            hudPlacement.windows[0] = {true, false, 400, 600}; command.primitive[0x70] = 0;
            controller.Update(hudPlacement);
            if (!check(!controller.State().windows[0].visible, "an allocated native owner is not treated as a rendered window")) return false;
            controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data())); controller.Update(hudPlacement);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{400, 600} && controller.State().windows[0].visible,
                "HUD owner geometry and movement use the same native coordinate space")) return false;
            controller.Update({});
            if (!check(command.Coordinates() == std::array<int16_t, 2>{140, 580}, "HUD owner placement restores the actual native position")) return false;
            controller.Shutdown();

            initialize(command, "menu    menuwind", {1792, 48, 1904, 250}, 1);
            command.primitive[0x70] = 1;
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data());
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position); controller.state_.menu_available = true;
            uintptr_t menuOwner = reinterpret_cast<uintptr_t>(command.owner.data());
            const auto mainMenu = native_hud_window_count + static_cast<size_t>(native_dialog::main_menu);
            controller.dialog_slots_[static_cast<size_t>(native_dialog::main_menu)] = reinterpret_cast<uintptr_t>(&menuOwner);
            controller.SetDrawMethod(reinterpret_cast<uintptr_t>(&Draw)); controller.SetEditing(true);
            drawing_controller = &controller; draw_calls = 0;
            native_menu_layout mainPlacement{}; mainPlacement.viewport_width = 1920; mainPlacement.viewport_height = 1080;
            mainPlacement.windows[mainMenu] = {true, false, 1200, 100};
            controller.Update(mainPlacement); controller.BeforeDraw(0);
            if (!check(draw_calls == 1 && command.primitive[0x70] == 1 && command.Coordinates() == std::array<int16_t, 2>{1200, 100},
                "inactive main menus use native drawing and custom placement without opening input or recursing")) return false;
            controller.Update(mainPlacement);
            if (!check(controller.State().windows[mainMenu].visible, "client-rendered inactive menus receive editor outlines")) return false;
            controller.SetEditing(false); controller.BeforeDraw(0); controller.Update({});
            if (!check(draw_calls == 1 && !controller.State().windows[mainMenu].visible && command.Coordinates() == std::array<int16_t, 2>{1792, 48},
                "leaving native editing stops inactive rendering and restores native positions")) return false;
            controller.Shutdown(); drawing_controller = nullptr;

            initialize(command, "menu    buff    ", {156, 95, 396, 99}, 0);
            command.primitive[0x70] = 0;
            command.component = {}; command.component[0] = 1;
            const std::array<int16_t, 4> icon{158, 97, 24, 24}; const int16_t iconIndex = 1;
            std::memcpy(command.component.data() + 0x2A, icon.data(), sizeof(icon));
            std::memcpy(command.component.data() + 0x3A, &iconIndex, sizeof(iconIndex));
            const int one = 1, capacity = 36;
            std::memcpy(command.owner.data() + 0x40, &one, 4); std::memcpy(command.owner.data() + 0x48, &capacity, 4);
            std::memcpy(command.owner.data() + 0x54, &one, 4);
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.hud_slots_[static_cast<size_t>(native_hud_element::status)] = reinterpret_cast<uintptr_t>(&menuOwner);
            controller.Update({});
            const auto status = controller.State().windows[static_cast<size_t>(native_hud_element::status)];
            if (!check(status.live && status.x == 159 && status.y == 98 && status.width == 24 && status.height == 24,
                "status bounds follow rendered icon components instead of the four-pixel native frame")) return false;
            link(command.component_node, 0, magic.component_node.data()); magic.component = {}; magic.component[0] = 1;
            const std::array<int16_t, 4> secondIcon{184, 123, 24, 24}; const int16_t secondIndex = 2; const int two = 2;
            std::memcpy(magic.component.data() + 0x2A, secondIcon.data(), sizeof(secondIcon));
            std::memcpy(magic.component.data() + 0x3A, &secondIndex, sizeof(secondIndex));
            std::memcpy(command.owner.data() + 0x40, &two, 4);
            controller.Update({});
            if (!check(controller.State().windows[static_cast<size_t>(native_hud_element::status)].width == 50 &&
                controller.State().windows[static_cast<size_t>(native_hud_element::status)].height == 50,
                "status bounds encompass every populated icon row and column")) return false;
            controller.Shutdown();

            command = {}; magic = {}; details = {}; money = {}; storage = {};
            initialize(command, "menu    equip   ", {130, 48, 312, 238}, 0);
            initialize(magic, "menu    persona ", {16, 48, 128, 238}, 0);
            initialize(storage, "menu    inventor", {314, 48, 496, 238}, 0);
            initialize(details, "menu    iteminfo", {130, 240, 496, 296}, 2);
            initialize(money, "menu    money   ", {16, 240, 128, 296}, 0);
            link(manager, 0, command.node.data()); link(command.node, 0, magic.node.data()); link(magic.node, 0, storage.node.data());
            link(storage.node, 0, details.node.data()); link(details.node, 0, money.node.data());
            std::array<uint8_t, 0x20> equipmentTable{};
            link(equipmentTable, 0x10, &Draw); link(command.owner, 0, equipmentTable.data());
            for (size_t index = 0; index < 10; ++index)
            {
                const std::array<int16_t, 2> offset{static_cast<int16_t>(16 + index * 3), static_cast<int16_t>(162 + index % 2 * 13)};
                std::memcpy(command.owner.data() + 0x88 + index * 0x20 + 8, offset.data(), sizeof(offset));
            }
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position); controller.equipment_draw_ = reinterpret_cast<uintptr_t>(&Draw);
            const auto equipment = native_hud_window_count + static_cast<size_t>(native_dialog::equipment);
            native_menu_layout equipmentLayout{}; equipmentLayout.viewport_width = 1920; equipmentLayout.viewport_height = 1080;
            equipmentLayout.windows[equipment] = {true, false, 300, 350};
            link(manager, 0, static_cast<void*>(nullptr)); controller.Update(equipmentLayout);
            link(manager, 0, command.node.data()); controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
            std::array<int16_t, 2> numberPosition{}, defensePosition{};
            std::memcpy(numberPosition.data(), command.owner.data() + 0x8C, sizeof(numberPosition));
            std::memcpy(defensePosition.data(), command.owner.data() + 0x1AC, sizeof(defensePosition));
            if (!check(command.Coordinates() == std::array<int16_t, 2>{300, 350} && magic.Coordinates() == std::array<int16_t, 2>{16, 48} &&
                storage.Coordinates() == std::array<int16_t, 2>{314, 48} && details.Coordinates() == std::array<int16_t, 2>{130, 240} &&
                money.Coordinates() == std::array<int16_t, 2>{16, 240} && numberPosition == std::array<int16_t, 2>{316, 512} &&
                defensePosition == std::array<int16_t, 2>{343, 525} &&
                controller.State().windows[equipment].width == 182 && controller.State().windows[equipment].height == 190,
                "equipment carries its stat values while status, storage, details and gil remain independent")) return false;
            equipmentLayout.windows[equipment].hidden = true; controller.Update(equipmentLayout);
            if (!check(controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())), "equipment artwork hides independently")) return false;
            for (auto* panel : {&magic, &storage, &details, &money})
                if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(panel->primitive.data())), "hiding equipment leaves adjacent native windows visible")) return false;
            controller.Update({});
            std::memcpy(numberPosition.data(), command.owner.data() + 0x8C, sizeof(numberPosition));
            if (!check(command.Coordinates() == std::array<int16_t, 2>{130, 48} && numberPosition == std::array<int16_t, 2>{146, 210},
                "restoring equipment also restores its native numeric text coordinates")) return false;
            controller.Shutdown();
            for (const auto& [resource, expected] : std::array<std::pair<const char*, native_dialog>, 12>{{
                {"persona ", native_dialog::status}, {"inventor", native_dialog::inventory}, {"mgcmenu ", native_dialog::actions},
                {"joblevel", native_dialog::status}, {"prty5us ", native_dialog::party_menu}, {"scresult", native_dialog::search},
                {"scanlist", native_dialog::map}, {"link12  ", native_dialog::linkshell}, {"conq3pts", native_dialog::region},
                {"quest01 ", native_dialog::quests}, {"conf7   ", native_dialog::native_config}, {"query   ", native_dialog::other}}})
            {
                std::array<char, 16> name{}; std::memcpy(name.data(), "menu    ", 8); std::memcpy(name.data() + 8, resource, 8);
                if (!check(controller.WindowIndex(0, name) < native_window_count &&
                    native_menu_windows[controller.WindowIndex(0, name) - native_hud_window_count].category == expected,
                    "Main Menu windows retain their navigation categories without sharing placement")) return false;
            }
            controller.Shutdown();
            command = {}; magic = {}; details = {}; money = {}; storage = {};
            initialize(command, "menu    statcom2", {1792, 48, 1904, 266}, 1);
            initialize(magic, "menu    persona ", {16, 48, 128, 238}, 0);
            initialize(details, "menu    level   ", {16, 208, 128, 296}, 0);
            initialize(money, "menu    profile ", {130, 48, 382, 226}, 0);
            initialize(storage, "menu    btlskill", {130, 48, 382, 196}, 0);
            link(manager, 0, command.node.data()); link(command.node, 0, magic.node.data()); link(magic.node, 0, details.node.data());
            link(details.node, 0, money.node.data()); link(money.node, 0, storage.node.data());
            std::array<uint8_t, 0x20> cachedTable{}, skillsTable{};
            link(cachedTable, 4, &Draw); link(cachedTable, 0x10, &Position);
            link(details.owner, 0, cachedTable.data()); link(money.owner, 0, cachedTable.data());
            std::array<uint8_t, 32> cacheCode{};
            const std::array<uint8_t, 8> prefix{0x8D, 0x41, 0x18, 0x8B, 0x49, 0x08, 0x50, 0xE8};
            std::memcpy(cacheCode.data(), prefix.data(), prefix.size()); cacheCode[12] = 0xC3;
            const auto relative = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&Frame) - reinterpret_cast<uintptr_t>(cacheCode.data()) - 12);
            std::memcpy(cacheCode.data() + 8, &relative, 4); link(skillsTable, 4, cacheCode.data()); link(skillsTable, 0x10, &Position); link(storage.owner, 0, skillsTable.data());
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position); controller.frame_read_ = reinterpret_cast<uintptr_t>(&Frame);
            controller.cached_frame_init_[0] = reinterpret_cast<uintptr_t>(&Draw);
            controller.module_ = reinterpret_cast<uintptr_t>(cacheCode.data()); controller.module_size_ = cacheCode.size();
            const auto statusMenu = native_hud_window_count + NativeMenuWindowIndex("statcom2");
            const auto character = native_hud_window_count + NativeMenuWindowIndex("persona ");
            const auto profile = native_hud_window_count + NativeMenuWindowIndex("profile ");
            const auto skills = native_hud_window_count + NativeMenuWindowIndex("btlskill");
            native_menu_layout statusLayout{}; statusLayout.viewport_width = 1920; statusLayout.viewport_height = 1080;
            statusLayout.windows[statusMenu] = {true, false, 1300, 400}; statusLayout.windows[character] = {true, false, 300, 350};
            statusLayout.windows[profile] = {true, false, 600, 100}; statusLayout.windows[skills] = {true, false, 1000, 200};
            details.owner[0x1C] = 0x7F;
            link(manager, 0, static_cast<void*>(nullptr)); controller.Update(statusLayout);
            link(manager, 0, command.node.data()); controller.BeforeDraw(reinterpret_cast<uintptr_t>(magic.primitive.data()));
            std::array<int16_t, 4> cachedExp{}, cachedProfile{}, cachedSkills{};
            std::memcpy(cachedExp.data(), details.owner.data() + 0x14, 8); std::memcpy(cachedProfile.data(), money.owner.data() + 0x14, 8);
            std::memcpy(cachedSkills.data(), storage.owner.data() + 0x18, 8);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{1300, 400} && magic.Coordinates() == std::array<int16_t, 2>{300, 350} &&
                details.Coordinates() == std::array<int16_t, 2>{300, 510} && money.Coordinates() == std::array<int16_t, 2>{600, 100} &&
                storage.Coordinates() == std::array<int16_t, 2>{1000, 200} && cachedExp == std::array<int16_t, 4>{300, 510, 412, 598} &&
                cachedProfile == std::array<int16_t, 4>{600, 100, 852, 278} && cachedSkills == std::array<int16_t, 4>{1000, 200, 1252, 348} && details.owner[0x1C] == 0x7F &&
                controller.State().windows[character].width == 112 && controller.State().windows[character].height == 248 &&
                controller.State().windows[statusMenu].width == 112,
                "Status selector and every result move independently; EXP, profile and skill contents use their native cached rectangles")) return false;
            statusLayout.windows[statusMenu].hidden = true; controller.Update(statusLayout);
            if (!check(controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())) &&
                !controller.ShouldHide(reinterpret_cast<uintptr_t>(magic.primitive.data())) && !controller.ShouldHide(reinterpret_cast<uintptr_t>(details.primitive.data())),
                "hiding the Status selector leaves the result and EXP visible")) return false;
            controller.Update({}); std::memcpy(cachedExp.data(), details.owner.data() + 0x14, 8);
            if (!check(cachedExp == std::array<int16_t, 4>{16, 208, 128, 296}, "restoring status also restores the EXP contents")) return false;
            controller.Shutdown();
            std::array<char, 16> currencyName{}; std::memcpy(currencyName.data(), "menu    evitem  ", 16);
            link(command.owner, 0, cachedTable.data()); link(cachedTable, 0x10, &Draw); controller.currency_draw_ = reinterpret_cast<uintptr_t>(&Draw);
            std::array<size_t, 3> currencyPages{};
            for (uint8_t page = 0; page < 3; ++page)
            {
                command.owner[0x88] = page; currencyPages[page] = controller.WindowIndex(reinterpret_cast<uintptr_t>(command.owner.data()), currencyName);
            }
            if (!check(currencyPages[0] != currencyPages[1] && currencyPages[1] != currencyPages[2] && currencyPages[0] != currencyPages[2],
                "Key Items, Currencies and Currencies 2 retain independent identities despite sharing a native DAT")) return false;
            command = {}; initialize(command, "menu    evitem  ", {16, 48, 382, 296}, 0); link(command.owner, 0, cachedTable.data());
            command.owner[0x88] = 1; link(manager, 0, command.node.data());
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            native_menu_layout currencyLayout{}; currencyLayout.viewport_width = 1920; currencyLayout.viewport_height = 1080;
            currencyLayout.windows[currencyPages[1]] = {true, true, 500, 100}; controller.Update(currencyLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{500, 100} && controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())),
                "Currencies keeps its own placement and hide")) return false;
            command.owner[0x88] = 2;
            if (!check(!controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())), "a reused currency window cannot inherit the previous page's hide")) return false;
            controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 48}, "a reused currency page restores native coordinates before its first draw")) return false;
            currencyLayout.windows[currencyPages[2]] = {true, false, 700, 300}; controller.Update(currencyLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{700, 300}, "Currencies 2 applies its independent saved position")) return false;
            command.owner[0x88] = 1; controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
            if (!check(command.Coordinates() == std::array<int16_t, 2>{500, 100} && controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())),
                "returning to a reused page applies its position and visibility on the same draw")) return false;
            controller.Shutdown();

            command = {}; magic = {}; details = {}; money = {}; storage = {};
            initialize(storage, "menu    logwindo", {16, 898, 804, 1064}, 2);
            const std::array<int16_t, 2> logPosition{16, 930}; std::memcpy(storage.primitive.data() + 0x52, logPosition.data(), 4);
            initialize(command, "menu    shopbuy ", {16, 888, 128, 928}, 2);
            initialize(magic, "menu    shopmain", {16, 856, 128, 896}, 0);
            initialize(details, "menu    shop    ", {16, 48, 290, 238}, 0);
            initialize(money, "menu    loot    ", {1500, 800, 1800, 900}, 3);
            link(manager, 0, storage.node.data()); link(storage.node, 0, command.node.data()); link(command.node, 0, magic.node.data());
            link(magic.node, 0, details.node.data()); link(details.node, 0, money.node.data());
            uintptr_t logOwner = reinterpret_cast<uintptr_t>(storage.owner.data());
            controller.hud_slots_[static_cast<size_t>(native_hud_element::chat1)] = reinterpret_cast<uintptr_t>(&logOwner);
            controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
            controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
            const auto buy = native_hud_window_count + NativeMenuWindowIndex("shopbuy ");
            native_menu_layout chatLayout{}; chatLayout.viewport_width = 2560; chatLayout.viewport_height = 1440;
            chatLayout.chat_windows = {native_screen_rect{8, 1223, 695, 207}, native_screen_rect{8, 1008, 745, 201}};
            controller.Update(chatLayout); controller.Update(chatLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 714} && magic.Coordinates() == std::array<int16_t, 2>{16, 682} &&
                details.Coordinates() == std::array<int16_t, 2>{16, 48} && money.Coordinates() == std::array<int16_t, 2>{1500, 800} &&
                storage.Coordinates() == logPosition && controller.State().windows[buy].y == 714,
                "windows stacked on the native log line move together above stacked XIUI+ chat; other windows and the log stay native")) return false;
            chatLayout.windows[buy] = {true, false, 400, 300}; controller.Update(chatLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{300, 225} && magic.Coordinates() == std::array<int16_t, 2>{16, 682},
                "a custom position overrides keeping menus above chat")) return false;
            chatLayout.windows[buy] = {}; chatLayout.chat_windows[1] = {}; controller.Update(chatLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 875} && magic.Coordinates() == std::array<int16_t, 2>{16, 843},
                "a single chat window raises the stack only by its overlap with the native log line")) return false;
            chatLayout.chat_windows[0] = {8, 100, 695, 207}; controller.Update(chatLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 888}, "chat in the upper half leaves bottom menus native")) return false;
            chatLayout.chat_windows = {}; controller.Update(chatLayout);
            if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 888} && magic.Coordinates() == std::array<int16_t, 2>{16, 856},
                "turning the option off restores native positions")) return false;
            controller.Shutdown();

            for (size_t index = 0; index < std::size(native_menu_windows); ++index)
            {
                command = {}; magic = {}; link(manager, 0, static_cast<void*>(nullptr));
                controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
                controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
                native_menu_layout placement{}; placement.viewport_width = 1920; placement.viewport_height = 1080;
                const auto window = native_hud_window_count + index;
                const bool hidden = (index % 2) != 0;
                placement.windows[window] = {true, hidden, 500, 300};
                controller.Update(placement);
                if ((index % 2) == 0) controller.BeforeDraw(0);
                const auto name = std::string{"menu    "} + native_menu_windows[index].resource;
                initialize(command, name.c_str(), {16, 48, 128, 216}, 0);
                if (native_menu_windows[index].variant != 0)
                {
                    link(command.owner, 0, cachedTable.data()); controller.currency_draw_ = reinterpret_cast<uintptr_t>(&Draw);
                    command.owner[0x88] = native_menu_windows[index].variant;
                }
                if ((index % 3) != 0) link(manager, 0, command.node.data());
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
                const auto message = std::string{"first draw applies the saved placement and visibility for "} + native_menu_windows[index].key;
                if (!check(command.Coordinates() == std::array<int16_t, 2>{500, 300} &&
                    controller.ShouldHide(reinterpret_cast<uintptr_t>(command.primitive.data())) == hidden, message.c_str())) return false;
                link(manager, 0, command.node.data());
                Position(command.primitive.data(), nullptr, 24, 60);
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
                if (!check(command.Coordinates() == std::array<int16_t, 2>{500, 300}, "native coordinate resets are reapplied before drawing without an intermediate offset")) return false;
                controller.Update({});
                if (!check(command.Coordinates() == std::array<int16_t, 2>{24, 60}, "reset restores the most recent native baseline")) return false;
                controller.Shutdown();
            }

            for (size_t index = 0; index < native_hud_window_count; ++index)
            {
                if (index == static_cast<size_t>(native_hud_element::compass) || index == static_cast<size_t>(native_hud_element::clock)) continue;
                command = {}; link(manager, 0, static_cast<void*>(nullptr));
                controller.manager_ = reinterpret_cast<uintptr_t>(manager.data()); controller.state_.menu_available = true;
                controller.reposition_ = reinterpret_cast<uintptr_t>(&Position);
                uintptr_t owner{}; controller.hud_slots_[index] = reinterpret_cast<uintptr_t>(&owner);
                native_menu_layout placement{}; placement.viewport_width = 1920; placement.viewport_height = 1080;
                placement.windows[index] = {true, false, 500, 300}; controller.Update(placement);
                const auto name = std::string{"menu    "} + (native_hud_options[index].resource ? native_hud_options[index].resource : "partywin");
                initialize(command, name.c_str(), {16, 48, 128, 216}, 0); owner = reinterpret_cast<uintptr_t>(command.owner.data());
                if (index == static_cast<size_t>(native_hud_element::status))
                {
                    controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
                    if (!check(command.Coordinates() == std::array<int16_t, 2>{16, 48}, "unavailable icon geometry never produces an invalid position")) return false;
                    const int count = 1, capacity = 32, first = 1;
                    std::memcpy(command.owner.data() + 0x40, &count, 4); std::memcpy(command.owner.data() + 0x48, &capacity, 4);
                    std::memcpy(command.owner.data() + 0x54, &first, 4); command.component[0] = 1;
                    const std::array<int16_t, 4> icon{15, 47, 24, 24};
                    std::memcpy(command.component.data() + 0x2A, icon.data(), sizeof(icon));
                    const int16_t componentIndex = 1; std::memcpy(command.component.data() + 0x3A, &componentIndex, 2);
                }
                controller.BeforeDraw(reinterpret_cast<uintptr_t>(command.primitive.data()));
                const auto message = std::string{"a newly allocated HUD owner uses its placement before drawing: "} + native_hud_options[index].key;
                if (!check(command.Coordinates() == std::array<int16_t, 2>{500, 300}, message.c_str())) return false;
                controller.Shutdown();
            }
            return true;
        }

        static auto Run(void) -> bool
        {
            native_menu_controller controller{};
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
        if (!xiui::native_menu_test::Run() || !xiui::native_menu_test::Windows()) return 1;
        std::puts("Native per-window placement, first draws, content, menu ownership and restoration passed.");
        return 0;
    }
    if (!xiui::native_hud_target_test::Run() || !xiui::native_hud_target_test::Macros() || !xiui::native_hud_target_test::MacroDelay() || !xiui::native_menu_test::Run() || !xiui::native_menu_test::Windows()) return 1;
    std::puts("Native target preservation and chat log/input suppression checks passed.");
    return 0;
}
