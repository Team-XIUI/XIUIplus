#include "design_system.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    auto fail(const char* message, const int code) -> int
    {
        std::fprintf(stderr, "theme reload smoke failed: %s\n", message);
        return code;
    }

    auto write_theme(const std::string& path, const char* content) -> bool
    {
        const HANDLE file = ::CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        DWORD written{};
        const DWORD size = static_cast<DWORD>(std::strlen(content));
        const BOOL result = ::WriteFile(file, content, size, &written, nullptr);
        ::FlushFileBuffers(file);
        ::CloseHandle(file);
        return result != FALSE && written == size;
    }
}

int main(void)
{
    char temporary[MAX_PATH]{};
    if (::GetTempPathA(MAX_PATH, temporary) == 0)
        return fail("GetTempPathA", 1);
    char fileName[MAX_PATH]{};
    if (::GetTempFileNameA(temporary, "xth", 0, fileName) == 0)
        return fail("GetTempFileNameA", 2);
    const std::string path = fileName;

    constexpr char firstTheme[] =
        "[colors]\n"
        "gold=FF010203\n"
        "[meter]\n"
        "border=FF4D463C\n"
        "[text.accent]\n"
        "scale=1.25\n"
        "shadow_soft=1\n";
    if (!write_theme(path, firstTheme))
        return fail("write initial theme", 3);

    xiui::ui::design_system design{};
    design.SetThemePath(path.c_str());
    if (design.GetTheme().colors.gold != D3DCOLOR_ARGB(255, 1, 2, 3))
        return fail("initial color parse", 4);
    if (design.GetTheme().meter.border != D3DCOLOR_ARGB(255, 0, 0, 0))
        return fail("legacy meter border migration", 4);
    if (std::abs(design.GetTheme().accent_text.scale - 1.25f) > 0.001f || !design.GetTheme().accent_text.shadow.soft)
        return fail("initial typography parse", 5);

    constexpr char secondTheme[] =
        "[colors]\n"
        "gold=80112233\n"
        "[text.accent]\n"
        "scale=99\n"
        "letter_spacing=2.5\n";
    if (!write_theme(path, secondTheme) || !design.ReloadTheme())
        return fail("reload changed theme", 6);
    if (design.GetTheme().colors.gold != D3DCOLOR_ARGB(128, 17, 34, 51))
        return fail("reloaded alpha color", 7);
    if (std::abs(design.GetTheme().accent_text.scale - 3.0f) > 0.001f ||
        std::abs(design.GetTheme().accent_text.letter_spacing - 2.5f) > 0.001f)
        return fail("reloaded typography clamp", 8);

    constexpr char polledTheme[] =
        "[colors]\n"
        "gold=FF445566\n";
    ::Sleep(20);
    if (!write_theme(path, polledTheme))
        return fail("write polled theme", 9);
    ::Sleep(300);
    if (design.PollTheme())
        return fail("theme committed before stable poll", 10);
    ::Sleep(300);
    if (!design.PollTheme() || design.GetTheme().colors.gold != D3DCOLOR_ARGB(255, 68, 85, 102))
        return fail("stable polling reload", 11);

    const HANDLE writer = ::CreateFileA(
        path.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (writer == INVALID_HANDLE_VALUE)
        return fail("open concurrent writer", 12);
    const bool loadedDuringWrite = design.ReloadTheme();
    ::CloseHandle(writer);
    if (loadedDuringWrite)
        return fail("theme loaded while writer active", 13);
    if (!design.ReloadTheme())
        return fail("theme did not recover after writer closed", 14);

    const auto customAccent = design.GetTheme().colors.accent;
    const auto priorSurface = design.GetTheme().colors.surface;
    design.SetWindowPalette(0xFF183024);
    if (design.GetTheme().colors.surface == priorSurface || design.GetTheme().colors.accent != customAccent)
        return fail("native window palette must update automatic roles while retaining custom colors", 15);
    const auto firstControl = design.GetTheme().colors.control;
    design.SetWindowPalette(0xFF302418);
    if (design.GetTheme().colors.control == firstControl || !design.ReloadTheme() || design.GetTheme().colors.control == firstControl)
        return fail("window palette changes survive a theme file reload", 16);
    design.Shutdown();
    ::DeleteFileA(path.c_str());
    std::puts("theme reload smoke passed");
    return 0;
}
