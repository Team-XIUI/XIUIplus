#pragma once

#include <d3d8.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace xiui
{
    inline constexpr DWORD terrain_vertex_declaration[]{536870912,1073872896,1073872899,1074003973,1073807367,1073872911,4294967295};
    inline auto IsTerrainVertexDeclaration(std::span<const DWORD> declaration) -> bool
    { return std::equal(declaration.begin(),declaration.end(),std::begin(terrain_vertex_declaration),std::end(terrain_vertex_declaration)); }

    enum class ring_camera_result
    {
        state_unavailable, depth_disabled, depth_write_disabled, alpha_blended, declaration_unrecognized,
        vertex_format_unrecognized, target_unavailable, terrain_camera_retained, different_scene_target,
        projection_unavailable, projection_not_perspective, transform_unavailable, sky_camera, accepted_fixed, accepted_terrain
    };

    class ring_shader_capture
    {
    public:
        enum class request : unsigned { start=1, stop=2, status=4 };
        struct frame { const char* camera; const char* overlay; const char* boundary_hook; };
        struct draw_scope
        {
            ring_shader_capture* capture{};
            IDirect3DDevice8* device{};
            ring_camera_result result{ring_camera_result::state_unavailable};
            ~draw_scope();
        };
        ring_shader_capture();
        ~ring_shader_capture();
        auto Initialize(const std::filesystem::path& install, const char* version, const char* build) -> void;
        auto Request(request value) noexcept -> void;
        auto Trace(IDirect3DDevice8* device) noexcept -> draw_scope;
        auto Tick(IDirect3DDevice8* device, const frame& observed) -> std::vector<std::string>;
        auto Shutdown() noexcept -> void;
    private:
        struct state;
        std::unique_ptr<state> state_;
        auto Record(IDirect3DDevice8* device, ring_camera_result result) noexcept -> void;
        auto Finish(const char* reason) -> void;
    };
}
