#pragma once

#include "../module.hpp"
#include "world/zone_ground.hpp"
#include <future>

namespace xiui
{
    struct target_ring_settings
    {
        static constexpr size_t option_count = 9;
        bool enabled{true}, hp_fill{true}, follow_name_color{true}, show_npcs{}, show_players{};
        bool scale_with_model{true};
        float inner_radius{1.15f}, outer_radius{1.4f};
        D3DCOLOR color{0xD060E8F0};
        auto Load(const settings_store& store) -> void;
        auto Save(const settings_store& store) const -> void;
        auto Option(size_t index) -> module_option;
        auto Changed(size_t index) -> void;
        auto Allows(uint32_t spawn_flags) const -> bool;
        auto Color(D3DCOLOR name_color) const -> D3DCOLOR;
        auto RadiusScale(float model_hitbox_radius) const -> float;
        static auto Section(size_t index) -> size_t { return index == 2 || index == 3 || index == 8 ? 1 : index == 4 || index == 5 ? 2 : 0; }
    };

    class target_ring_renderer
    {
    public:
        struct vertex { float x, y, z; D3DCOLOR color; };
        struct screen_vertex { float x,y,z,rhw; D3DCOLOR color; };
        ~target_ring_renderer();
        auto Status(void) const -> std::string;
        auto Draw(IAshitaCore* core, IDirect3DDevice8* device, const hud_snapshot& snapshot, const target_ring_settings& settings) -> void;
        // Geometry remains independent of the camera and clips to each actual floor triangle.
        static auto Build(const zone_ground& ground, const ground_point& center, float hp, const target_ring_settings& settings, std::vector<vertex>& vertices,
            float yaw = 0, float model_hitbox_radius = 1) -> void;
        static auto Project(std::span<const vertex> vertices, const hud_snapshot& snapshot, std::vector<screen_vertex>& screen,
            double depth_unit = 1.0 / 16777215.0) -> void;
    private:
        uint16_t zone_{}, loading_zone_{};
        std::shared_ptr<std::atomic_bool> cancel_{};
        std::future<std::unique_ptr<zone_ground>> loading_{};
        std::unique_ptr<zone_ground> ground_{};
        std::vector<vertex> vertices_{};
        std::vector<screen_vertex> screen_vertices_{};
        ground_point last_center_{};
        target_ring_settings last_settings_{};
        float last_scale_{1}, current_model_radius_{}, current_scale_{1};
        meter_animation_state hp_animation_{};
        uint32_t last_id_{};
        float last_hp_{}, last_yaw_{};
        bool mesh_valid_{};
        const char* stage_{"waiting for Target visibility"};
        uint32_t current_id_{}, current_hp_{}, current_index_{}, spawn_flags_{}, render_flags_{};
        bool camera_valid_{};
        HRESULT draw_result_{S_OK};
        struct { uint32_t count; float last_ms, max_ms; } builds_{};
        auto UpdateGround(uint16_t zone) -> void;
    };
}
