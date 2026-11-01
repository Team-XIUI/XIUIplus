#include "modules/target/target_module.hpp"
#include "modules/target/target_ring_draw_scope.hpp"
#include "world/entity_render_position.hpp"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <limits>

namespace
{
auto CheckRingRasterProjection(IDirect3DDevice8* device) -> bool
{
    using Microsoft::WRL::ComPtr;
    xiui::target_ring_draw_scope saved{device};
    ComPtr<IDirect3DSurface8> target,readback;
    D3DSURFACE_DESC desc{};
    if (!saved.Ready() || FAILED(device->GetRenderTarget(&target)) || FAILED(target->GetDesc(&desc)) ||
        FAILED(device->CreateImageSurface(desc.Width,desc.Height,desc.Format,&readback))) return false;
    D3DMATRIX identity{}; identity._11=identity._22=identity._33=identity._44=1;
    D3DMATRIX projection{}; projection._11=projection._22=1;
    projection._33=-1.00000155f; projection._34=-1; projection._43=-0.100000158f;
    device->SetTransform(D3DTS_WORLD,&identity); device->SetTransform(D3DTS_VIEW,&identity); device->SetTransform(D3DTS_PROJECTION,&projection);
    const D3DVIEWPORT8 viewport{0,0,desc.Width,desc.Height,0,1}; device->SetViewport(&viewport);
    for (DWORD stage=0;stage<8;++stage) device->SetTexture(stage,nullptr);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_FOGENABLE,FALSE);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); device->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE); device->SetRenderState(D3DRS_ZBIAS,0);
    const std::array native{xiui::target_ring_renderer::vertex{-0.7f,-0.7f,-2,0xFF00FF00},
        xiui::target_ring_renderer::vertex{0.7f,-0.7f,-3,0xFF00FF00},xiui::target_ring_renderer::vertex{0,0.7f,-2.5f,0xFF00FF00}};
    const auto capture=[&](bool cpu,float offset,bool equal,std::vector<bool>& mask,bool overwrite=false) {
        if (FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF000000,1,0)) || FAILED(device->BeginScene())) return false;
        device->SetPixelShader(0);
        device->SetRenderState(D3DRS_ZENABLE,equal ? D3DZB_TRUE : D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE,TRUE); device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        HRESULT result=S_OK;
        if (!cpu || equal) {
            auto source=native;
            if (equal) for (auto& v : source) v.color=0xFFFF0000;
            device->SetVertexShader(D3DFVF_XYZ|D3DFVF_DIFFUSE);
            result=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,source.data(),sizeof(source[0]));
        }
        if (cpu && SUCCEEDED(result)) {
            std::array<xiui::target_ring_renderer::screen_vertex,3> screen{};
            for (size_t i=0;i<screen.size();++i) {
                const double w=-native[i].z;
                screen[i]={static_cast<float>((native[i].x/w+1)*desc.Width*0.5+offset),
                    static_cast<float>((1-native[i].y/w)*desc.Height*0.5+offset),
                    static_cast<float>((native[i].z*projection._33+projection._43)/w),static_cast<float>(1/w),0xFF00FF00};
            }
            device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE); device->SetRenderState(D3DRS_ZFUNC,equal ? D3DCMP_EQUAL : D3DCMP_LESSEQUAL);
            device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE);
            if (overwrite) {
                std::vector<xiui::target_ring_renderer::screen_vertex> upload;
                upload.reserve(4095);
                for (size_t i=0;i<1365;++i) upload.insert(upload.end(),screen.begin(),screen.end());
                result=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1365,upload.data(),sizeof(screen[0]));
                std::fill(upload.begin(),upload.end(),xiui::target_ring_renderer::screen_vertex{-100000,-100000,1,1,0xFFFF0000});
            } else result=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,screen.data(),sizeof(screen[0]));
        }
        device->EndScene();
        if (FAILED(result) || FAILED(device->CopyRects(target.Get(),nullptr,0,readback.Get(),nullptr))) return false;
        D3DLOCKED_RECT lock{};
        if (FAILED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY))) return false;
        mask.clear(); mask.reserve(desc.Width*desc.Height);
        for (UINT y=0;y<desc.Height;++y) for (UINT x=0;x<desc.Width;++x)
            mask.push_back((reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(lock.pBits)+y*lock.Pitch)[x]&0xFFFFFF)==0x00FF00);
        readback->UnlockRect(); return true;
    };
    std::vector<bool> baseline,mask;
    if (!capture(false,0,false,baseline)) return false;
    const auto count=[](const auto& pixels) { return std::count(pixels.begin(),pixels.end(),true); };
    std::printf("Ring raster native XYZ pixels: %lld.\n",static_cast<long long>(count(baseline)));
    for (const float offset : {0.0f,-0.5f,0.5f}) {
        if (!capture(true,offset,false,mask)) return false;
        size_t mismatch=0;
        for (size_t i=0;i<mask.size();++i) mismatch+=mask[i]!=baseline[i];
        if (!capture(true,offset,true,mask)) return false;
        std::printf("Ring raster XYZRHW offset %+.1f: coverage mismatch %zu, equal-depth pixels %lld.\n",offset,mismatch,static_cast<long long>(count(mask)));
    }
    if (!capture(true,0,false,mask,true)) return false;
    const auto overwritten=count(mask);
    std::printf("Ring raster 4,095 UP vertices, immediate overwrite/free: %lld/%lld pixels retained.\n",
        static_cast<long long>(overwritten),static_cast<long long>(count(baseline)));
    if (mask!=baseline) return false;
    return count(baseline)>0;
}
}

auto RunTargetRingStateChecks(void) -> bool
{
    using Microsoft::WRL::ComPtr;
    const auto window=::CreateWindowExW(0,L"STATIC",L"XIUI ring state check",WS_POPUP,0,0,64,64,nullptr,nullptr,::GetModuleHandleW(nullptr),nullptr);
    struct close_window { HWND window; ~close_window() { if (window) ::DestroyWindow(window); } } close{window};
    wchar_t wrapperPath[MAX_PATH]{};
    const auto wrapperLength=::GetEnvironmentVariableW(L"XIUI_RING_RASTER_WRAPPER",wrapperPath,MAX_PATH);
    const auto wrapper=wrapperLength && wrapperLength<MAX_PATH ? ::LoadLibraryExW(wrapperPath,nullptr,LOAD_WITH_ALTERED_SEARCH_PATH) : nullptr;
    struct close_library { HMODULE module; ~close_library() { if (module) ::FreeLibrary(module); } } closeWrapper{wrapper};
    using create_d3d=IDirect3D8* (WINAPI*)(UINT);
    const auto create=wrapper ? reinterpret_cast<create_d3d>(::GetProcAddress(wrapper,"Direct3DCreate8")) : &::Direct3DCreate8;
    if ((wrapperLength && !wrapper) || !create) { std::fputs("Ring raster wrapper could not load.\n",stderr); return false; }
    ComPtr<IDirect3D8> d3d; d3d.Attach(create(D3D_SDK_VERSION));
    if (!window || !d3d) return false;
    D3DPRESENT_PARAMETERS params{};
    D3DDISPLAYMODE display{}; d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&display);
    params.BackBufferFormat=display.Format;
    params.Windowed=TRUE; params.SwapEffect=D3DSWAPEFFECT_DISCARD; params.hDeviceWindow=window;
    params.BackBufferWidth=params.BackBufferHeight=64; params.EnableAutoDepthStencil=TRUE;
    params.AutoDepthStencilFormat=wrapper ? D3DFMT_D24S8 : D3DFMT_D16;
    ComPtr<IDirect3DDevice8> device;
    const auto created=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&params,&device);
    if (FAILED(created)) { std::fprintf(stderr,"Ring state check device creation: %08lX\n",static_cast<unsigned long>(created)); return false; }
    struct vertex { float x,y,z,rhw; D3DCOLOR color; };
    const vertex triangle[]{{0,0,0.5f,1,0xFFFFFFFF},{32,0,0.5f,1,0xFFFFFFFF},{0,32,0.5f,1,0xFFFFFFFF}};
    constexpr DWORD fvf=D3DFVF_XYZRHW|D3DFVF_DIFFUSE;
    ComPtr<IDirect3DVertexBuffer8> stream;
    ComPtr<IDirect3DIndexBuffer8> indices;
    ComPtr<IDirect3DSurface8> original,depth,scene,sceneDepth;
    ComPtr<IDirect3DTexture8> originalTexture,replacementTexture;
    if (FAILED(device->CreateVertexBuffer(sizeof(triangle),0,fvf,D3DPOOL_MANAGED,&stream)) ||
        FAILED(device->CreateIndexBuffer(6,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&indices)) ||
        FAILED(device->CreateRenderTarget(32,32,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,FALSE,&scene)) ||
        FAILED(device->CreateDepthStencilSurface(32,32,D3DFMT_D16,D3DMULTISAMPLE_NONE,&sceneDepth)) ||
        FAILED(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&originalTexture)) ||
        FAILED(device->CreateTexture(2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&replacementTexture))) { std::fputs("Ring state check resources unavailable.\n",stderr); return false; }
    BYTE* data{};
    stream->Lock(0,0,&data,0); std::memcpy(data,triangle,sizeof(triangle)); stream->Unlock();
    const uint16_t order[]{0,1,2};
    indices->Lock(0,0,&data,0); std::memcpy(data,order,sizeof(order)); indices->Unlock();
    device->GetRenderTarget(&original); device->GetDepthStencilSurface(&depth);
    device->SetTexture(3,originalTexture.Get());
    device->SetStreamSource(0,stream.Get(),sizeof(vertex)); device->SetIndices(indices.Get(),0); device->SetVertexShader(fvf);
    D3DMATRIX transform{}; transform._11=transform._22=transform._33=transform._44=1; transform._41=7;
    device->SetTransform(D3DTS_WORLD,&transform);
    device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    if (FAILED(device->BeginScene())) return false;
    bool okay=true;
    {
        xiui::target_ring_draw_scope scope{device.Get()};
        okay=scope.Ready() && scope.Matches(original.Get(),depth.Get()) && !scope.Matches(scene.Get(),sceneDepth.Get());
        device->SetRenderTarget(scene.Get(),sceneDepth.Get());
        device->SetTexture(3,replacementTexture.Get());
        auto changed=transform; changed._41=99; device->SetTransform(D3DTS_WORLD,&changed);
        device->SetIndices(nullptr,0);
        okay &= SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,triangle,sizeof(vertex)));
    }
    ComPtr<IDirect3DVertexBuffer8> actualStream;
    ComPtr<IDirect3DIndexBuffer8> actualIndices;
    ComPtr<IDirect3DSurface8> actualTarget,actualDepth;
    ComPtr<IDirect3DBaseTexture8> actualTexture;
    UINT stride{},base{}; DWORD actualFvf{}; D3DMATRIX actualTransform{}; D3DVIEWPORT8 viewport{};
    device->GetStreamSource(0,&actualStream,&stride); device->GetIndices(&actualIndices,&base);
    device->GetRenderTarget(&actualTarget); device->GetDepthStencilSurface(&actualDepth);
    device->GetTexture(3,&actualTexture);
    device->GetVertexShader(&actualFvf); device->GetTransform(D3DTS_WORLD,&actualTransform); device->GetViewport(&viewport);
    okay &= actualStream==stream && stride==sizeof(vertex) && actualIndices==indices && base==0 &&
        actualTarget==original && actualDepth==depth && actualTexture.Get()==originalTexture.Get() &&
        actualFvf==fvf && actualTransform._41==7 && viewport.Width==64 && viewport.Height==64;
    okay &= SUCCEEDED(device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,3,0,1));
    device->EndScene();
    if (okay && wrapper) okay=CheckRingRasterProjection(device.Get());
    if (!okay) std::fputs("Target ring device restoration failed.\n",stderr);
    else std::puts("Target ring real-device native buffer, target/depth, texture stage 3, viewport, transform and resumed indexed-draw checks passed.");
    return okay;
}

auto RunTargetRingChecks(const char* client_root) -> bool
{
    using namespace xiui;
    const auto check=[](bool okay, const char* message) {
        if (!okay) std::fprintf(stderr,"target ring check failed: %s\n",message);
        return okay;
    };
    {
        alignas(uintptr_t) std::array<uint8_t, 0x700> actor{};
        Ashita::FFXI::entity_t entity{};
        std::array<uintptr_t, 0x1C0 / sizeof(uintptr_t)> table{};
        std::array<uint8_t, 7> getter{0x8D, 0x81, 0xFC, 0x05, 0, 0, 0xC3};
        const auto actorAddress = reinterpret_cast<uintptr_t>(actor.data());
        const auto entityAddress = reinterpret_cast<uintptr_t>(&entity);
        const auto getterAddress = reinterpret_cast<uintptr_t>(getter.data());
        const auto tableAddress = reinterpret_cast<uintptr_t>(table.data());
        std::memcpy(actor.data(), &tableAddress, sizeof(tableAddress));
        std::memcpy(actor.data() + 0x70, &entityAddress, sizeof(entityAddress));
        entity.ActorPointer = actorAddress;
        table[0x1BC / sizeof(uintptr_t)] = getterAddress;
        entity.Movement.LocalPosition = {12, 3, 24};
        ground_point rendered{11.5f, 2.9f, 23.5f}, position{};
        std::memcpy(actor.data() + 0x5FC, rendered.data(), sizeof(rendered));
        if (!check(ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress, getter.size(), position) && position == rendered,
            "ring anchor follows the model draw position when the movement anchor runs ahead")) return false;
        entity.Movement.LocalPosition.X = 14;
        rendered[0] = 12.25f;
        std::memcpy(actor.data() + 0x5FC, rendered.data(), sizeof(rendered));
        if (!check(ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress, getter.size(), position) && position == rendered,
            "moving models use their current interpolated anchor without velocity prediction or retained lag")) return false;
        entity.ActorPointer = 0;
        if (!check(!ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress, getter.size(), position),
            "replaced actor ownership cannot supply a ring anchor")) return false;
        entity.ActorPointer = actorAddress;
        getter[0] = 0xE9;
        if (!check(!ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress, getter.size(), position),
            "unrecognized native position getters fail without guessing an actor offset")) return false;
        getter[0] = 0x8D;
        rendered[0] = std::numeric_limits<float>::quiet_NaN();
        std::memcpy(actor.data() + 0x5FC, rendered.data(), sizeof(rendered));
        if (!check(!ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress, getter.size(), position) &&
            !ReadEntityRenderPosition(actorAddress, entityAddress, getterAddress + 8, getter.size(), position),
            "invalid model coordinates and getters outside the native client are unavailable")) return false;
    }
    zone_ground slope;
    const auto point=[](float x,float z) -> ground_point { return {x,x*0.3f+z*0.1f,z}; };
    slope.Add({point(-5,-5),point(5,-5),point(5,5)});
    slope.Add({point(-5,-5),point(5,5),point(-5,5)});
    target_ring_settings settings;
    if (!check(settings.follow_name_color && settings.scale_with_model && !settings.show_npcs && !settings.show_players &&
        settings.Allows(0x10) && !settings.Allows(0x02) && !settings.Allows(0x01) && !settings.Allows(0),
        "name colors default on and optional entity types stay off")) return false;
    settings.show_npcs=settings.show_players=true;
    if (!check(settings.Allows(0x02) && settings.Allows(0x110) && settings.Allows(0x01) && !settings.Allows(0),
        "NPC and player switches include their native entity types without unrelated objects")) return false;
    for (D3DCOLOR name : std::array<D3DCOLOR,5>{unclaimed_name_color,group_claim_name_color,other_claim_name_color,0xFFC2FFC2u,0xFFFFFFFFu})
        if (!check(settings.Color(name)==((settings.color&0xFF000000)|(name&0xFFFFFF)),
            "ring follows claim/player/NPC name colors while retaining its opacity")) return false;
    settings.follow_name_color=false;
    if (!check(settings.Color(group_claim_name_color)==settings.color,"manual ring color overrides name coloring when disabled")) return false;
    std::vector<target_ring_renderer::vertex> vertices;
    target_ring_renderer::Build(slope,{0,0,0},0.5f,settings,vertices);
    if (!check(!vertices.empty() && vertices.size()%3==0,"sloped floor produces complete triangles")) return false;
    for (const auto& v : vertices)
        if (!check(std::abs(v.y-(v.x*0.3f+v.z*0.1f-0.018f))<0.0001f,"every vertex follows the actual slope")) return false;
    const auto half=vertices.size();
    target_ring_renderer::Build(slope,{0,0,0},1,settings,vertices);
    if (!check(vertices.size()>half,"native HP percentage controls the filled arc")) return false;
    const auto full=vertices.size();
    for (const uint32_t flags : {0x100u, 0x104u, 0x108u, 0x110u})
    {
        const float petHp = EntityHasHealth(flags) ? 0.25f : 1.0f;
        target_ring_renderer::Build(slope,{0,0,0},petHp,settings,vertices);
        if (!check(vertices.size() < full, "damaged allied pets use their HP arc rather than a full decorative ring")) return false;
    }
    for (float yaw : {0.0f,1.570796327f,3.141592654f,-1.570796327f})
    {
        target_ring_renderer::Build(slope,{0,0,0},1,settings,vertices,yaw);
        const auto tip=std::max_element(vertices.begin(),vertices.end(),[](const auto& a,const auto& b) {
            return a.x*a.x+a.z*a.z<b.x*b.x+b.z*b.z;
        });
        const float radius=settings.outer_radius+0.23f;
        if (!check(tip!=vertices.end() && std::abs(tip->x-radius*std::cos(yaw))<0.001f &&
            std::abs(tip->z+radius*std::sin(yaw))<0.001f,"triangle marks native facing at each cardinal turn")) return false;
    }
    {
        zone_ground wide;
        wide.Add({ground_point{-50,0,-50},ground_point{50,0,-50},ground_point{50,0,50}});
        wide.Add({ground_point{-50,0,-50},ground_point{50,0,50},ground_point{-50,0,50}});
        const auto extent=[&](const target_ring_settings& appearance,float hitbox) {
            target_ring_renderer::Build(wide,{0,0,0},1,appearance,vertices,0,hitbox);
            float radius=0;
            for (const auto& v : vertices) radius=std::max(radius,std::hypot(v.x,v.z));
            return radius;
        };
        for (float hitbox : {0.1f,0.6f,1.0f,4.2f,25.5f}) {
            if (!check(std::abs(extent(settings,hitbox)-(settings.outer_radius+0.23f)*hitbox)<0.001f,
                "small and dragon-sized hitboxes scale the ring and facing marker together")) return false;
            bool inner=false,outer=false;
            for (const auto& v : vertices) {
                const float radius=std::hypot(v.x,v.z);
                inner |= std::abs(radius-settings.inner_radius*hitbox)<0.001f;
                outer |= std::abs(radius-settings.outer_radius*hitbox)<0.001f;
            }
            if (!check(inner && outer,"both configured band radii follow the native model footprint")) return false;
        }
        for (float invalid : {0.0f,-1.0f,26.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
            if (!check(std::abs(extent(settings,invalid)-(settings.outer_radius+0.23f))<0.001f,
                "unavailable or invalid hitboxes retain the one-yalm fallback")) return false;
        auto fixed=settings; fixed.scale_with_model=false;
        if (!check(std::abs(extent(fixed,4.2f)-(settings.outer_radius+0.23f))<0.001f,
            "fixed radius mode ignores target model size")) return false;
    }
    meter_animation_state hp;
    if (!check(hp.Update(1,1000)==1,"first HP sample initializes immediately")) return false;
    const float damaged=hp.Update(0.25f,1050);
    const float healing=hp.Update(1,1100);
    if (!check(damaged>0.25f && damaged<1 && healing>damaged && healing<1,"damage and healing both interpolate without overshoot")) return false;
    hp={};
    if (!check(hp.Update(0.4f,1200)==0.4f,"target changes can reset HP without inheriting the previous fill")) return false;
    meter_animation_state coarse,fine;
    coarse.Update(1,1000); fine.Update(1,1000);
    coarse.Update(0.2f,1100);
    for (uint64_t tick=1010;tick<=1100;tick+=10) fine.Update(0.2f,tick);
    if (!check(std::abs(coarse.displayed_ratio-fine.displayed_ratio)<0.00001f,"HP interpolation is independent of frame rate")) return false;
    settings.hp_fill=false;
    target_ring_renderer::Build(slope,{0,0,0},0.25f,settings,vertices);
    if (!check(vertices.size()==full,"decorative mode remains a complete ring")) return false;

    zone_ground steps;
    const auto floor=[&](float x0,float x1,float y) {
        steps.Add({ground_point{x0,y,-5},ground_point{x1,y,-5},ground_point{x1,y,5}});
        steps.Add({ground_point{x0,y,-5},ground_point{x1,y,5},ground_point{x0,y,5}});
    };
    floor(-5,0,0); floor(0,5,-0.4f); floor(-5,5,2);
    target_ring_renderer::Build(steps,{0,0,0},1,settings,vertices);
    bool lower=false,upper=false;
    for (size_t i=0;i<vertices.size();i+=3)
    {
        const float y=vertices[i].y;
        lower |= std::abs(y+0.018f)<0.001f; upper |= std::abs(y+0.418f)<0.001f;
        for (unsigned v=0;v<3;++v)
            if (!check(std::abs(vertices[i+v].y-y)<0.001f && y<0,"steps stay separate, without sloped bridges or lower-floor leakage")) return false;
    }
    if (!check(lower && upper,"both actual step levels receive the ring")) return false;
    zone_ground missing;
    target_ring_renderer::Build(missing,{0,0,0},1,settings,vertices);
    if (!check(vertices.empty(),"missing terrain draws no fabricated flat floor")) return false;

    hud_snapshot camera{};
    camera.scene_view._11=camera.scene_view._22=camera.scene_view._33=camera.scene_view._44=1;
    camera.scene_view._41=510;
    camera.scene_projection._11=camera.scene_projection._22=camera.scene_projection._34=1;
    camera.scene_projection._33=0.99f; camera.scene_projection._43=-0.1f;
    camera.scene_viewport={0,0,1000,600,0,1};
    std::vector<target_ring_renderer::screen_vertex> screen;
    float previous{};
    for (unsigned frame=0;frame<32;++frame)
    {
        const float x=-510.0f+frame*0.01f;
        const std::array moving{target_ring_renderer::vertex{x,-0.05f,1,0xFFFFFFFF},
            target_ring_renderer::vertex{x+0.05f,-0.05f,1,0xFFFFFFFF},target_ring_renderer::vertex{x,0.05f,1,0xFFFFFFFF}};
        target_ring_renderer::Project(moving,camera,screen);
        if (!check(screen.size()==3 && (frame==0 || (screen[0].x>previous && screen[0].x-previous<5.1f)),"moving world vertices project continuously without previous device transforms")) return false;
        previous=screen[0].x;
    }
    const std::array crossing{target_ring_renderer::vertex{-510.05f,-0.05f,1,0xFFFFFFFF},
        target_ring_renderer::vertex{-509.95f,-0.05f,1,0xFFFFFFFF},target_ring_renderer::vertex{-510,0.05f,-1,0xFFFFFFFF}};
    target_ring_renderer::Project(crossing,camera,screen);
    if (!check(!screen.empty() && screen.size()%3==0,"near-plane crossing is clipped into complete triangles")) return false;
    for (const auto& v : screen)
        if (!check(std::isfinite(v.x) && std::isfinite(v.y) && v.x>=-0.01f && v.x<=1000.01f && v.y>=-0.01f && v.y<=600.01f && v.z>=-0.001f && v.z<=1.001f && v.rhw>0,"camera-edge clipping cannot emit projection spikes")) return false;

    auto decal_camera=camera;
    decal_camera.scene_projection._33=1000.0f/999.9f;
    decal_camera.scene_projection._43=-100.0f/999.9f;
    decal_camera.scene_viewport={20,10,1280,720,0,1};
    const double projection_a=decal_camera.scene_projection._33, projection_b=decal_camera.scene_projection._43;
    const auto depth_at=[&](double distance) { return projection_a+projection_b/distance; };
    for (const float distance : {0.15f,2.0f,10.0f,100.0f,400.0f})
    {
        const std::array steep{target_ring_renderer::vertex{-510-distance*0.001f,-distance*0.0002f,distance,0xFFFFFFFF},
            target_ring_renderer::vertex{-510+distance*0.001f,-distance*0.0002f,distance*1.4f,0xFFFFFFFF},
            target_ring_renderer::vertex{-510,distance*0.0002f,distance*1.2f,0xFFFFFFFF}};
        target_ring_renderer::Project(steep,decal_camera,screen);
        if (!check(screen.size()==3,"steep decal remains a complete projected triangle")) return false;
        bool toward_camera=false;
        for (size_t i=0;i<screen.size();++i)
        {
            const auto& actual=screen[i];
            const float original=static_cast<float>(depth_at(steep[i].z));
            const float nearest=std::nextafter(static_cast<float>(depth_at(steep[i].z-0.02)),
                -std::numeric_limits<float>::infinity());
            toward_camera |= actual.z<original;
            if (!check(actual.z<=original && actual.z>=nearest && actual.z>=0 && actual.z<=1,
                "slope bias moves toward the camera within two centimeters, allowing float depth rounding")) return false;
            const float expected_x=20+(steep[i].x+510.0f)/steep[i].z*640+640;
            const float expected_y=10-steep[i].y/steep[i].z*360+360;
            if (!check(std::abs(actual.x-expected_x)<0.001f && std::abs(actual.y-expected_y)<0.001f,
                "decal bias preserves the projected floor position")) return false;
        }
        if (!check(distance>10 || toward_camera,"nearby slopes receive a representable depth offset")) return false;
    }
    std::array<target_ring_renderer::screen_vertex,3> last_decal{};
    for (unsigned frame=0;frame<32;++frame)
    {
        const float x=-510.0f+frame*0.001f;
        const std::array moving{target_ring_renderer::vertex{x-0.5f,-0.2f,10,0xFFFFFFFF},
            target_ring_renderer::vertex{x+0.5f,-0.2f,10.25f,0xFFFFFFFF},target_ring_renderer::vertex{x,0.2f,9.75f,0xFFFFFFFF}};
        target_ring_renderer::Project(moving,decal_camera,screen);
        if (!check(screen.size()==3,"moving biased slope retains all vertices")) return false;
        for (size_t i=0;i<screen.size();++i)
        {
            if (frame && !check(screen[i].x>last_decal[i].x && screen[i].x-last_decal[i].x<0.08f &&
                std::abs(screen[i].z-last_decal[i].z)<0.000001f,"biased slope movement remains continuous in screen position and depth")) return false;
            last_decal[i]=screen[i];
        }
    }
    const float far_distance=static_cast<float>(-projection_b/(projection_a-1));
    for (const auto distances : {std::array{0.05f,0.15f,0.15f},
        std::array{far_distance*0.999f,far_distance*1.001f,far_distance*0.999f}})
    {
        const std::array clipped{target_ring_renderer::vertex{-510.01f,-0.01f,distances[0],0xFFFFFFFF},
            target_ring_renderer::vertex{-509.99f,-0.01f,distances[1],0xFFFFFFFF},target_ring_renderer::vertex{-510,0.01f,distances[2],0xFFFFFFFF}};
        target_ring_renderer::Project(clipped,decal_camera,screen);
        if (!check(!screen.empty() && screen.size()%3==0,"biased near/far intersections produce complete clipped triangles")) return false;
        for (const auto& v : screen)
            if (!check(std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::isfinite(v.rhw) &&
                v.z>=0 && v.z<=1 && v.rhw>0,"biased near/far intersections retain finite legal depth")) return false;
    }

    target_module target;
    target.SelectConfigView(4);
    if (!check(std::string_view{target.DisplayName()}=="Target" && std::string_view{target.ConfigViews()[4]}=="Target Rings","category and ring view names")) return false;
    size_t shown=0;
    for (size_t s=0;s<target.ConfigSections().size();++s)
        for (size_t i=0;i<target.OptionCount();++i) if (target.ConfigSectionContains(s,i)) ++shown;
    if (!check(shown==target_ring_settings::option_count,"each ring control appears once in its own group")) return false;
    settings.inner_radius=2; settings.outer_radius=1;
    settings.Changed(2);
    if (!check(settings.outer_radius>settings.inner_radius,"editing inner radius retains positive band width")) return false;
    settings.outer_radius=0.1f; settings.Changed(3);
    if (!check(settings.inner_radius>=0.049f && settings.inner_radius<settings.outer_radius,"editing outer radius retains a valid center")) return false;
    const auto path=std::filesystem::temp_directory_path()/("xiui-ring-check-"+std::to_string(::GetCurrentProcessId())+".ini");
    struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path,ignored); } } remove{path};
    settings_store store{path.string()};
    target_ring_settings existing; existing.scale_with_model=false; existing.Load(store);
    if (!check(existing.scale_with_model,"existing profiles without the new key enable model-relative sizing")) return false;
    settings.enabled=false; settings.color=0xA0123456; settings.scale_with_model=false;
    settings.Save(store);
    target_ring_settings restored; restored.Load(store);
    if (!check(!restored.enabled && !restored.hp_fill && !restored.follow_name_color && !restored.scale_with_model && restored.show_npcs && restored.show_players &&
        restored.color==settings.color && std::abs(restored.inner_radius-settings.inner_radius)<0.0001f,"all ring preferences persist, including entity filters and color mode")) return false;
    if (client_root)
    {
        std::atomic_bool cancel{false};
        const auto terrain=LoadZoneGround(104,client_root,cancel);
        if (!check(terrain && !terrain->triangles.empty(),"current Jugner Forest DAT decodes native terrain")) return false;
        target_ring_renderer::Build(*terrain,{-484.118f,8.86762f,297.376f},1,settings,vertices);
        if (!check(!vertices.empty(),"current native player floor supports the projected ring")) return false;
        std::printf("Jugner Forest: %zu floor triangles decoded; %zu ring vertices at current native position.\n",terrain->triangles.size(),vertices.size());
    }
    std::puts("Target ring model sizing, slope/step projection, HP fill, missing terrain, config routing and persistence passed.");
    return true;
}
