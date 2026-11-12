#include "modules/satchel/satchel_source.hpp"
#include "modules/satchel/satchel_cache.hpp"
#include "modules/satchel/satchel_identity.hpp"
#include "modules/satchel/storage_slips.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <stdexcept>

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
    struct satchel_data_test
    {
        static auto Run(void) -> void
        {
            std::array<satchel_server_endpoint, 2> endpoints{{{99, 0x33DEFEE1, 54230, true}, {99, 0x33DEFEE1, 54231, true}}};
            const auto serverIdentity = SatchelServerIdentity(endpoints, 99, 123);
            Check(serverIdentity == "server-33DEFEE1-54231-123" && SatchelStableIdentity(*serverIdentity, 123),
                "established same-process login data/auth endpoints form a server-scoped character identity");
            Check(!SatchelServerIdentity(std::span<const satchel_server_endpoint>{endpoints}.first(1), 99, 123) &&
                !SatchelServerIdentity(endpoints, 100, 123), "unpaired and different-process endpoints cannot scope the active character");
            endpoints[1].address = 0x01020304;
            Check(!SatchelServerIdentity(endpoints, 99, 123), "login endpoints must agree on the remote server address");
            endpoints[1].address = 0x33DEFEE1; endpoints[0].established = false;
            Check(!SatchelServerIdentity(endpoints, 99, 123), "disconnected endpoints cannot establish server identity");
            endpoints[0].established = true;
            const std::array<satchel_server_endpoint, 4> ambiguousEndpoints{{endpoints[0], endpoints[1], {99, 0x01020304, 54230, true}, {99, 0x01020304, 54231, true}}};
            Check(!SatchelServerIdentity(ambiguousEndpoints, 99, 123), "multiple active login server pairs remain ambiguous");
            Check(SatchelStableIdentity("world-10-123", 123) && !SatchelStableIdentity("local-123", 123) &&
                !SatchelStableIdentity("server-00000000-54231-123", 123) && !SatchelStableIdentity("server-33DEFEE1-54230-123", 123) &&
                !SatchelStableIdentity("server-33DEFEE1-54231-123", 456) && !SatchelStableIdentity("server-33DEFEE1-54231-4294967296"),
                "cache identity validation bounds server addresses, auth port and character suffix");

            inventory native;
            native.capacity[0] = 30; native.used[0] = 1;
            auto& item = native.items[0][2]; item.Id = 29312; item.Index = 2; item.Count = 7;
            item.Flags = 0x10002; item.Price = 250; item.Extra[0] = 0x81; item.Extra[23] = 0x80; item.Extra[27] = 0xA5;
            native.equipment[4].Index = 2;
            const auto bag = satchel_source::ReadBag(native, 0, true, true);
            Check(bag.ready && bag.capacity == 30 && bag.items.size() == 1 && bag.items[0].slot == 2 && bag.items[0].count == 7 &&
                bag.items[0].flags == 0x10002 && bag.items[0].price == 250 && bag.items[0].extra[27] == 0xA5 && bag.items[0].equipped,
                "source preserves native capacity, slot, full flags, bazaar price, equipment and all extra bytes");
            native.flags = 0;
            Check(!satchel_source::ReadBag(native, 0, true, true).ready, "loading flags gate source readiness");
            native.flags = 1; native.used[0] = 2;
            Check(!satchel_source::ReadBag(native, 0, true, true).ready, "incoherent item counts do not become a complete snapshot");
            Check(!SatchelBagAccessible(1, false) && !SatchelBagAccessible(2, false) && !SatchelBagAccessible(4, false) &&
                !SatchelBagAccessible(9, false) && SatchelBagAccessible(5, false) && SatchelBagAccessible(8, false) && SatchelBagAccessible(1, true),
                "Mog storage actions require an observed Mog context");

            satchel_snapshot live;
            live.identity = "world-10-123"; live.character_id = 123; live.name = "Test"; live.gil = 999; live.ready = true;
            live.bags[0] = bag;
            satchel_source source; source.Publish(live);
            const auto initialGeneration = source.Snapshot().bags[0].items[0].generation;
            const auto initialSnapshot = source.Snapshot().generation;
            source.Publish(live);
            Check(source.Snapshot().generation == initialSnapshot && source.Snapshot().bags[0].items[0].generation == initialGeneration,
                "unchanged scans retain item and snapshot generations");
            live.bags[0].items[0].count = 8; source.Publish(live);
            Check(source.Snapshot().bags[0].items[0].generation > initialGeneration, "changed quantities invalidate the item generation");
            source.Reset(); source.Publish(live);
            Check(source.Snapshot().bags[0].items[0].generation > initialGeneration, "reset does not recycle item generations");
            std::array<uint8_t, 12> packet{}; source.HandlePacket(0x1D, packet);
            Check(!source.Snapshot().ready && source.syncing_, "native update start immediately gates actions and cache writes");
            packet[4] = 1; source.HandlePacket(0x1D, std::span<const uint8_t>{packet}.first(5));
            Check(source.syncing_, "truncated completion cannot end inventory synchronization");
            source.HandlePacket(0x1D, packet);
            Check(!source.syncing_, "complete native inventory signal permits the next scan");
            std::array<uint8_t, 0x81> zone{}; zone[0x80] = 1; source.HandlePacket(0xA, zone);
            Check(source.in_mog_house_ && !source.Snapshot().ready, "zone packet establishes Mog context while invalidating old items");

            std::array<uint8_t, 24> extra{};
            for (const auto id : SatchelSlipIds)
            {
                const auto catalog = SatchelSlipCatalog(id);
                Check(!catalog.empty() && catalog.size() <= 192, "all 28 storage slips have bounded catalogs");
                extra.fill(0); extra[0] = 1;
                Check(SatchelDecodeSlip(id, extra) == std::vector<uint16_t>{catalog.front()}, "slip bits map little-endian from the first item");
                extra.fill(0); const auto last = catalog.size() - 1; extra[last / 8] = static_cast<uint8_t>(1u << (last % 8));
                Check(SatchelDecodeSlip(id, extra) == std::vector<uint16_t>{catalog.back()}, "last catalog bit stays in bounds");
            }
            Check(SatchelDecodeSlip(29312, std::span<const uint8_t>{extra}.first(23)).empty() && SatchelSlipCatalog(1).empty(),
                "unknown and truncated slips do not produce stored items");

            std::vector<satchel_item> ordered(4);
            for (uint8_t index = 0; index < 4; ++index) { ordered[index].id = static_cast<uint16_t>(10 * (index + 1)); ordered[index].slot = index + 1; }
            const auto entry = [](const uint16_t id, const uint8_t slot) { return satchel_native_entry{id, slot}; };
            const std::array<satchel_native_entry, 4> nativeOrder{entry(30, 3), entry(99, 1), entry(10, 1), entry(20, 2)};
            SatchelApplyNativeOrder(ordered, nativeOrder);
            Check(ordered[0].slot == 3 && ordered[1].slot == 1 && ordered[2].slot == 2 && ordered[3].slot == 4,
                "native list order applies by item and slot; stale entries are ignored and unlisted items follow in slot order");

            satchel_item listedLater{}; listedLater.id = 4096; listedLater.slot = 1; listedLater.count = 12;
            live.bags[0].items.push_back(listedLater);
            const auto path = std::filesystem::temp_directory_path() / ("xiui-satchel-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            satchel_cache cache; cache.Initialize(path.string());
            satchel_cache peer; peer.Initialize(path.string());
            cache.Observe(live, 100); cache.Observe(live, 400);
            Check(cache.Characters().size() == 1 && cache.Characters()[0].read_only && cache.Characters()[0].bags[0].items[0].read_only &&
                !cache.Characters()[0].bags[0].accessible, "settled complete observations become read-only offline records");
            const auto observed = cache.Characters()[0].observed;
            Check(observed > 1700000000ULL, "offline observation timestamps use wall-clock epoch seconds");
            cache.characters_[0].observed -= 31;
            cache.Observe(live, 410); cache.Observe(live, 710);
            Check(cache.Characters()[0].observed >= observed, "unchanged complete inventories refresh stale observation dates after settling");
            auto partial = live; partial.gil = 0; partial.bags[0].ready = false;
            cache.Observe(partial, 500); cache.Observe(partial, 1000);
            Check(cache.Characters()[0].gil == 999, "partial bags never overwrite a complete cache");
            auto ambiguous = live; ambiguous.identity = "local-123";
            cache.Observe(ambiguous, 1100); cache.Observe(ambiguous, 1500);
            Check(cache.Characters().size() == 1, "world-ambiguous fallback identities cannot collide in the offline cache");
            auto alt = live; alt.identity = "world-10-456"; alt.character_id = 456; alt.name = "Second"; alt.gil = 222;
            peer.Observe(alt, 100); peer.Observe(alt, 400);
            cache.WriteOrder(live.identity, -1, {"0:2:29312", "0:1:10"}); cache.Stop();
            peer.WriteOrder(alt.identity, -1, {"0:1:10", "0:2:29312"}); peer.Stop();
            satchel_cache restored; restored.Initialize(path.string());
            Check(restored.Characters().size() == 2 && restored.Characters()[0].bags[0].items[0].extra[27] == 0xA5 &&
                restored.Characters()[0].bags[0].items[0].count == 8 && restored.Characters()[0].bags[0].items[0].flags == 0x10002 &&
                restored.ReadOrder(live.identity, -1) == std::vector<std::string>{"0:2:29312", "0:1:10"} &&
                restored.ReadOrder(alt.identity, -1) == std::vector<std::string>{"0:1:10", "0:2:29312"},
                "atomic cache round trip retains stacks, full extra data, flags and local visual order");
            Check(restored.Characters()[0].bags[0].items.size() == 2 && restored.Characters()[0].bags[0].items[0].slot == 2 &&
                restored.Characters()[0].bags[0].items[1].slot == 1, "offline records keep the native item list order");
            Check(restored.Characters()[1].identity == alt.identity && restored.Characters()[1].gil == 222,
                "clients initialized together cannot erase another character's inventory or layout");
            live.gil = 1000; alt.gil = 333;
            restored.Observe(live, 2000); restored.Observe(live, 2300);
            restored.Observe(alt, 2400); restored.Observe(alt, 2700);
            Check(restored.writers_.size() == 2, "each identity has an independent bounded writer");
            restored.Stop();
            satchel_cache drained; drained.Initialize(path.string());
            Check(drained.Characters().size() == 2 && drained.Characters()[0].gil == 1000 && drained.Characters()[1].gil == 333,
                "draining writers preserves queued updates for different characters");
            auto firstServer = live; firstServer.identity = *serverIdentity;
            auto secondServer = live; secondServer.identity = "server-01020304-54231-123"; secondServer.gil = 555;
            drained.Observe(firstServer, 3000); drained.Observe(firstServer, 3300);
            drained.Observe(secondServer, 3400); drained.Observe(secondServer, 3700);
            drained.Stop();
            satchel_cache scoped; scoped.Initialize(path.string());
            const auto first = std::find_if(scoped.Characters().begin(), scoped.Characters().end(), [&](const auto& current) { return current.identity == firstServer.identity; });
            const auto second = std::find_if(scoped.Characters().begin(), scoped.Characters().end(), [&](const auto& current) { return current.identity == secondServer.identity; });
            Check(scoped.Characters().size() == 4 && first != scoped.Characters().end() && second != scoped.Characters().end() && first->gil == 1000 && second->gil == 555,
                "the same character id on different servers retains independent cached inventories");
            scoped.Stop();
            std::filesystem::remove(path / (live.identity + ".ini"));
            std::filesystem::remove(path / (alt.identity + ".ini"));
            std::filesystem::remove(path / (firstServer.identity + ".ini"));
            std::filesystem::remove(path / (secondServer.identity + ".ini"));
            std::filesystem::remove(path);
        }
    };
}

int main(void)
{
    try { xiui::satchel_data_test::Run(); std::puts("Satchel source, offline cache and 28 storage-slip checks passed."); return 0; }
    catch (const std::exception& error) { std::fprintf(stderr, "Satchel data: %s\n", error.what()); return 1; }
}
