#include "notifications/inventory_tracker.hpp"

#include <array>
#include <cstdio>

namespace
{
    auto check(bool condition, const char* message) -> bool
    {
        if (!condition)
            std::fprintf(stderr, "Inventory notification check failed: %s\n", message);
        return condition;
    }
}

int main()
{
    xiui::inventory_tracker tracker;
    const xiui::item_totals original{{uint16_t{100}, 5u}, {uint16_t{101}, 23u}, {uint16_t{102}, 4u}, {uint16_t{103}, 9u}, {uint16_t{104}, 1u}};
    std::array<uint8_t, 12> complete{};
    complete[4] = 1;
    tracker.Reset(0);
    tracker.CanSample(0, 1, 3, 3);
    if (!check(tracker.CanSample(300, 1, 3, 3) && tracker.Sample(original, 300).empty(), "hot load establishes a silent baseline"))
        return 1;
    auto gained = original;
    gained[100] += 1;
    tracker.CanSample(1000, 2, 3, 3);
    if (!check(!tracker.CanSample(1299, 2, 3, 3) && tracker.CanSample(1300, 2, 3, 3), "ordinary item updates settle"))
        return 1;
    if (!check(tracker.Sample(gained, 1300) == xiui::item_totals{{uint16_t{100}, 1u}}, "report only the newly gained amount"))
        return 1;

    tracker.BeginSync(2000);
    if (!check(!tracker.CanSample(20000, 2, 3, 3), "a slow homepoint reload cannot time out into an empty baseline"))
        return 1;
    tracker.CanSample(21000, 3, 0, 3);
    if (!check(!tracker.CanSample(22000, 4, 1, 3), "partially loaded bags cannot establish a baseline"))
        return 1;
    tracker.HandleSyncPacket(complete, 25000);
    tracker.Invalidate(25000);
    tracker.CanSample(25000, 5, 3, 3);
    if (!check(tracker.CanSample(25300, 5, 3, 3) && tracker.Sample(gained, 25300).empty(), "zone completion survives session refresh and never replays possessions"))
        return 1;
    tracker.CanSample(26000, 6, 3, 3);
    if (!check(tracker.CanSample(26300, 6, 3, 3) && tracker.Sample(gained, 26300).empty(), "bag transfers with unchanged totals stay silent"))
        return 1;

    std::array<uint8_t, 12> updating{};
    tracker.HandleSyncPacket(updating, 27000);
    tracker.CanSample(27000, 7, 3, 3);
    if (!check(!tracker.CanSample(28000, 7, 3, 3), "wait for a normal inventory batch to finish"))
        return 1;
    gained[101] += 2;
    tracker.HandleSyncPacket(complete, 28000);
    if (!check(tracker.CanSample(28300, 7, 3, 3) && tracker.Sample(gained, 28300) == xiui::item_totals{{uint16_t{101}, 2u}}, "normal completion packets preserve real gains"))
        return 1;
    gained[100] = 2;
    tracker.CanSample(29000, 8, 3, 3);
    if (!check(tracker.CanSample(29300, 8, 3, 3) && tracker.Sample(gained, 29300).empty(), "consumption updates the baseline silently"))
        return 1;
    gained[100] = 3;
    tracker.CanSample(30000, 9, 3, 3);
    if (!check(tracker.CanSample(30300, 9, 3, 3) && tracker.Sample(gained, 30300) == xiui::item_totals{{uint16_t{100}, 1u}}, "gains after a loss still report correctly"))
        return 1;

    tracker.CanSample(31000, 10, 0, 3);
    if (!check(!tracker.CanSample(35000, 10, 0, 3), "cleared loading flags suspend sampling without a zone change"))
        return 1;
    tracker.CanSample(40000, 11, 3, 3);
    if (!check(tracker.CanSample(40300, 11, 3, 3) && tracker.Sample(gained, 40300).empty(), "loading flag recovery rebuilds the baseline silently"))
        return 1;
    tracker.BeginSync(41000);
    tracker.HandleSyncPacket({complete.data(), 11}, 42000);
    auto invalid = complete;
    invalid[4] = 2;
    tracker.HandleSyncPacket(invalid, 42000);
    if (!check(!tracker.CanSample(45000, 11, 3, 3), "malformed completion packets cannot release the loading guard"))
        return 1;
    tracker.HandleSyncPacket(complete, 45000);
    if (!check(tracker.CanSample(45300, 11, 3, 3) && tracker.Sample(original, 45300).empty(), "repeated same-zone reloads stay silent"))
        return 1;
    tracker.Invalidate(46000);
    if (!check(tracker.CanSample(46300, 11, 3, 3) && tracker.Sample(original, 46300).empty(), "logout or character changes discard the previous baseline"))
        return 1;
    if (!check(!tracker.CanSample(50000, 11, 0, 0), "missing container capacity is not an empty ready inventory"))
        return 1;
    std::puts("Inventory loading, homepoint, transfer, and genuine gain checks passed.");
    return 0;
}
