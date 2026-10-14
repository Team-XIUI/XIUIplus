#include "runtime_host.hpp"

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

namespace
{
    auto fail(const char* message, const int code) -> int
    {
        std::fprintf(stderr, "hot reload smoke failed: %s\n", message);
        return code;
    }

    auto join(const std::string& left, const char* right) -> std::string
    {
        return left + "\\" + right;
    }

    auto touch(const std::string& path) -> bool
    {
        const HANDLE file = ::CreateFileA(
            path.c_str(),
            FILE_WRITE_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        FILETIME now{};
        ::GetSystemTimeAsFileTime(&now);
        const BOOL result = ::SetFileTime(file, nullptr, nullptr, &now);
        ::CloseHandle(file);
        return result != FALSE;
    }

    auto publish_copy(const std::string& source, const std::string& destination) -> bool
    {
        const std::string staging = destination + ".staging";
        ::DeleteFileA(staging.c_str());
        if (::CopyFileA(source.c_str(), staging.c_str(), FALSE) == FALSE || !touch(staging))
            return false;
        return ::MoveFileExA(staging.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }

    auto publish_invalid(const std::string& destination) -> bool
    {
        const std::string staging = destination + ".staging";
        const HANDLE file = ::CreateFileA(staging.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        constexpr char invalid[] = "not a runtime dll";
        DWORD written{};
        const BOOL writeResult = ::WriteFile(file, invalid, sizeof(invalid), &written, nullptr);
        ::FlushFileBuffers(file);
        ::CloseHandle(file);
        if (writeResult == FALSE || written != sizeof(invalid) || !touch(staging))
            return false;
        return ::MoveFileExA(staging.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
}

int main(const int argc, char** argv)
{
    if (argc != 7)
        return fail("expected working, rejected, stateful, activation-failure, exit-failure, and render-failure runtime paths", 1);

    char temporary[MAX_PATH]{};
    if (::GetTempPathA(MAX_PATH, temporary) == 0)
        return fail("GetTempPathA", 2);
    char directoryName[96]{};
    std::snprintf(directoryName, sizeof(directoryName), "xiui-hot-reload-smoke-%lu", static_cast<unsigned long>(::GetCurrentProcessId()));
    const std::string root = std::string(temporary) + directoryName;
    const std::string plugins = join(root, "plugins");
    const std::string config = join(root, "config");
    const std::string published = join(plugins, "xiuinative-runtime.dll");
    std::snprintf(directoryName, sizeof(directoryName), "xiuinative-runtime-%lu", static_cast<unsigned long>(::GetCurrentProcessId()));
    const std::string shadowDirectory = std::string(temporary) + directoryName;

    ::CreateDirectoryA(root.c_str(), nullptr);
    ::CreateDirectoryA(plugins.c_str(), nullptr);
    ::CreateDirectoryA(config.c_str(), nullptr);
    if (!publish_copy(argv[1], published))
        return fail("initial atomic publish", 3);

    xiui::runtime_host host{};
    if (!host.Initialize(nullptr, root.c_str()) || !host.IsLoaded())
        return fail("initial shadow load", 4);
    const std::string initialBuild = host.GetBuildId();
    if (initialBuild.empty())
        return fail("runtime build identity", 5);

    host.SetVisible(false);
    ::Sleep(300);
    if (!publish_copy(argv[1], published))
        return fail("second atomic publish", 6);
    ::Sleep(300);
    std::string message;
    if (!host.PollReload(message) || message.find("hot-reloaded") == std::string::npos)
        return fail("runtime generation swap", 7);
    if (!host.IsLoaded() || host.IsVisible() || host.GetBuildId() != initialBuild)
        return fail("state preservation", 8);

    std::atomic<bool> keepCalling{true};
    std::thread concurrentCalls{[&host, &keepCalling] {
        xiui::hud_snapshot snapshot{};
        while (keepCalling.load(std::memory_order_relaxed))
        {
            host.Render(snapshot);
            host.HandleMouseMessage(WM_MOUSEMOVE, 0, 0, false);
        }
    }};
    const bool concurrentPublish = publish_copy(argv[1], published);
    ::Sleep(300);
    const bool concurrentSwap = concurrentPublish && host.PollReload(message);
    keepCalling.store(false, std::memory_order_relaxed);
    concurrentCalls.join();
    if (!concurrentPublish)
        return fail("concurrent atomic publish", 9);
    if (!concurrentSwap || !host.IsLoaded())
        return fail("render versus swap serialization", 10);

    if (!publish_copy(argv[1], published))
        return fail("transient retry publish", 11);
    const HANDLE publishedLock = ::CreateFileA(published.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (publishedLock == INVALID_HANDLE_VALUE)
        return fail("exclusive published-runtime lock", 12);
    ::Sleep(300);
    const bool transientReported = host.PollReload(message) && message.find("shadow-copy") != std::string::npos;
    ::CloseHandle(publishedLock);
    if (!transientReported)
        return fail("transient copy failure classification", 13);
    ::Sleep(600);
    if (!host.PollReload(message) || message.find("hot-reloaded") == std::string::npos)
        return fail("unchanged generation retry", 14);

    if (!publish_copy(argv[2], published))
        return fail("rejected candidate publish", 15);
    ::Sleep(300);
    if (!host.PollReload(message) || message.find("candidate validation failed") == std::string::npos)
        return fail("candidate activation rejection", 16);
    if (!host.IsLoaded() || host.IsVisible() || host.GetBuildId() != initialBuild)
        return fail("candidate rollback preservation", 17);

    if (!publish_copy(argv[3], published))
        return fail("stateful candidate publish", 18);
    ::Sleep(300);
    if (!host.PollReload(message) || host.GetBuildId() != "stateful-test-runtime")
        return fail("stateful candidate activation", 19);
    xiui::chat_event chat{};
    chat.sequence = 7; chat.timestamp = ::GetTickCount64(); chat.mode = 1; chat.windows = 1;
    strcpy_s(chat.text, "Private smoke history");
    host.HandleChat(chat);
    chat.windows = 2;
    host.HandleChat(chat);
    if (!host.HandleCommand("chatcount 1") || !host.HandleKey(VK_RETURN, 0, false) || !host.ChatFocused())
        return fail("chat delivery, native-window merge, and keyboard callback", 37);
    if (!publish_copy(argv[3], published)) return fail("chat state publish", 38);
    ::Sleep(300);
    if (!host.PollReload(message) || !host.HandleCommand("chatcount 1") || !host.ChatFocused() || !host.HandleCommand("draft preserved"))
        return fail("chat history replay and focused draft transfer", 39);
    host.ExitEdit();
    if (host.ChatFocused()) return fail("chat capture release", 40);
    if (!host.ToggleEdit() || !host.IsEditing())
        return fail("stateful edit-mode setup", 20);

    if (!publish_copy(argv[4], published))
        return fail("activation-failure publish", 21);
    ::Sleep(300);
    if (!host.PollReload(message) || message.find("Runtime activation failed") == std::string::npos)
        return fail("final activation failure", 22);
    if (host.GetBuildId() != "stateful-test-runtime" || !host.IsEditing())
        return fail("edit-state rollback", 23);

    if (!publish_invalid(published))
        return fail("invalid atomic publish", 24);
    ::Sleep(300);
    if (!host.PollReload(message) || message.find("previous runtime retained") == std::string::npos)
        return fail("invalid runtime containment", 25);
    if (!host.IsLoaded() || host.GetBuildId() != "stateful-test-runtime" || !host.IsEditing())
        return fail("invalid-runtime state preservation", 26);

    if (!publish_copy(argv[5], published))
        return fail("exit-failure publish", 27);
    ::Sleep(300);
    if (!host.PollReload(message) || host.GetBuildId() != "exit-failure-runtime" || !host.IsEditing())
        return fail("exit-failure candidate activation", 28);
    if (!publish_copy(argv[1], published))
        return fail("post-exit-failure candidate publish", 29);
    ::Sleep(300);
    if (!host.PollReload(message) || message.find("could not exit edit mode") == std::string::npos)
        return fail("edit deactivation failure containment", 30);
    if (host.GetBuildId() != "exit-failure-runtime" || !host.IsEditing())
        return fail("edit deactivation rollback", 31);

    host.Shutdown();
    if (!publish_copy(argv[6], published))
        return fail("render-failure publish", 32);
    if (!host.Initialize(nullptr, root.c_str()) || host.GetBuildId() != "render-failure-runtime")
        return fail("render-failure candidate activation", 33);
    xiui::hud_snapshot snapshot{};
    if (host.Render(snapshot) || !host.HasFaulted())
        return fail("render failure quarantine", 34);
    if (host.Render(snapshot))
        return fail("quarantined runtime rendered again", 35);

    host.Shutdown();
    if (::GetFileAttributesA(shadowDirectory.c_str()) != INVALID_FILE_ATTRIBUTES)
        return fail("shadow directory cleanup", 36);
    ::DeleteFileA(published.c_str());
    ::DeleteFileA((published + ".staging").c_str());
    ::RemoveDirectoryA(config.c_str());
    ::RemoveDirectoryA(plugins.c_str());
    ::RemoveDirectoryA(root.c_str());
    std::puts("hot reload smoke passed");
    return 0;
}
