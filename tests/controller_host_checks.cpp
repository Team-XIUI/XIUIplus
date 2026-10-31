#include "runtime_host.hpp"

#include <atomic>
#include <cstdio>
#include <thread>

namespace xiui
{
    struct controller_host_test
    {
        struct state
        {
            HANDLE rendering{::CreateEventA(nullptr, TRUE, FALSE, nullptr)};
            HANDLE resume{::CreateEventA(nullptr, TRUE, FALSE, nullptr)};
            std::atomic_uint calls{};
            std::atomic_bool block_render{true};
            std::atomic_bool wrong_thread{};
            DWORD render_thread{};
            uint32_t routing{ControllerEntryBit(controller_action::cycle_window) |
                ControllerEntryBit(controller_action::up) | ControllerEntryBit(controller_action::down)};
            ~state() { ::CloseHandle(this->rendering); ::CloseHandle(this->resume); }
        };

        static auto __stdcall InputState(void* instance) -> uint32_t { return static_cast<state*>(instance)->routing; }
        static auto __stdcall KeyboardFocused(void*) -> BOOL { return FALSE; }
        static auto __stdcall KeyboardState(void*) -> uint32_t { return 0; }
        static auto __stdcall Status(void*, const controller_status*) -> void {}
        static auto __stdcall Controller(void* instance, const controller_event* event) -> BOOL
        {
            auto& value = *static_cast<state*>(instance);
            if (::GetCurrentThreadId() != value.render_thread) value.wrong_thread = true;
            ++value.calls;
            if (event->action == controller_action::cycle_window) value.routing |= static_cast<uint32_t>(controller_capture_mode::navigation);
            return TRUE;
        }
        static auto __stdcall Render(void* instance, const hud_snapshot*) -> BOOL
        {
            auto& value = *static_cast<state*>(instance);
            if (value.block_render.exchange(false))
            {
                ::SetEvent(value.rendering);
                ::WaitForSingleObject(value.resume, 5000);
            }
            return TRUE;
        }

        static auto Run(void) -> bool
        {
            state value;
            runtime_host host;
            host.instance_ = &value;
            host.api_.controller_input_state = &InputState;
            host.api_.chat_focused = &KeyboardFocused;
            host.api_.keyboard_input_state = &KeyboardState;
            host.api_.handle_controller = &Controller;
            host.api_.update_controller_status = &Status;
            host.api_.render = &Render;
            host.RefreshInputState();
            const auto event = [](controller_action action) {
                return controller_event{action, controller_backend::xinput, 0, true, false, false, ::GetTickCount64()};
            };
            if (host.HandleController(event(controller_action::down), true)) return false;
            if (!host.HandleController(event(controller_action::cycle_window)) ||
                host.ControllerCapture() != controller_capture_mode::navigation || value.calls != 0) return false;

            std::thread renderer([&]() {
                value.render_thread = ::GetCurrentThreadId();
                host.Render({});
                host.Render({});
            });
            const bool rendering = ::WaitForSingleObject(value.rendering, 2000) == WAIT_OBJECT_0;
            std::atomic_bool callback_done{};
            std::atomic_bool accepted{};
            std::thread callback([&]() {
                accepted = host.HandleController(event(controller_action::confirm));
                host.ControllerCapture();
                host.ChatFocused();
                host.UpdateControllerStatus({});
                callback_done = true;
            });
            const auto deadline = ::GetTickCount64() + 1000;
            while (!callback_done && ::GetTickCount64() < deadline) ::Sleep(1);
            const bool independent = callback_done && value.calls == 1;
            ::SetEvent(value.resume);
            callback.join();
            renderer.join();
            if (!rendering || !independent || !accepted || value.calls != 2 || value.wrong_thread) return false;

            value.render_thread = ::GetCurrentThreadId();
            host.HandleController(event(controller_action::confirm));
            host.SuspendControllerInput();
            host.Render({});
            if (value.calls != 2) return false;
            host.HandleController(event(controller_action::confirm));
            value.routing += 1u << 16;
            host.RefreshInputState();
            host.Render({});
            if (value.calls != 2) return false;
            auto stale = event(controller_action::confirm);
            stale.timestamp -= 1000;
            host.HandleController(stale);
            host.Render({});
            if (value.calls != 2) return false;
            host.Shutdown();
            return !host.HandleController(event(controller_action::confirm)) && !host.ChatFocused();
        }
    };

    auto RunControllerHostChecks(void) -> bool
    {
        const bool result = controller_host_test::Run();
        if (!result) std::fputs("Controller callback isolation/lifecycle check failed.\n", stderr);
        return result;
    }
}
