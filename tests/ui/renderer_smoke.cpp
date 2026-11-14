#include "hud_runtime.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
    auto __stdcall WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT
    {
        return ::DefWindowProcA(window, message, wParam, lParam);
    }

    auto fail(const char* message, const int code) -> int
    {
        std::fprintf(stderr, "renderer smoke failed: %s\n", message);
        return code;
    }
}

int main(void)
{
    WNDCLASSA windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = ::GetModuleHandleA(nullptr);
    windowClass.lpszClassName = "XiuiRendererSmoke";
    if (::RegisterClassA(&windowClass) == 0)
        return fail("RegisterClassA", 1);

    const HWND window = ::CreateWindowExA(
        WS_EX_TOOLWINDOW,
        windowClass.lpszClassName,
        "XIUI+ renderer smoke",
        WS_POPUP,
        0,
        0,
        640,
        480,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr);
    if (window == nullptr)
        return fail("CreateWindowExA", 2);

    IDirect3D8* direct3d = ::Direct3DCreate8(D3D_SDK_VERSION);
    if (direct3d == nullptr)
        return fail("Direct3DCreate8", 3);

    D3DDISPLAYMODE displayMode{};
    if (FAILED(direct3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &displayMode)))
        return fail("GetAdapterDisplayMode", 4);

    D3DPRESENT_PARAMETERS present{};
    present.Windowed = TRUE;
    present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    present.BackBufferFormat = displayMode.Format;
    present.BackBufferWidth = 640;
    present.BackBufferHeight = 480;
    present.hDeviceWindow = window;

    IDirect3DDevice8* device{};
    if (FAILED(direct3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            window,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &present,
            &device)))
    {
        direct3d->Release();
        ::DestroyWindow(window);
        return fail("CreateDevice", 5);
    }

    D3DCAPS8 caps{};
    if (FAILED(device->GetDeviceCaps(&caps)))
        return fail("GetDeviceCaps", 6);
    const bool shaderExpected = caps.PixelShaderVersion >= D3DPS_VERSION(1, 1);

    xiui::hud_runtime runtime{};
    runtime.Initialize(nullptr);
    runtime.SetDevice(device);
    if (shaderExpected && !runtime.IsGpuShaderActive())
    {
        runtime.Shutdown();
        device->Release();
        direct3d->Release();
        ::DestroyWindow(window);
        return fail("advertised Direct3D8 pixel shader unavailable", 7);
    }

    xiui::hud_snapshot snapshot{};
    ::strncpy_s(snapshot.player_name, "Smoke Test", _TRUNCATE);
    ::strncpy_s(snapshot.main_job, "RDM 99", _TRUNCATE);
    ::strncpy_s(snapshot.sub_job, "NIN 49", _TRUNCATE);
    snapshot.hp = 1200;
    snapshot.hp_max = 1500;
    snapshot.mp = 700;
    snapshot.mp_max = 900;
    snapshot.tp = 1300;
    snapshot.target_valid = true;
    ::strncpy_s(snapshot.target_name, "Tunnel Worm", _TRUNCATE);
    snapshot.target_hp_percent = 67;
    snapshot.target_distance = 8.4f;
    snapshot.exp_current = 18000;
    snapshot.exp_needed = 32000;

    device->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    device->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
    device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
    device->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_TEMP);
    device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_XRGB(24, 24, 24), 1.0f, 0);
    if (FAILED(device->BeginScene()))
        return fail("BeginScene", 8);
    runtime.Render(snapshot);
#if defined(XIUI_ENABLE_TEST_FAULTS)
    runtime.SetRenderFaultForTesting(true);
    bool renderFaultObserved = false;
    try
    {
        runtime.Render(snapshot);
    }
    catch (...)
    {
        renderFaultObserved = true;
    }
    runtime.SetRenderFaultForTesting(false);
    if (!renderFaultObserved)
        return fail("injected render exception", 9);
#endif

    xiui::player_module resetPlayer{};
    const auto resetTheme = xiui::ui::theme::CreateDefault();
    resetPlayer.State().bounds = {91.0f, 73.0f, 555.0f, 101.0f};
    resetPlayer.Settings().show_identity = false;
    resetPlayer.Settings().hp_style.corner_radius = 1.0f;
    resetPlayer.Settings().name_text.style.scale = 2.0f;
    resetPlayer.ResetDefaults(resetTheme);
    if (!resetPlayer.Settings().show_identity || resetPlayer.Settings().hp_style.corner_radius != 8.0f ||
        resetPlayer.Settings().name_text.style.scale != resetTheme.title_text.scale ||
        resetPlayer.State().bounds.x != 91.0f || resetPlayer.State().bounds.y != 73.0f ||
        resetPlayer.State().bounds.width != 555.0f || resetPlayer.State().bounds.height != 101.0f)
        return fail("module defaults reset", 9);

    char temporaryDirectory[MAX_PATH]{};
    char textProfilePath[MAX_PATH]{};
    if (::GetTempPathA(MAX_PATH, temporaryDirectory) == 0)
        return fail("module text profile temp path", 9);
    std::snprintf(textProfilePath, sizeof(textProfilePath), "%sXIUIplus-text-profile-smoke.ini", temporaryDirectory);
    ::DeleteFileA(textProfilePath);
    xiui::settings_store textStore{textProfilePath};
    resetPlayer.Settings().name_text.style.scale = 1.35f;
    resetPlayer.Settings().name_text.style.top_color = D3DCOLOR_ARGB(180, 12, 34, 56);
    resetPlayer.Settings().name_text.placement = 2;
    resetPlayer.Settings().name_text.offset_x = 17.0f;
    resetPlayer.Save(textStore);
    textStore.Flush();
    xiui::player_module loadedPlayer{};
    loadedPlayer.ApplyMeterDefaults(resetTheme.meter);
    loadedPlayer.ApplyTextDefaults(resetTheme);
    loadedPlayer.Load(textStore);
    ::DeleteFileA(textProfilePath);
    if (std::abs(loadedPlayer.Settings().name_text.style.scale - 1.35f) > 0.001f ||
        loadedPlayer.Settings().name_text.style.top_color != D3DCOLOR_ARGB(180, 12, 34, 56) ||
        loadedPlayer.Settings().name_text.placement != 2 || loadedPlayer.Settings().name_text.offset_x != 17.0f)
        return fail("module text profile persistence", 9);

    xiui::ui::design_system showcase{};
    showcase.SetDevice(device);
    const auto& defaultMeter = showcase.GetTheme().meter;
    if (defaultMeter.border != D3DCOLOR_ARGB(255, 0, 0, 0))
        return fail("default meter border color", 9);
    if (defaultMeter.bookends || defaultMeter.corner_radius != 8.0f || defaultMeter.border_width != 2.0f ||
        !defaultMeter.shadow_enabled || defaultMeter.shadow_strength != 0.2f || defaultMeter.shadow_offset_x != 0.0f ||
        defaultMeter.shadow_offset_y != 1.0f || defaultMeter.shadow_blur != 4.0f)
        return fail("default meter style", 9);
    if (!showcase.BeginFrame())
        return fail("design system BeginFrame", 9);
    auto showcaseText = showcase.GetTheme().accent_text;
    showcaseText.shadow.soft = true;
    showcaseText.sheen.enabled = true;
    showcase.DrawPanel({360.0f, 360.0f, 250.0f, 88.0f}, showcase.GetTheme().hud_panel);
    showcase.DrawMeter({372.0f, 409.0f, 220.0f, 19.0f}, 0.72f, D3DCOLOR_XRGB(120, 196, 238), 0.5f);
    auto decoratedMeter = showcase.GetTheme().meter;
    decoratedMeter.bookends = true;
    decoratedMeter.shadow_enabled = true;
    decoratedMeter.shadow_strength = 0.8f;
    decoratedMeter.shadow_blur = 8.0f;
    decoratedMeter.border_width = 3.0f;
    decoratedMeter.track_opacity = 0.45f;
    decoratedMeter.glow_enabled = true;
    decoratedMeter.fill_glow_enabled = true;
    decoratedMeter.fill_glow_strength = 0.7f;
    decoratedMeter.fill_glow_radius = 6.0f;
    showcase.DrawMeter(
        {372.0f, 432.0f, 176.0f, 14.0f},
        0.62f,
        D3DCOLOR_XRGB(64, 91, 130),
        D3DCOLOR_XRGB(120, 196, 238),
        0.25f,
        decoratedMeter);
    auto roundedMeter = decoratedMeter;
    roundedMeter.bookends = false;
    roundedMeter.corner_radius = 7.0f;
    showcase.DrawMeter(
        {372.0f, 451.0f, 176.0f, 14.0f},
        0.48f,
        D3DCOLOR_XRGB(92, 58, 32),
        D3DCOLOR_XRGB(235, 160, 72),
        0.75f,
        roundedMeter);
    showcase.DrawText("GPU STROKE + GLOW + SHEEN", {372.0f, 371.0f, 220.0f, 30.0f}, showcaseText);
    showcase.DrawToggle({560.0f, 432.0f, 30.0f, 16.0f}, true);
    showcase.DrawEditFrame({360.0f, 360.0f, 250.0f, 88.0f});
    showcase.EndFrame();
    showcase.Shutdown();

    xiui::ui::design_system fallback{};
    fallback.SetGpuShaderEnabled(false);
    fallback.SetDevice(device);
    if (fallback.IsGpuShaderActive())
        return fail("fixed-function fallback selection", 10);
    if (!fallback.BeginFrame())
        return fail("fallback BeginFrame", 11);
    auto fallbackText = fallback.GetTheme().body_text;
    fallbackText.letter_spacing = 1.5f;
    fallback.DrawText("FIXED FALLBACK", {372.0f, 443.0f, 150.0f, 24.0f}, fallbackText);
    fallback.EndFrame();
    fallback.Shutdown();

    if (FAILED(device->EndScene()))
        return fail("EndScene", 12);

    DWORD alphaTest{};
    DWORD fillMode{};
    DWORD shadeMode{};
    DWORD colorWrite{};
    DWORD colorOperation{};
    DWORD resultArgument{};
    DWORD textureTransform{};
    device->GetRenderState(D3DRS_ALPHATESTENABLE, &alphaTest);
    device->GetRenderState(D3DRS_FILLMODE, &fillMode);
    device->GetRenderState(D3DRS_SHADEMODE, &shadeMode);
    device->GetRenderState(D3DRS_COLORWRITEENABLE, &colorWrite);
    device->GetTextureStageState(0, D3DTSS_COLOROP, &colorOperation);
    device->GetTextureStageState(0, D3DTSS_RESULTARG, &resultArgument);
    device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &textureTransform);

    const bool restored =
        alphaTest == TRUE &&
        fillMode == D3DFILL_WIREFRAME &&
        shadeMode == D3DSHADE_FLAT &&
        colorWrite == D3DCOLORWRITEENABLE_RED &&
        colorOperation == D3DTOP_ADD &&
        resultArgument == D3DTA_TEMP &&
        textureTransform == D3DTTFF_COUNT2;

    const HRESULT presentResult = device->Present(nullptr, nullptr, nullptr, nullptr);
    runtime.Shutdown();
    device->Release();
    direct3d->Release();
    ::DestroyWindow(window);
    ::UnregisterClassA(windowClass.lpszClassName, windowClass.hInstance);

    if (!restored)
        return fail("Direct3D state was not restored", 13);
    if (FAILED(presentResult))
        return fail("Present", 14);

    std::puts("renderer smoke passed");
    return 0;
}
