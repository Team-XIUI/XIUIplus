#include "callback_gate.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
    auto fail(const char* message, const int code) -> int
    {
        std::fprintf(stderr, "callback gate smoke failed: %s\n", message);
        return code;
    }
}

int main(void)
{
    int value = 42;
    xiui::callback_gate<int> gate{};
    gate.Set(&value);

    std::atomic<bool> entered{false};
    std::atomic<bool> releaseLease{false};
    std::thread callback{[&] {
        auto lease = gate.Enter();
        entered.store(true, std::memory_order_release);
        while (!releaseLease.load(std::memory_order_acquire))
            std::this_thread::yield();
        if (!lease || *lease.operator->() != 42)
            std::abort();
    }};
    while (!entered.load(std::memory_order_acquire))
        std::this_thread::yield();

    gate.Close();
    std::atomic<bool> waitReturned{false};
    std::thread waiter{[&] {
        gate.Wait();
        waitReturned.store(true, std::memory_order_release);
    }};
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    if (waitReturned.load(std::memory_order_acquire))
    {
        releaseLease.store(true, std::memory_order_release);
        callback.join();
        waiter.join();
        return fail("wait returned with callback active", 1);
    }

    releaseLease.store(true, std::memory_order_release);
    callback.join();
    waiter.join();
    if (!waitReturned.load(std::memory_order_acquire))
        return fail("wait did not observe callback exit", 2);

    {
        auto closedLease = gate.Enter();
        if (closedLease)
            return fail("closed gate admitted callback target", 3);
    }
    gate.Wait();
    std::puts("callback gate smoke passed");
    return 0;
}
