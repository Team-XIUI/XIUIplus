#include "plugin.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void
    {
        if (!condition) throw std::runtime_error{message};
    }
}

namespace xiui
{
    struct world_overlay_test
    {
        static auto Run(IDirect3DDevice8* device) -> void
        {
            using Microsoft::WRL::ComPtr;
            plugin host;
            host.device_=device;
            const auto captureRoot=std::filesystem::temp_directory_path()/("xiui-ring-shaders-"+std::to_string(::GetCurrentProcessId()));
            host.ring_capture_.Initialize(captureRoot,"test-version","test-build");
            Check(host.HandleCommand(0,"/xiui rings capture",false) && host.HandleCommand(0,"/xiui rings capture status",false),
                "capture and status commands share the render-thread request queue");
            host.Direct3DPresent(nullptr,nullptr,nullptr,nullptr);
            ComPtr<IDirect3DSurface8> back,depth,scene;
            ComPtr<IDirect3DTexture8> texture;
            Check(SUCCEEDED(device->GetRenderTarget(&back)) && SUCCEEDED(device->GetDepthStencilSurface(&depth)) &&
                SUCCEEDED(device->CreateTexture(64,64,1,D3DUSAGE_RENDERTARGET,D3DFMT_X8R8G8B8,D3DPOOL_DEFAULT,&texture)) &&
                SUCCEEDED(texture->GetSurfaceLevel(0,&scene)), "scene surfaces");
            D3DMATRIX view{}; view._11=view._22=view._33=view._44=1; view._41=10;
            D3DMATRIX projection{}; projection._11=projection._22=1;
            projection._33=-1.00000155f; projection._34=-1; projection._43=-0.100000158f;
            const auto world=[&] {
                Check(SUCCEEDED(device->SetVertexShader(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)), "fixed-function shader");
                device->SetTransform(D3DTS_VIEW,&view); device->SetTransform(D3DTS_PROJECTION,&projection);
                device->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE); device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
                device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
            };
            const auto present=[&] { host.Direct3DPresent(nullptr,nullptr,nullptr,nullptr); };
            world();
            host.CaptureSceneCamera();
            Check(host.scene_camera_valid_ && !host.scene_world_camera_valid_ && host.scene_depth_==depth &&
                host.scene_target_width_==64 && host.scene_target_height_==64 && host.scene_depth_mode_==D3DZB_TRUE,
                "fixed-function camera includes scene/depth diagnostics");
            host.DrawWorldOverlay();
            Check(!host.world_overlay_drawn_, "world geometry is not a submission boundary");
            device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            host.DrawWorldOverlay();
            Check(!host.world_overlay_drawn_, "early screen labels do not submit rings");
            host.DrawWorldOverlay(true);
            Check(host.world_overlay_drawn_ && host.scene_end_seen_, "fixed-function world submits at scene end");
            present();
            Check(!host.scene_camera_valid_ && !host.scene_target_ && host.ring_probe_.frames==1 &&
                host.ring_probe_.overlay_misses==0, "fixed-function frames are probed and camera expires after present");
            host.DrawWorldOverlay(true);
            Check(!host.world_overlay_drawn_, "a previous frame's camera cannot submit rings");

            Check(SUCCEEDED(device->SetRenderTarget(scene.Get(),depth.Get())), "offscreen scene target");
            world(); host.CaptureSceneCamera();
            device->SetTexture(0,nullptr);
            Check(SUCCEEDED(device->SetRenderTarget(back.Get(),depth.Get())), "restore back buffer for copy");
            host.DrawWorldOverlay(true);
            Check(!host.world_overlay_drawn_, "scene end on another surface does not submit rings");
            device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            host.DrawWorldOverlay();
            Check(!host.world_overlay_drawn_, "unrelated screen draws do not submit rings");
            device->SetTexture(0,texture.Get());
            host.DrawWorldOverlay();
            Check(host.world_overlay_drawn_, "fixed-function offscreen world submits before its scene copy");
            device->SetTexture(0,nullptr); present();

            const DWORD declaration[]{D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT3),D3DVSD_REG(3,D3DVSDT_FLOAT3),
                D3DVSD_REG(5,D3DVSDT_D3DCOLOR),D3DVSD_REG(7,D3DVSDT_FLOAT2),D3DVSD_REG(15,D3DVSDT_FLOAT3),D3DVSD_END()};
            // vs.1.1: mov oPos, v0
            const DWORD code[]{0xFFFE0101,D3DSIO_MOV,0xC00F0000,0x90E40000,D3DSIO_END};
            DWORD shader{};
            Check(SUCCEEDED(device->CreateVertexShader(declaration,code,&shader,0)), "terrain vertex shader");
            struct delete_shader { IDirect3DDevice8* device; DWORD shader; ~delete_shader() { device->DeleteVertexShader(shader); } } cleanup{device,shader};
            world(); host.CaptureSceneCamera();
            view._41=20; device->SetTransform(D3DTS_VIEW,&view); device->SetVertexShader(shader);
            host.CaptureSceneCamera();
            Check(host.scene_world_camera_valid_ && host.scene_view_._41==20, "terrain shader takes priority over fixed-function capture");
            view._41=30; world(); host.CaptureSceneCamera();
            Check(host.scene_view_._41==20, "later fixed-function draws retain the authoritative terrain camera");
            host.DrawWorldOverlay(true);
            Check(host.world_overlay_drawn_, "terrain shader path still submits");
            present();

            world(); device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE); host.CaptureSceneCamera();
            Check(!host.scene_camera_valid_, "transparent/non-depth-writing passes cannot provide the world camera");
            world(); view._41=0; device->SetTransform(D3DTS_VIEW,&view); host.CaptureSceneCamera();
            Check(!host.scene_camera_valid_, "rotation-only sky camera is rejected");
            const DWORD alternateDeclaration[]{D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT3),D3DVSD_END()};
            DWORD alternateShader{};
            Check(SUCCEEDED(device->CreateVertexShader(alternateDeclaration,code,&alternateShader,0)), "alternate shader layout");
            delete_shader alternateCleanup{device,alternateShader};
            view._41=10; world(); device->SetVertexShader(alternateShader);
            for (size_t i=0;i<25;++i) host.CaptureSceneCamera();
            Check(!host.scene_camera_valid_, "recording an unknown layout does not make it eligible for camera capture");
            DWORD currentShader{}; D3DMATRIX currentView{};
            device->GetVertexShader(&currentShader); device->GetTransform(D3DTS_VIEW,&currentView);
            Check(currentShader==alternateShader && currentView._41==view._41, "shader inspection preserves native device state");
            Check(host.HandleCommand(0,"/xiui rings capture stop",false), "manual stop command"); present();
            host.ring_capture_.Shutdown();
            const auto folder=captureRoot/"config"/"xiuinative"/"diagnostics";
            std::vector<std::filesystem::path> reports;
            for (const auto& entry : std::filesystem::directory_iterator(folder)) if (entry.path().extension()==".json") reports.push_back(entry.path());
            Check(reports.size()==1, "one completed report is atomically saved");
            std::ifstream file(reports.front(),std::ios::binary);
            const std::string report{std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
            for (const auto* expected : {"accepted fixed-function camera","accepted terrain camera","unrecognized vertex declaration",
                "depth writes disabled","rotation-only sky camera","\"terrain_layout_match\":true","\"draws\":25","manual stop"})
                Check(report.find(expected)!=std::string::npos,"report contains accepted/rejected layouts, reasons, grouped draws and manual completion");
            Check(report.find("\"declaration\":[\"0x20000000\",\"0x40020000\",\"0xFFFFFFFF\"]")!=std::string::npos,
                "unknown shader declaration words are retained");
            Check(report.find("\"function_fnv1a64\":\"0x0\"")==std::string::npos,
                "programmable shader bytecode is fingerprinted");
            host.HandleCommand(0,"/xiui rings capture start",false); present();
            host.CaptureSceneCamera();
            for (size_t i=0;i<600;++i) present();
            host.ring_capture_.Shutdown();
            size_t finished=0;
            for (const auto& entry : std::filesystem::directory_iterator(folder)) {
                Check(entry.path().extension()==".json", "no temporary files remain after writer drains"); ++finished;
            }
            Check(finished==2, "frame limit automatically saves a second capture");
            std::printf("Ring shader report fixtures: %s\n",folder.string().c_str());
            host.device_=nullptr;
        }
    };
}

int main()
{
    using Microsoft::WRL::ComPtr;
    const auto window=::CreateWindowExW(0,L"STATIC",L"XIUI world camera check",WS_POPUP,0,0,64,64,nullptr,nullptr,::GetModuleHandleW(nullptr),nullptr);
    struct close_window { HWND window; ~close_window() { if (window) ::DestroyWindow(window); } } cleanup{window};
    try
    {
        ComPtr<IDirect3D8> d3d; d3d.Attach(::Direct3DCreate8(D3D_SDK_VERSION));
        Check(window && d3d, "Direct3D initialization");
        D3DPRESENT_PARAMETERS parameters{};
        parameters.Windowed=TRUE; parameters.SwapEffect=D3DSWAPEFFECT_DISCARD; parameters.hDeviceWindow=window;
        parameters.BackBufferFormat=D3DFMT_X8R8G8B8; parameters.BackBufferWidth=parameters.BackBufferHeight=64;
        parameters.EnableAutoDepthStencil=TRUE; parameters.AutoDepthStencilFormat=D3DFMT_D16;
        ComPtr<IDirect3DDevice8> device;
        Check(SUCCEEDED(d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&device)), "Direct3D device");
        xiui::world_overlay_test::Run(device.Get());
        std::puts("Fixed-function/terrain camera, scene boundaries, shader priority, depth pairing and frame lifetime passed.");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::fprintf(stderr,"World overlay check failed: %s\n",error.what());
        return 1;
    }
}
