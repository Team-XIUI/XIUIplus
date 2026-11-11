#include "modules/satchel/satchel_actions.hpp"
#include "Ashita.h"

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    {
        if (!condition) throw std::runtime_error{message};
    }

    class packets final : public IPacketManager
    {
    public:
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> outgoing{};
        void AddIncomingPacket(uint16_t, uint32_t, uint8_t*) override {}
        void AddOutgoingPacket(const uint16_t id, const uint32_t size, uint8_t* bytes) override
        {
            this->outgoing.emplace_back(id, std::vector<uint8_t>{bytes, bytes + size});
        }
        bool QueueOutgoingPacket(uint16_t, uint16_t, uint16_t, uint32_t, uint32_t, queuepacketcallback_f, void*) const override { return false; }
    };
}

namespace xiui
{
    struct satchel_actions_test
    {
        static auto Snapshot(void) -> satchel_snapshot
        {
            satchel_snapshot snapshot{};
            snapshot.identity = "world-1-123"; snapshot.character_id = 123; snapshot.zone = 100; snapshot.ready = true;
            for (auto& bag : snapshot.bags) { bag.ready = true; bag.accessible = true; bag.capacity = 2; }
            satchel_item item{};
            item.id = 4112; item.slot = 1; item.count = 8; item.generation = 12;
            snapshot.bags[0].items.push_back(item);
            return snapshot;
        }

        static auto Request(const satchel_snapshot& snapshot, const satchel_action action) -> satchel_action_request
        {
            satchel_action_request request{};
            request.action = action; request.identity = snapshot.identity; request.item = snapshot.bags[0].items[0];
            request.destination = 5;
            return request;
        }

        static auto IdentityAndPlanning(void) -> void
        {
            auto snapshot = Snapshot();
            auto request = Request(snapshot, satchel_action::move);
            satchel_actions::resource resource{}; resource.type = 7; resource.stack = 12;
            satchel_actions::plan plan{};
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty(), "an accessible bag with space accepts a move");
            auto stale = request; stale.item.generation--;
            Check(!satchel_actions::Plan(snapshot, stale, resource, plan).empty(), "a reused slot with a stale generation is rejected");
            stale = request; stale.item.extra.back() = 1;
            Check(!satchel_actions::Plan(snapshot, stale, resource, plan).empty(), "all 28 bytes of instance data participate in identity");
            stale = request; stale.item.count--;
            Check(!satchel_actions::Plan(snapshot, stale, resource, plan).empty(), "a changed stack quantity is rejected");
            stale = request; stale.identity = "world-2-123";
            Check(!satchel_actions::Plan(snapshot, stale, resource, plan).empty(), "the same character id on another world is rejected");
            auto offline = snapshot; offline.read_only = true;
            Check(!satchel_actions::Plan(offline, request, resource, plan).empty(), "offline character data cannot dispatch");
            stale = request; stale.item.read_only = true;
            Check(!satchel_actions::Plan(snapshot, stale, resource, plan).empty(), "storage slip projections cannot dispatch");

            auto target = request.item; target.bag = 5; target.slot = 1; target.count = 4;
            snapshot.bags[5].capacity = 1; snapshot.bags[5].items = {target};
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty() && plan.destination_slot == 1,
                "a full bag can accept an exactly compatible stack merge");
            snapshot.bags[5].items[0].extra.back() = 1;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a full bag cannot merge different extra data");
            snapshot.bags[5].items[0].extra.back() = 0; snapshot.bags[5].items[0].count = 5;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a merge cannot overfill the resource stack size");
            snapshot.bags[5].capacity = 2;
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty() && plan.destination_slot == 0x52,
                "an incompatible stack uses a distinct empty slot");
            request.quantity = 2; snapshot.bags[5].capacity = 1;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a partial transfer needs an empty slot even when a stack exists");
            request = Request(snapshot, satchel_action::move); request.destination = 8;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "consumables cannot enter a wardrobe");
            request.destination = 3;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "temporary bag transfers are blocked");
            request.destination = 1; snapshot.bags[1].accessible = false;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "loaded remote storage is not treated as accessible");

            request = Request(snapshot, satchel_action::split); request.quantity = 2;
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty() && plan.destination_slot == 0x52,
                "split creates a new slot within the source bag");
            snapshot.bags[0].capacity = 1;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "split does not reuse its still occupied source slot");
            request.quantity = 8; snapshot.bags[0].capacity = 2;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "split must leave an original stack");
        }

        static auto RestrictionsAndPackets(void) -> void
        {
            auto snapshot = Snapshot();
            auto request = Request(snapshot, satchel_action::drop);
            satchel_actions::resource resource{}; resource.type = 7; resource.flags = 0x0200; resource.targets = 1;
            satchel_actions::plan plan{};
            snapshot.bags[0].items[0].flags = request.item.flags = 4;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a reserved stack cannot be dropped");
            snapshot.bags[0].items[0].flags = request.item.flags = 0;
            snapshot.bags[0].items[0].price = request.item.price = 100;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a bazaar stack cannot be dropped");
            auto slip = snapshot; auto slipRequest = request;
            slip.bags[0].items[0].id = slipRequest.item.id = 29312;
            slip.bags[0].items[0].price = slipRequest.item.price = 0;
            slip.bags[0].items[0].extra[0] = slipRequest.item.extra[0] = 1;
            Check(!satchel_actions::Plan(slip, slipRequest, resource, plan).empty(), "a storage slip with stored items cannot be dropped");
            request.action = satchel_action::bazaar; request.price = 0;
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty(), "an existing bazaar listing can be removed");
            resource.flags |= 0x4000;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "exclusive items cannot be newly listed");
            resource.flags = 0x0200;
            snapshot.bags[0].items[0].price = request.item.price = 0;
            request.action = satchel_action::use;
            Check(satchel_actions::Plan(snapshot, request, resource, plan).empty(), "a self-target consumable supports exact-slot use");
            resource.targets = 0x20;
            Check(!satchel_actions::Plan(snapshot, request, resource, plan).empty(), "target-required use is delegated to the native menu");

            packets sent{};
            request = Request(snapshot, satchel_action::move); plan.quantity = 3; plan.destination_slot = 0x52;
            satchel_actions::Dispatch(sent, request, plan, 0x12345678, 0x456);
            const auto& move = sent.outgoing.back();
            Check(move.first == 0x029 && move.second.size() == 12 && move.second[4] == 3 && move.second[8] == 0 &&
                move.second[9] == 5 && move.second[10] == 1 && move.second[11] == 0x52, "move sends the captured source slot and planned destination");
            request.action = satchel_action::use;
            satchel_actions::Dispatch(sent, request, plan, 0x12345678, 0x456);
            const auto& use = sent.outgoing.back();
            Check(use.first == 0x037 && use.second.size() == 20 && use.second[4] == 0x78 && use.second[7] == 0x12 &&
                use.second[12] == 0x56 && use.second[13] == 4 && use.second[14] == 1 && use.second[16] == 0,
                "use addresses the exact source slot and self identity without a name command");
            request.action = satchel_action::equip; plan.equipment_slot = 14;
            satchel_actions::Dispatch(sent, request, plan, 123, 0x456);
            Check(sent.outgoing.back().first == 0x050 && sent.outgoing.back().second[4] == 1 && sent.outgoing.back().second[5] == 14,
                "equip preserves the chosen equipment slot");
            sent.outgoing.clear(); request.action = satchel_action::bazaar; request.price = 54321;
            satchel_actions::Dispatch(sent, request, plan, 123, 0x456);
            Check(sent.outgoing.size() == 3 && sent.outgoing[0].first == 0x10B && sent.outgoing[1].first == 0x10A &&
                sent.outgoing[2].first == 0x109 && sent.outgoing[1].second[4] == 1 && sent.outgoing[1].second[8] == 0x31 && sent.outgoing[1].second[9] == 0xD4,
                "bazaar closes, updates the exact slot, then reopens in order");
        }

        static auto Observations(void) -> void
        {
            const auto before = Snapshot();
            satchel_actions actions{};
            Check(actions.Outcome() == satchel_action_outcome::none, "a fresh dispatcher has no action outcome");
            const auto start = [&](const satchel_action action) {
                actions.before_ = before; actions.pending_ = Request(before, action); actions.busy_ = true;
                actions.inventory_ack_ = false;
                actions.outcome_ = satchel_action_outcome::pending;
                actions.plan_ = {}; actions.plan_.quantity = 8; actions.deadline_ = 5000;
            };
            start(satchel_action::move);
            actions.Observe(before, 2000);
            Check(actions.Busy() && actions.Outcome() == satchel_action_outcome::pending, "submitting a move does not immediately report success");
            auto partial = before; partial.bags[0].items.clear();
            actions.Observe(partial, 2200);
            Check(actions.Busy(), "source disappearance alone cannot confirm a transfer");
            auto moved = partial; auto item = before.bags[0].items[0]; item.bag = 5; item.slot = 2; moved.bags[5].items.push_back(item);
            actions.Observe(moved, 2300);
            Check(actions.Busy(), "both bag changes still require the inventory completion acknowledgment");
            Acknowledge(actions, 1u << 0); actions.Observe(moved, 2310);
            Check(actions.Busy(), "completion must include both affected bags");
            Acknowledge(actions, (1u << 0) | (1u << 5)); actions.Observe(moved, 2320);
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::observed, "a complete two-sided transfer is released immediately after acknowledgment");
            start(satchel_action::move); Acknowledge(actions); actions.Observe(partial, 2400);
            Check(actions.Busy(), "an acknowledgment alone cannot confirm an incomplete transfer");
            auto loading = before; loading.ready = false; actions.Observe(loading, 2500);
            Check(actions.Busy(), "temporary loading does not release an in-flight action");
            start(satchel_action::move); actions.Observe(before, 5000);
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::unconfirmed, "an unchanged inventory times out without claiming success");
            start(satchel_action::sort); actions.Observe(before, 2000);
            Check(actions.Busy(), "an unchanged sort waits for native completion");
            Acknowledge(actions); actions.Observe(before, 2100);
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::observed, "native completion confirms an unchanged, fully stacked bag without a five-second wait");
            start(satchel_action::move); Acknowledge(actions, 0xFFFFFFFF, 0); actions.Observe(moved, 2200);
            Check(actions.Busy(), "loading packets cannot confirm a transfer");
            std::array<uint8_t, 8> truncated{}; truncated[4] = 1; actions.HandlePacket(0x001D, truncated);
            Acknowledge(actions, 0xFFFFFFFF, 2); actions.Observe(moved, 2210);
            Check(actions.Busy(), "truncated and unknown completion packets cannot unlock transfers");
            Acknowledge(actions); Acknowledge(actions, 0xFFFFFFFF, 0); actions.Observe(moved, 2300);
            Check(actions.Busy(), "a new loading phase invalidates an earlier completion acknowledgment");
            start(satchel_action::move); actions.HandlePacket(0x000A, {});
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::cancelled, "zoning cancels pending actions before inventory is reused");
            start(satchel_action::equip); auto equipped = before; equipped.bags[0].items[0].equipped = true;
            actions.Observe(equipped, 2200, false);
            Check(actions.Busy(), "equipment in another ring or ear slot does not confirm the selected slot");
            actions.Observe(equipped, 2300, true);
            Check(!actions.Busy(), "equipment confirmation requires the actual selected equipment slot");
            start(satchel_action::drop); auto changedIdentity = before; changedIdentity.identity = "world-2-123";
            actions.Observe(changedIdentity, 2200);
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::cancelled, "character changes cancel pending observation");
            start(satchel_action::drop); actions.SetRestricted(true);
            Check(!actions.Busy(), "native ownership or suspension releases the pending observer");
            Check(!actions.Submit(nullptr, before, Request(before, satchel_action::drop), 3000) &&
                actions.Outcome() == satchel_action_outcome::rejected, "a refused dispatch exposes a typed rejection");
        }

        static auto Acknowledge(satchel_actions& actions, const uint32_t flags = 0xFFFFFFFF, const uint8_t state = 1) -> void
        {
            std::array<uint8_t, 12> packet{}; packet[4] = state;
            for (size_t index = 0; index < 4; ++index) packet[8 + index] = static_cast<uint8_t>(flags >> (index * 8));
            actions.HandlePacket(0x001D, packet);
        }

        static auto Stacking(void) -> void
        {
            auto snapshot = Snapshot();
            auto item = snapshot.bags[0].items[0]; item.slot = 2; item.count = 2;
            snapshot.bags[0].items.push_back(item);
            const std::array<std::pair<uint16_t, uint16_t>, 1> limits{{{item.id, uint16_t{12}}}};
            Check(!satchel_actions::SortComplete(snapshot.bags[0], limits), "two partial stacks need a native sort");
            Check(!satchel_actions::SortComplete(snapshot.bags[0], {}), "unknown stack limits cannot prove a duplicate pair is complete");
            snapshot.bags[0].items[1].count = 12;
            Check(satchel_actions::SortComplete(snapshot.bags[0], limits), "a full stack and a partial stack need no native sort");
            snapshot.bags[0].items[1].count = 2; snapshot.bags[0].items[1].flags = 4;
            Check(satchel_actions::SortComplete(snapshot.bags[0], limits), "locked stacks are excluded from native stacking");
            snapshot.bags[0].items[1].flags = 0; snapshot.bags[0].items[1].equipped = true;
            Check(satchel_actions::SortComplete(snapshot.bags[0], limits), "equipped stacks are excluded from native stacking");
            snapshot.bags[0].items[1].equipped = false; snapshot.bags[0].items[1].price = 100;
            Check(satchel_actions::SortComplete(snapshot.bags[0], limits), "bazaar stacks are excluded from native stacking");
            snapshot.bags[0].items[1].price = 0; item.slot = 3; item.count = 1; snapshot.bags[0].items.push_back(item);
            satchel_actions actions{}; actions.before_ = snapshot; actions.pending_ = Request(snapshot, satchel_action::sort);
            actions.plan_.stack_limits.assign(limits.begin(), limits.end()); actions.busy_ = true; actions.deadline_ = 5000;
            Acknowledge(actions);
            snapshot.bags[0].items[0].count = 10; snapshot.bags[0].items.erase(snapshot.bags[0].items.begin() + 1);
            actions.Observe(snapshot, 1200);
            Check(actions.Busy(), "an intermediate merge and completion packet cannot finish a multi-stack sort");
            snapshot.bags[0].items.pop_back(); actions.Observe(snapshot, 1205);
            Check(actions.Busy(), "removing a donor before the receiving count arrives cannot finish stacking");
            snapshot.bags[0].items[0].count = 11; actions.Observe(snapshot, 1210);
            Check(!actions.Busy() && actions.Outcome() == satchel_action_outcome::observed, "sort finishes when every eligible partial pair is combined");
            actions.next_sort_[5] = 2000;
            Check(!actions.SortReady(5, 1999) && actions.SortReady(5, 2000) && actions.SortReady(1, 1000),
                "native sort rate limits are per bag and leave other bags available");
        }
    };
}

int main(void)
{
    try
    {
        xiui::satchel_actions_test::IdentityAndPlanning();
        xiui::satchel_actions_test::RestrictionsAndPackets();
        xiui::satchel_actions_test::Observations();
        xiui::satchel_actions_test::Stacking();
        std::puts("Satchel actions smoke passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
