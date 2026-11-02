#include "ring_shader_capture.hpp"
#include "settings/storage_paths.hpp"

#include <Windows.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <future>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>

namespace xiui
{
    namespace
    {
        constexpr uint64_t capture_ms=3000;
        constexpr size_t max_shaders=128, max_samples=512, max_frames=600, max_declaration_words=256;
        auto ResultName(const ring_camera_result result) -> const char*
        {
            constexpr const char* names[]{"state unavailable","depth disabled","depth writes disabled","alpha blended",
                "unrecognized vertex declaration","unrecognized fixed-function format","render target unavailable",
                "retaining terrain camera","fixed-function draw on another target","projection unavailable",
                "projection is not perspective","view or viewport unavailable","rotation-only sky camera",
                "accepted fixed-function camera","accepted terrain camera"};
            return names[static_cast<size_t>(result)];
        }
        auto Json(std::ostream& out, const std::string_view text) -> void
        {
            out << '"';
            for (const unsigned char c : text)
                if (c=='"' || c=='\\') out << '\\' << c;
                else if (c<32) { char escaped[7]{}; std::snprintf(escaped,sizeof(escaped),"\\u%04x",c); out << escaped; }
                else out << c;
            out << '"';
        }
        auto Hex(const uint64_t value) -> std::string
        { std::ostringstream out; out << "0x" << std::hex << std::uppercase << value; return out.str(); }
        auto Matrix(std::ostream& out, const D3DMATRIX& value) -> void
        {
            out << '[';
            for (size_t i=0;i<16;++i) {
                if (i) out << ',';
                if (std::isfinite(value.m[i/4][i%4])) out << value.m[i/4][i%4]; else out << "null";
            }
            out << ']';
        }
        auto Surface(std::ostream& out, const uintptr_t id, const D3DSURFACE_DESC& desc) -> void
        { out << "{\"id\":\"" << Hex(id) << "\",\"width\":" << desc.Width << ",\"height\":" << desc.Height << ",\"format\":" << desc.Format << '}'; }
        struct shader_record
        {
            DWORD handle{}, declaration_bytes{}, function_bytes{};
            HRESULT declaration_result{S_FALSE}, function_result{S_FALSE};
            std::vector<DWORD> declaration;
            uint64_t function_hash{}, draws{};
        };
        struct draw_record
        {
            size_t shader{};
            ring_camera_result result{};
            DWORD z{}, write{}, blend{};
            HRESULT state_result{S_OK};
            uintptr_t target{}, depth{};
            D3DSURFACE_DESC target_desc{}, depth_desc{};
            D3DVIEWPORT8 viewport{};
            D3DMATRIX world{}, view{}, projection{};
            uint64_t draws{};
        };
        struct report
        {
            std::string version, build, adapter, driver, started, finish, boundary_hook;
            uint32_t vendor{}, device{};
            uint64_t duration{}, frames{}, draws{}, dropped{}, read_errors{}, camera_terrain{}, camera_fixed{}, camera_missing{}, overlays{};
            std::vector<shader_record> shaders;
            std::vector<draw_record> samples;
        };
        auto Save(const report& data, const std::filesystem::path& path) -> std::string
        {
            auto temporary=path; temporary += L".tmp";
            try
            {
                std::filesystem::create_directories(path.parent_path());
                std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
                out.exceptions(std::ios::badbit|std::ios::failbit); out.imbue(std::locale::classic()); out << std::setprecision(9);
                out << "{\n\"format\":\"xiui-ring-shaders-v1\",\n\"version\":"; Json(out,data.version);
                out << ",\n\"host_build\":"; Json(out,data.build); out << ",\n\"started_utc\":"; Json(out,data.started);
                out << ",\n\"finish\":"; Json(out,data.finish); out << ",\n\"adapter\":"; Json(out,data.adapter);
                out << ",\n\"driver\":"; Json(out,data.driver); out << ",\n\"vendor_id\":" << data.vendor << ",\n\"device_id\":" << data.device;
                out << ",\n\"boundary_hook\":"; Json(out,data.boundary_hook);
                out << ",\n\"duration_ms\":" << data.duration << ",\n\"frames\":" << data.frames << ",\n\"draws\":" << data.draws;
                out << ",\n\"dropped_draws\":" << data.dropped << ",\n\"read_errors\":" << data.read_errors;
                out << ",\n\"limits\":{\"shaders\":" << max_shaders << ",\"samples\":" << max_samples << ",\"declaration_words\":" << max_declaration_words << "}";
                out << ",\n\"frame_results\":{\"terrain_camera\":" << data.camera_terrain << ",\"fixed_camera\":" << data.camera_fixed
                    << ",\"missing_camera\":" << data.camera_missing << ",\"overlay_drawn\":" << data.overlays << "},\n\"shaders\":[";
                for (size_t i=0;i<data.shaders.size();++i) {
                    const auto& s=data.shaders[i]; if (i) out << ',';
                    out << "\n{\"id\":" << i << ",\"handle_or_fvf\":\"" << Hex(s.handle) << "\",\"programmable\":" << ((s.handle&1) ? "true" : "false")
                        << ",\"draws\":" << s.draws << ",\"declaration_result\":\"" << Hex(static_cast<uint32_t>(s.declaration_result))
                        << "\",\"declaration_bytes\":" << s.declaration_bytes << ",\"terrain_layout_match\":" << (IsTerrainVertexDeclaration(s.declaration) ? "true" : "false")
                        << ",\"function_result\":\"" << Hex(static_cast<uint32_t>(s.function_result)) << "\",\"function_bytes\":" << s.function_bytes
                        << ",\"function_fnv1a64\":";
                    if (s.function_bytes && s.function_bytes<=65536 && SUCCEEDED(s.function_result)) out << '"' << Hex(s.function_hash) << '"'; else out << "null";
                    out << ",\"declaration_truncated\":" << (s.declaration_bytes>max_declaration_words*sizeof(DWORD) ? "true" : "false") << ",\"declaration\":[";
                    for (size_t w=0;w<s.declaration.size();++w) { if (w) out << ','; out << '"' << Hex(s.declaration[w]) << '"'; }
                    out << "]}";
                }
                out << "\n],\n\"draw_groups\":[";
                for (size_t i=0;i<data.samples.size();++i) {
                    const auto& s=data.samples[i]; if (i) out << ',';
                    out << "\n{\"shader_id\":" << s.shader << ",\"camera_check\":"; Json(out,ResultName(s.result));
                    out << ",\"draws\":" << s.draws << ",\"state_result\":\"" << Hex(static_cast<uint32_t>(s.state_result))
                        << "\",\"depth_enable\":" << s.z << ",\"depth_write\":" << s.write << ",\"alpha_blend\":" << s.blend << ",\"target\":";
                    Surface(out,s.target,s.target_desc); out << ",\"depth\":"; Surface(out,s.depth,s.depth_desc);
                    out << ",\"viewport\":[" << s.viewport.X << ',' << s.viewport.Y << ',' << s.viewport.Width << ',' << s.viewport.Height << ']';
                    out << ",\"world_sample\":"; Matrix(out,s.world); out << ",\"view_sample\":"; Matrix(out,s.view);
                    out << ",\"projection_sample\":"; Matrix(out,s.projection); out << '}';
                }
                out << "\n]\n}\n"; out.close(); std::filesystem::rename(temporary,path);
                return "[XIUI] Ring shader capture saved: " + path.string();
            }
            catch (const std::exception& error)
            {
                std::error_code ignored; std::filesystem::remove(temporary,ignored);
                return std::string{"[XIUI] Ring shader capture could not save: "}+error.what();
            }
        }
    }

    struct ring_shader_capture::state
    {
        std::atomic<unsigned> requests{};
        std::filesystem::path directory, path;
        std::string version, build, last_result;
        bool active{};
        uint64_t start{}, sequence{};
        report data;
        std::future<std::string> writer;
    };

    ring_shader_capture::ring_shader_capture() : state_(std::make_unique<state>()) {}
    ring_shader_capture::~ring_shader_capture() { this->Shutdown(); }
    ring_shader_capture::draw_scope::~draw_scope() { if (capture) capture->Record(device,result); }
    auto ring_shader_capture::Initialize(const std::filesystem::path& install, const char* version, const char* build) -> void
    { state_->directory=storage_paths{install/"config"/"xiuinative"}.Diagnostics(); state_->version=version; state_->build=build; }
    auto ring_shader_capture::Request(const request value) noexcept -> void { state_->requests.fetch_or(static_cast<unsigned>(value),std::memory_order_release); }
    auto ring_shader_capture::Trace(IDirect3DDevice8* device) noexcept -> draw_scope { return {state_->active ? this : nullptr,device}; }

    auto ring_shader_capture::Record(IDirect3DDevice8* device, const ring_camera_result result) noexcept -> void
    {
        auto& data=state_->data; ++data.draws;
        try
        {
            if (!device) { ++data.read_errors; return; }
            DWORD handle{}; if (FAILED(device->GetVertexShader(&handle))) { ++data.read_errors; return; }
            auto shader=std::find_if(data.shaders.begin(),data.shaders.end(),[&](const auto& s) { return s.handle==handle; });
            if (shader==data.shaders.end()) {
                if (data.shaders.size()==max_shaders) { ++data.dropped; return; }
                shader_record entry{}; entry.handle=handle;
                if (handle&1) {
                    entry.declaration_result=device->GetVertexShaderDeclaration(handle,nullptr,&entry.declaration_bytes);
                    if (SUCCEEDED(entry.declaration_result) && entry.declaration_bytes && entry.declaration_bytes<=max_declaration_words*sizeof(DWORD) && entry.declaration_bytes%sizeof(DWORD)==0) {
                        entry.declaration.resize(entry.declaration_bytes/sizeof(DWORD)); DWORD bytes=entry.declaration_bytes;
                        entry.declaration_result=device->GetVertexShaderDeclaration(handle,entry.declaration.data(),&bytes);
                        if (FAILED(entry.declaration_result)) entry.declaration.clear();
                    }
                    entry.function_result=device->GetVertexShaderFunction(handle,nullptr,&entry.function_bytes);
                    if (SUCCEEDED(entry.function_result) && entry.function_bytes && entry.function_bytes<=65536) {
                        std::vector<uint8_t> code(entry.function_bytes); DWORD bytes=entry.function_bytes;
                        entry.function_result=device->GetVertexShaderFunction(handle,code.data(),&bytes);
                        if (SUCCEEDED(entry.function_result) && bytes<=code.size()) {
                            entry.function_hash=14695981039346656037ull;
                            for (size_t i=0;i<bytes;++i) { entry.function_hash^=code[i]; entry.function_hash*=1099511628211ull; }
                        }
                    }
                }
                data.shaders.push_back(std::move(entry)); shader=std::prev(data.shaders.end());
            }
            ++shader->draws;
            draw_record entry{}; entry.shader=static_cast<size_t>(shader-data.shaders.begin()); entry.result=result;
            const auto read=[&](const D3DRENDERSTATETYPE type, DWORD& value) { const auto hr=device->GetRenderState(type,&value); if (FAILED(hr)) entry.state_result=hr; };
            read(D3DRS_ZENABLE,entry.z); read(D3DRS_ZWRITEENABLE,entry.write); read(D3DRS_ALPHABLENDENABLE,entry.blend);
            Microsoft::WRL::ComPtr<IDirect3DSurface8> target,depth;
            device->GetRenderTarget(&target); device->GetDepthStencilSurface(&depth); device->GetViewport(&entry.viewport);
            entry.target=reinterpret_cast<uintptr_t>(target.Get()); entry.depth=reinterpret_cast<uintptr_t>(depth.Get());
            auto sample=std::find_if(data.samples.begin(),data.samples.end(),[&](const auto& s) {
                return s.shader==entry.shader && s.result==result && s.z==entry.z && s.write==entry.write && s.blend==entry.blend &&
                    s.target==entry.target && s.depth==entry.depth && s.state_result==entry.state_result &&
                    s.viewport.X==entry.viewport.X && s.viewport.Y==entry.viewport.Y && s.viewport.Width==entry.viewport.Width && s.viewport.Height==entry.viewport.Height;
            });
            if (sample!=data.samples.end()) { ++sample->draws; return; }
            if (data.samples.size()==max_samples) { ++data.dropped; return; }
            if (target) target->GetDesc(&entry.target_desc); if (depth) depth->GetDesc(&entry.depth_desc);
            device->GetTransform(D3DTS_WORLD,&entry.world); device->GetTransform(D3DTS_VIEW,&entry.view); device->GetTransform(D3DTS_PROJECTION,&entry.projection);
            entry.draws=1; data.samples.push_back(entry);
        }
        catch (...) { ++data.read_errors; }
    }

    auto ring_shader_capture::Finish(const char* reason) -> void
    {
        state_->active=false; state_->data.duration=::GetTickCount64()-state_->start; state_->data.finish=reason;
        state_->writer=std::async(std::launch::async,[data=std::move(state_->data),path=state_->path] { return Save(data,path); });
    }

    auto ring_shader_capture::Tick(IDirect3DDevice8* device, const frame& observed) -> std::vector<std::string>
    {
        std::vector<std::string> messages;
        if (state_->writer.valid() && state_->writer.wait_for(std::chrono::seconds{0})==std::future_status::ready) {
            state_->last_result=state_->writer.get(); messages.push_back(state_->last_result);
        }
        if (state_->active) {
            auto& data=state_->data; ++data.frames; data.boundary_hook=observed.boundary_hook;
            const std::string_view camera=observed.camera;
            if (camera=="terrain") ++data.camera_terrain; else if (camera=="fixed-function") ++data.camera_fixed; else ++data.camera_missing;
            data.overlays+=std::string_view{observed.overlay}!="none";
            if (::GetTickCount64()-state_->start>=capture_ms || data.frames>=max_frames) this->Finish("automatic stop");
        }
        const auto requests=state_->requests.exchange(0,std::memory_order_acq_rel);
        if (requests&static_cast<unsigned>(request::start)) {
            if (state_->active || state_->writer.valid()) messages.emplace_back("[XIUI] Ring shader capture is already recording or saving.");
            else if (state_->directory.empty()) messages.emplace_back("[XIUI] Ring shader capture has no install directory.");
            else {
                state_->data={}; state_->data.version=state_->version; state_->data.build=state_->build;
                SYSTEMTIME now{}; ::GetSystemTime(&now); char stamp[80]{};
                std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
                state_->data.started=stamp;
                std::snprintf(stamp,sizeof(stamp),"rings-%04u%02u%02u-%02u%02u%02u-%03u-%lu-%llu.json",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,::GetCurrentProcessId(),++state_->sequence);
                state_->path=state_->directory/stamp;
                D3DDEVICE_CREATION_PARAMETERS parameters{}; D3DADAPTER_IDENTIFIER8 adapter{};
                Microsoft::WRL::ComPtr<IDirect3D8> d3d;
                if (device && SUCCEEDED(device->GetCreationParameters(&parameters)) && SUCCEEDED(device->GetDirect3D(&d3d)) &&
                    SUCCEEDED(d3d->GetAdapterIdentifier(parameters.AdapterOrdinal,0,&adapter))) {
                    state_->data.adapter.assign(adapter.Description,strnlen_s(adapter.Description,sizeof(adapter.Description)));
                    state_->data.driver.assign(adapter.Driver,strnlen_s(adapter.Driver,sizeof(adapter.Driver)));
                    state_->data.vendor=adapter.VendorId; state_->data.device=adapter.DeviceId;
                }
                state_->start=::GetTickCount64(); state_->active=true;
                messages.emplace_back("[XIUI] Recording ring shader layouts for up to 3 seconds. Keep a living target selected and move the camera normally.");
            }
        }
        if (requests&static_cast<unsigned>(request::stop)) {
            if (state_->active) { this->Finish("manual stop"); messages.emplace_back("[XIUI] Saving ring shader capture."); }
            else messages.emplace_back(state_->writer.valid() ? "[XIUI] Ring shader capture is saving." : "[XIUI] No ring shader capture is recording.");
        }
        if (requests&static_cast<unsigned>(request::status)) {
            messages.emplace_back(state_->active ? "[XIUI] Ring shader capture is recording." : state_->writer.valid() ?
                "[XIUI] Ring shader capture is saving." : state_->last_result.empty() ? "[XIUI] Ring shader capture is idle." : state_->last_result);
        }
        return messages;
    }

    auto ring_shader_capture::Shutdown() noexcept -> void
    {
        try { if (state_->active) this->Finish("plugin unloaded"); if (state_->writer.valid()) state_->last_result=state_->writer.get(); }
        catch (...) {}
        state_->active=false; state_->requests.store(0,std::memory_order_release);
    }
}
