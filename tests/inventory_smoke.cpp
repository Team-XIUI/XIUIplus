#include "hud_runtime.hpp"
#include "resources/item_icon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <stdexcept>
#include <fstream>

namespace
{
    auto Check(const bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }

    class inventory final : public IInventory
    {
    public:
        std::array<uint32_t, 17> used{}, capacity{};
        mutable std::array<Ashita::FFXI::equipmententry_t, 16> equipment{};
        mutable std::array<std::array<Ashita::FFXI::item_t, 81>, 17> items{};
        uint32_t flags{0x1FFFF};
        Ashita::FFXI::inventory_t* GetRawStructure() const override { return nullptr; }
        Ashita::FFXI::item_t* GetContainerItem(uint32_t bag, uint32_t index) const override { return bag < this->items.size() && index < this->items[bag].size() ? &this->items[bag][index] : nullptr; }
        uint32_t GetContainerCount(uint32_t bag) const override { return this->used[bag]; }
        uint32_t GetContainerCountMax(uint32_t bag) const override { return this->capacity[bag]; }
        Ashita::FFXI::treasureitem_t* GetTreasurePoolItem(uint32_t) const override { return nullptr; }
        uint32_t GetTreasurePoolStatus() const override { return 0; }
        uint32_t GetTreasurePoolItemCount() const override { return 0; }
        uint32_t GetContainerUpdateCounter() const override { return 0; }
        uint32_t GetContainerUpdateFlags() const override { return this->flags; }
        uint32_t GetDisplayItemSlot() const override { return 0; }
        uint32_t GetDisplayItemPointer() const override { return 0; }
        Ashita::FFXI::equipmententry_t* GetEquippedItem(uint32_t slot) const override { return slot < this->equipment.size() ? &this->equipment[slot] : nullptr; }
        Ashita::FFXI::itemcheck_t* GetCheckEquippedItem(uint32_t) const override { return nullptr; }
        uint32_t GetCheckTargetIndex() const override { return 0; }
        uint32_t GetCheckServerId() const override { return 0; }
        uint32_t GetCheckFlags() const override { return 0; }
        uint8_t GetCheckMainJob() const override { return 0; }
        uint8_t GetCheckSubJob() const override { return 0; }
        uint8_t GetCheckMainJobLevel() const override { return 0; }
        uint8_t GetCheckSubJobLevel() const override { return 0; }
        uint8_t GetCheckMainJob2() const override { return 0; }
        uint8_t GetCheckMasteryLevel() const override { return 0; }
        uint8_t GetCheckMasteryFlags() const override { return 0; }
        uint8_t* GetCheckLinkshellName() const override { return nullptr; }
        uint16_t GetCheckLinkshellColor() const override { return 0; }
        uint8_t GetCheckLinkshellIconSetId() const override { return 0; }
        uint8_t GetCheckLinkshellIconSetIndex() const override { return 0; }
        uint32_t GetCheckBallistaChevronCount() const override { return 0; }
        uint16_t GetCheckBallistaChevronFlags() const override { return 0; }
        uint16_t GetCheckBallistaFlags() const override { return 0; }
        const char* GetSearchComment() const override { return ""; }
        uint32_t GetCraftStatus() const override { return 0; }
        uintptr_t GetCraftCallback() const override { return 0; }
        uint32_t GetCraftTimestampAttempt() const override { return 0; }
        uint32_t GetCraftTimestampResponse() const override { return 0; }
        void SetCraftStatus(uint32_t) const override {}
        void SetCraftCallback(uintptr_t) const override {}
        void SetCraftTimestampAttempt(uint32_t) const override {}
        void SetCraftTimestampResponse(uint32_t) const override {}
        const char* GetSelectedItemName() const override { return ""; }
        uint16_t GetSelectedItemId() const override { return 0; }
        uint8_t GetSelectedItemIndex() const override { return 0; }
    };
}

namespace xiui
{
    struct inventory_test
    {
        static auto GilParty(const char* iconPath) -> void
        {
            inventory native;
            native.items[0][0].Id = 65535; native.items[0][0].Count = 1234567;
            Check(gil_source::ReadBalance(native) == 1234567u, "gil is the native slot-zero quantity");
            native.flags &= ~1u;
            Check(!gil_source::ReadBalance(native), "loading inventory cannot establish a gil baseline");
            native.flags |= 1; native.items[0][0].Count = 0;
            Check(gil_source::ReadBalance(native) == 0u, "a ready zero balance is valid");
            native.items[0][0].Id = 0;
            Check(!gil_source::ReadBalance(native), "uninitialized slot zero cannot masquerade as zero gil");
            gil_source source;
            source.session_.Observe(1, 1000, 100);
            source.session_.Observe(1, 1500, 3600100);
            Check(source.Session().Net() == 500 && source.Session().PerHour(3600100) == 500, "session net and hourly rate use elapsed time");
            std::array<uint8_t, 12> packet{};
            source.HandlePacket(0xB, packet);
            Check(!source.Session().ready && source.Session().Net() == 500 && source.syncing_, "zoning hides the tracker without resetting earnings");
            source.HandlePacket(0x1D, std::span<const uint8_t>{packet}.first(5));
            Check(source.syncing_, "truncated completion cannot permit loading gil");
            packet[4] = 1; source.HandlePacket(0x1D, packet);
            Check(!source.syncing_, "native inventory completion ends synchronization");
            source.session_.Observe(1, 0, 7200100);
            Check(source.Session().Net() == -1000 && source.Session().PerHour(7200100) == -500, "spending to zero retains signed session earnings");
            auto sessionStore = settings_store::FromIniText(""); source.SaveSession(sessionStore);
            gil_source restored; restored.LoadSession(sessionStore); restored.session_.Observe(1, 250, 7201100);
            Check(restored.Session().Net() == -750 && restored.Session().started == 100, "runtime replacement preserves this client session baseline");
            sessionStore.WriteInt("session.gil", "process", 0);
            gil_source anotherProcess; anotherProcess.LoadSession(sessionStore);
            Check(!anotherProcess.Session().initialized, "a new game process cannot restore old earnings");
            source.ResetSession(7200100);
            Check(source.Session().Net() == 0 && source.Session().started == 7200100, "manual reset accepts a zero balance");
            source.session_.Observe(2, 999999999, 7201100);
            Check(source.Session().Net() == 0 && source.Session().player == 2, "character changes establish a silent baseline");
            Check(FormatGil(1234567) == "1,234,567" && FormatGil(-2500, true) == "-2,500" && FormatGil(0, true) == "+0" &&
                FormatGil(INT64_MIN) == "-9,223,372,036,854,775,808", "gil formatting handles signs and full integer range");
            const auto theme = ui::theme::CreateDefault();
            gil_module gil; gil.SetSource(&source); gil.ApplyTextDefaults(theme);
            gil.right_align_ = true; gil.icon_right_ = false; gil.mode_ = 1; gil.text_[1].offset_x = -4;
            gil.text_[1].style.shadow.enabled = true; gil.positive_ = 0xAA88CCAA;
            auto store = settings_store::FromIniText(""); gil.Save(store);
            gil_module loaded; loaded.ApplyTextDefaults(theme); loaded.Load(store);
            Check(loaded.right_align_ && !loaded.icon_right_ && loaded.mode_ == 1 && loaded.text_[1].offset_x == -4 &&
                loaded.text_[1].style.shadow.enabled && loaded.positive_ == 0xAA88CCAA, "gil layout, ARGB tints and shared text effects persist");
            for (size_t index = 0; index < loaded.OptionCount(); ++index)
            {
                size_t owners{};
                for (size_t section = 0; section < loaded.ConfigSections().size(); ++section) owners += loaded.ConfigSectionContains(section, index);
                Check(owners == 1 && loaded.Option(index).label != nullptr, "each gil control has one config topic");
            }
            Check(gil.Option(gil_module::reset).kind == module_option_kind::action && gil.Option(gil_module::reset).available,
                "reset tracking is an action available for a ready balance");
            gil.OnOptionChanged(gil_module::reset);
            Check(source.Session().Net() == 0, "the config reset action resets actual tracking");
            party_module party, first{1}, second{2};
            party.bar_widths_[0] = 153; party.bar_offsets_x_[1] = -12; party.tp_display_ = 1;
            party.spacing_ = 0; party.text_[0].offset_y = 9; party.state_.bounds.width = 390;
            party.meters_[1].height = 7; party.status_placement_ = 1; party.selection_style_.background_opacity = 0.22f;
            first.state_.bounds.x = 88; first.state_.bounds.y = 99; first.state_.enabled = false; first.hide_preview_ = true;
            first.CopyPreferences(party); second.CopyPreferences(party);
            for (const auto* alliance : {&first, &second})
                Check(alliance->bar_widths_[0] == 153 && alliance->bar_offsets_x_[1] == -12 && alliance->tp_display_ == 1 &&
                    alliance->spacing_ == 0 && alliance->text_[0].offset_y == 9 && alliance->state_.bounds.width == 390 &&
                    alliance->meters_[1].height == 7 && alliance->status_placement_ == 1 && alliance->selection_style_.background_opacity == 0.22f,
                    "both alliances inherit current party resources, geometry, effects and text");
            Check(first.state_.bounds.x == 88 && first.state_.bounds.y == 99 && !first.state_.enabled && !first.PreviewVisible() &&
                second.PreviewVisible() && party.PreviewVisible() && std::string_view{first.Id()} == "alliance1", "copy preserves independent positions, enablement and preview visibility");
            first.Save(store); party_module savedFirst{1}; savedFirst.Load(store);
            Check(!savedFirst.PreviewVisible() && !savedFirst.State().enabled && savedFirst.bar_widths_[0] == 153, "alliance preview visibility and copied settings persist");
            hud_runtime runtime;
            runtime.gil_source_.session_.Observe(1, 300, ::GetTickCount64());
            runtime.gil_source_.session_.Observe(1, 350, ::GetTickCount64());
            runtime.active_module_ = &runtime.gil_; runtime.config_bounds_ = {100, 100, 1100, 600};
            runtime.mouse_x_ = static_cast<int>(runtime.ConfigLayout().OptionRow(gil_module::reset).x + 50);
            runtime.mouse_y_ = static_cast<int>(runtime.ConfigLayout().OptionRow(gil_module::reset).y + 10);
            Check(runtime.ProcessConfigClick({0, 0, 1600, 900, 0, 1}) && runtime.gil_source_.Session().Net() == 0,
                "the shared action row dispatches through config input");
            if (iconPath != nullptr)
            {
                std::ifstream file{iconPath, std::ios::binary};
                const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
                const auto icon = resources::DecodeItemIcon(bytes);
                Check(icon && icon->width == 32 && icon->height == 32 && std::any_of(icon->pixels.begin(), icon->pixels.end(), [](auto pixel) { return pixel != 0; }),
                    "AgentLink's native gil DAT bitmap decodes through the shared item icon path");
            }
        }

        static auto Run(void) -> void
        {
            inventory native;
            native.capacity[0] = 30; native.used[0] = 29;
            native.capacity[1] = native.capacity[9] = 80;
            native.used[1] = 50; native.used[9] = 70;
            native.capacity[8] = native.capacity[10] = 80;
            native.used[8] = 10; native.used[10] = 25;
            inventory_source source;
            source.state_ = {inventory_source::ReadBags(native), true, false};
            const auto main = InventoryTotal(source.State(), inventory_groups[0]);
            const auto safe = InventoryTotal(source.State(), inventory_groups[3]);
            const auto wardrobe = InventoryTotal(source.State(), inventory_groups[5]);
            Check(main.used == 29 && main.capacity == 30 && main.ready, "main inventory counts occupied slots against its real capacity");
            Check(safe.used == 120 && safe.capacity == 160 && safe.ready, "Safe includes both unlocked containers");
            Check(wardrobe.used == 35 && wardrobe.capacity == 160 && wardrobe.ready, "Wardrobes sum only unlocked containers");
            Check(!InventoryTotal(source.State(), inventory_groups[1]).ready, "locked storage stays hidden");
            native.flags &= ~(1u << 9);
            source.state_.bags = inventory_source::ReadBags(native);
            Check(!InventoryTotal(source.State(), inventory_groups[3]).ready && InventoryTotal(source.State(), inventory_groups[0]).ready,
                "partially loaded storage does not appear as empty and does not hide a ready inventory");
            native.used[0] = 31;
            Check(!inventory_source::ReadBags(native)[0].ready, "inconsistent native counts wait for a coherent snapshot");
            Check(inventory_module::Fullness({59, 80, true}, 0, 75, 95) == 0 && inventory_module::Fullness({60, 80, true}, 0, 75, 95) == 1 &&
                inventory_module::Fullness({76, 80, true}, 0, 75, 95) == 2, "percentage boundaries use occupied slots inclusively");
            Check(inventory_module::Fullness({15, 30, true}, 1, 15, 29) == 1 && inventory_module::Fullness({29, 30, true}, 1, 15, 29) == 2 &&
                inventory_module::Fullness({30, 30, true}, 1, 60, 75) == 2, "fixed-count thresholds retain XIUI behavior and full bags are always critical");
            Check(inventory_module::Fullness({0, 80, true}, 0, 0, 0) == 0 && inventory_module::Fullness({0, 0, false}, 0, 75, 95) == 0,
                "empty and unavailable storage never warns");
            std::set<std::pair<int, int>> positions;
            for (uint32_t slot = 0; slot < 640; ++slot)
            {
                const auto at = inventory_module::DotPosition(slot, 10, 8, 3, 7, 2, 8);
                Check(at.x >= 0 && at.x + 7 <= 280 && at.y >= 0, "wrapped dot groups stay within their planned width");
                positions.emplace(static_cast<int>(at.x), static_cast<int>(at.y));
            }
            Check(positions.size() == 640, "every slot has one distinct dot across wrapped groups");
            native.flags = 0x1FFFF; native.used[0] = 29;
            source.state_.bags = inventory_source::ReadBags(native);
            std::array<uint8_t, 12> packet{};
            source.HandlePacket(0x001D, packet);
            Check(source.State().available && source.syncing_ && InventoryTotal(source.State(), inventory_groups[0]).used == 29,
                "ordinary item updates retain the last ready snapshot instead of flashing an empty grid");
            packet[4] = 1; source.HandlePacket(0x001D, packet);

            inventory_module module{3};
            const auto theme = ui::theme::CreateDefault();
            module.ApplyTextDefaults(theme);
            module.SetSource(&source);
            module.state_.enabled = module.split_ = module.background_ = module.text_tint_ = true;
            module.state_.bounds = {55, 70, 240, 100};
            module.columns_ = 7; module.rows_ = 4; module.dot_size_ = 9; module.text_position_ = 1;
            module.warning_percent_ = 80; module.critical_percent_ = 97; module.empty_ = 0x40556677;
            module.text_[1].offset_x = -3;
            ui::design_system design;
            const module_render_context context{design, nullptr, false, nullptr, {0, 0, 1280, 720}};
            module.Render({}, context);
            const auto separateHeight = module.State().bounds.height;
            module.split_ = false; module.Render({}, context);
            Check(separateHeight > module.State().bounds.height && module.State().bounds.width < 1280,
                "separate containers reserve their own rows while combined dots fit the viewport");
            module.split_ = true;
            auto settings = settings_store::FromIniText("");
            module.Save(settings);
            inventory_module loaded{3}; loaded.ApplyTextDefaults(theme); loaded.SetSource(&source); loaded.Load(settings);
            Check(loaded.state_.enabled && loaded.split_ && loaded.background_ && loaded.text_tint_ && loaded.columns_ == 7 && loaded.rows_ == 4 &&
                loaded.dot_size_ == 9 && loaded.text_position_ == 1 && loaded.warning_percent_ == 80 && loaded.critical_percent_ == 97 &&
                loaded.empty_ == 0x40556677 && loaded.text_[1].offset_x == -3 && loaded.state_.bounds.x == 55,
                "layout, thresholds, alpha colors and shared text profiles persist independently");
            for (size_t index = 0; index < loaded.OptionCount(); ++index)
            {
                size_t owners{};
                for (size_t section = 0; section < loaded.ConfigSections().size(); ++section) owners += loaded.ConfigSectionContains(section, index);
                Check(owners == 1 && loaded.Option(index).label != nullptr, "every inventory option has one config topic");
            }
            const auto tinted = TintTextProfile(loaded.text_[1], loaded.warning_);
            Check(tinted.style.shadow.enabled == loaded.text_[1].style.shadow.enabled && tinted.style.stroke.enabled == loaded.text_[1].style.stroke.enabled,
                "fullness tint retains the configured HUD text effects");
            loaded.Load(settings_store::FromIniText("[inventory.safe]\ncolumns=0\nrows=nan\nwarning_percent=95\ncritical_percent=40\n"));
            Check(loaded.columns_ == 1 && loaded.rows_ == 6 && loaded.critical_percent_ == 95, "invalid geometry and crossed thresholds load safely");

            source.HandlePacket(0x000A, packet);
            Check(!source.State().available && source.syncing_, "zoning immediately invalidates the previous character's bag data");
            source.HandlePacket(0x001D, {packet.data(), 4});
            Check(source.syncing_, "a truncated inventory completion cannot release loading");
            packet[4] = 1; source.HandlePacket(0x001D, packet);
            Check(!source.syncing_ && source.next_scan_ == 0, "native inventory completion permits the next snapshot without a timeout");
            loaded.ResetDefaults(theme);
            Check(loaded.source_ == &source && loaded.state_.bounds.x == 55 && !loaded.state_.enabled && loaded.dots_, "reset preserves the source and saved position");

            hud_runtime runtime;
            const auto pages = runtime.ConfigPages();
            const auto found = std::find_if(pages.begin(), pages.end(), [](const auto& page) { return std::string_view{page.id} == "inventory"; });
            Check(found != pages.end() && found->Count() == 8 && found->SectionCount() == 3,
                "one Inventory config page owns all six storage trackers, Equipment and Gil");
            for (size_t i = 0; i < found->Count(); ++i)
            {
                runtime.active_module_ = found->Module(i);
                Check(found->Contains(runtime.active_module_) && runtime.HasConfigSections() && runtime.HasConfigSubmenu(), "all Inventory members use the shared config navigation");
            }
            Check(runtime.inventory_[0].State().enabled && !runtime.inventory_[5].State().enabled, "only main Inventory starts enabled");
            const auto extras = std::find_if(pages.begin(), pages.end(), [](const auto& page) { return std::string_view{page.id} == "extras"; });
            Check(extras != pages.end() && extras->Count() == 3 && extras->Module(0) == &runtime.compass_ && extras->Module(1) == &runtime.world_info_ && extras->Module(2) == &runtime.commands_,
                "Extras consolidates Compass, World Info and Commands without replacing their modules");
            for (size_t i = 0; i < extras->Count(); ++i)
            {
                runtime.active_module_ = extras->Module(i);
                Check(runtime.HasConfigSections() && runtime.ActiveConfigPage().Contains(runtime.active_module_), "each Extra retains its own shared config selector");
            }
            for (const auto& page : pages)
                Check(std::string_view{page.id} != "compass" && std::string_view{page.id} != "world" && std::string_view{page.id} != "commands", "Extras has no duplicate sidebar entries");
            Equipment(native, theme);
        }

        static auto Equipment(inventory& native, const ui::theme& theme) -> void
        {
            native.flags = 0x1FFFF;
            native.equipment[0].Index = 1;
            native.items[0][1].Id = 5000; native.items[0][1].Count = 1;
            native.equipment[5].Index = (8u << 8) | 2u;
            native.items[8][2].Id = 6000; native.items[8][2].Count = 1;
            native.equipment[3].Index = 3;
            native.items[0][3].Id = 7000; native.items[0][3].Count = 99;
            const auto main = equipment_source::ReadSlot(native, 0), body = equipment_source::ReadSlot(native, 5), ammo = equipment_source::ReadSlot(native, 3);
            Check(main.ready && main.id == 5000 && main.bag == 0 && main.index == 1, "equipment resolves inventory indexes to actual item IDs");
            Check(body.ready && body.id == 6000 && body.bag == 8 && body.index == 2 && ammo.count == 99, "equipment resolves Wardrobe items and ammunition stacks");
            Check(equipment_source::ReadSlot(native, 1).ready && equipment_source::ReadSlot(native, 1).id == 0, "unequipped slots stay in the grid");
            native.flags &= ~(1u << 8);
            Check(!equipment_source::ReadSlot(native, 5).ready, "loading Wardrobes cannot replace equipped items with empty icons");
            native.flags |= 1u << 8;
            native.equipment[5].Index = (17u << 8) | 2u;
            Check(!equipment_source::ReadSlot(native, 5).ready, "unsupported bags never reach the native item getter");
            native.equipment[5].Index = (8u << 8) | 81u;
            Check(!equipment_source::ReadSlot(native, 5).ready, "out-of-range equipment indexes are rejected");
            native.equipment[5].Index = (8u << 8) | 2u;
            native.items[8][2].Id = 0xFFFF;
            Check(!equipment_source::ReadSlot(native, 5).ready, "invalid item sentinels are not sent to the icon cache");
            Check(std::set<uint8_t>(equipment_grid_order.begin(), equipment_grid_order.end()).size() == 16 && equipment_grid_order[5] == 9 &&
                equipment_grid_order[8] == 5 && equipment_grid_order[15] == 8, "native equipment ordering includes every slot once");

            equipment_source source;
            source.available_ = true;
            source.items_[0] = main; source.items_[3] = ammo;
            equipment_module module;
            module.ApplyTextDefaults(theme); module.SetSource(&source);
            module.state_.bounds = {40, 60, 206, 226};
            module.icon_size_ = 40; module.slot_labels_ = true; module.text_[2].offset_x = -2;
            ui::design_system design;
            const module_render_context context{design, nullptr, false, nullptr, {0, 0, 1280, 720}};
            module.Render({}, context);
            const auto first = module.SlotBounds(0), right = module.SlotBounds(3), last = module.SlotBounds(15);
            Check(first.x < right.x && right.y == first.y && last.x == right.x && last.y > first.y &&
                last.y + last.height + module.LabelHeight() <= module.state_.bounds.y + module.state_.bounds.height,
                "all sixteen icon and label bounds fit inside the equipment container");
            const auto originalHeight = module.state_.bounds.height;
            module.layout_ = 1; module.Render({}, context);
            Check(module.state_.bounds.height < originalHeight && module.SlotBounds(7).y == module.SlotBounds(0).y && module.SlotBounds(8).y > module.SlotBounds(0).y,
                "alternate layouts retain all slots and update their hit bounds");
            auto settings = settings_store::FromIniText(""); module.Save(settings);
            equipment_module loaded; loaded.ApplyTextDefaults(theme); loaded.SetSource(&source); loaded.Load(settings);
            Check(loaded.icon_size_ == 40 && loaded.layout_ == 1 && loaded.slot_labels_ && loaded.text_[2].offset_x == -2 && loaded.state_.bounds.x == 40,
                "equipment layout, position and shared text profiles persist");
            for (size_t index = 0; index < loaded.OptionCount(); ++index)
            {
                size_t owners{};
                for (size_t section = 0; section < loaded.ConfigSections().size(); ++section) owners += loaded.ConfigSectionContains(section, index);
                Check(owners == 1 && loaded.Option(index).label != nullptr, "every equipment setting has one config topic");
            }
            Check(module.ShouldRender({}), "live equipment is visible by default");
            source.Reset();
            Check(!module.ShouldRender({}) && source.Items()[0].id == 0, "zoning removes stale equipment and hides the live module");
            loaded.ResetDefaults(theme);
            Check(loaded.source_ == &source && loaded.state_.bounds.x == 40 && loaded.icon_size_ == 32 && loaded.layout_ == 0,
                "equipment reset preserves the source and moved position");
        }
    };
}

int main(int argc, char** argv)
{
    try {
        if (argc > 1 && std::string_view{argv[1]} == "--gil-party-only") {
            xiui::inventory_test::GilParty(argc > 2 ? argv[2] : nullptr);
            std::puts("Gil tracking, native icon, config actions, alliance settings and preview checks passed.");
        } else {
            xiui::inventory_test::Run();
            std::puts("Inventory, equipment slots, settings and Extras navigation checks passed.");
        }
        return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Inventory dots: %s\n", error.what()); return 1; }
}
