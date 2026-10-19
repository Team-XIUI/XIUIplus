#include "modules/minimap/minimap_module.hpp"
#include "modules/minimap/minimap_source.hpp"
#include "ui/rounded_rect.hpp"
#include "ui/contour.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace
{
    auto Check(const bool value, const char* message) -> void { if (!value) throw std::runtime_error{message}; }
    auto Near(const float a, const float b) -> bool { return std::abs(a - b) < 0.00001f; }
}

namespace xiui
{
    struct minimap_checks
    {
        static auto CheckSourceCache(const std::span<const native_map_record> records) -> void
        {
            minimap_source source; source.records_.assign(records.begin(), records.end());
            const auto* field = source.SelectMap(100, 100, 0, 0, 0);
            Check(field != nullptr && field->floor == 0 && source.SelectMap(100, 200, 100, 0, 100) == field && source.next_floor_ == 0,
                "single-map zones retain their projection without terrain calls as the player moves");
            Check(source.SelectMap(238, 201, 0, 0, 0) == nullptr && source.zone_records_.size() == 2 &&
                source.SelectMap(184, 202, 0, 0, 0) == nullptr && source.zone_records_.size() == 3,
                "cached zone changes discard the previous projection and never guess an unresolved dungeon floor");
            Check(source.SelectMap(999, 203, 0, 0, 0) == nullptr && source.SelectMap(100, 204, 0, 0, 0) != nullptr,
                "missing zone records cannot poison a later valid projection");
            source.Clear();
            Check(source.map_ == nullptr && source.zone_records_.empty() && source.SelectMap(100, 205, 0, 0, 0) != nullptr,
                "disable/logout clears projection pointers and returning to the same zone resolves them again");
        }

        static auto Decode(minimap_maps& maps, const uint16_t zone, const int floor, const uint8_t style) -> bool
        {
            for (const auto& record : maps.records_)
                if (record.zone == zone && record.floor == floor && record.style == style)
                {
                    const auto image = minimap_maps::DecodeImage(record.bytes);
                    if (image)
                    {
                        const auto outline = minimap_maps::ExtractOutline(*image);
                        const ui::rect view{0, 0, 256, 256};
                        Check(ui::ContourContains(view, *outline, {128, 128}, 8) && !ui::ContourContains(view, *outline, {0, 0}),
                            "burnt outline uses alpha alone, retains the map center and cuts out its burned corner");
                        for (const auto point : *outline)
                            Check(point.x >= 0 && point.x <= 1 && point.y >= 0 && point.y <= 1,
                                "worker-extracted burnt contours stay within the map's bounds");
                    }
                    return image && image->width == 512 && image->height == 512 && image->pixels.size() == 512 * 512 &&
                        image->pixels[0] >> 24 == 0 && image->pixels[128 * 512 + 128] >> 24 == 255;
                }
            return false;
        }
    };
}

auto RunMinimapChecks(void) -> bool
{
    using namespace xiui;
    try
    {
        const std::array<native_map_record, 6> records{{
            {100, 0, 1, 0, 1, 1, 0, 1, -312, -248},
            {238, 1, 1, 0, 4, 3, 0, 189, -240, -328},
            {238, 2, 2, 0, 4, 3, 0, 190, -304, -120},
            {184, 15, 1, 0, 4, 26, 0, 73, 112, -272},
            {184, 1, 2, 0, 4, 26, 0, 61, 112, -272},
            {184, 2, 3, 0, 4, 26, 0, 62, 112, -272}}};
        minimap_checks::CheckSourceCache(records);
        const auto origin = MinimapUV(records[0], 0, 0), moved = MinimapUV(records[0], 100, 100);
        Check(Near(origin.x, 156.0f / 256) && Near(origin.y, 124.0f / 256) &&
            Near(moved.x - origin.x, 10.0f / 256) && Near(moved.y - origin.y, -10.0f / 256),
            "field coordinates use the native scale and offsets with north upward");
        const auto north = MinimapUV(records[1], 0, 0), south = MinimapUV(records[2], 0, 0);
        Check(Near(north.x, 120.0f / 256) && Near(north.y, 164.0f / 256) && Near(south.x, 152.0f / 256) && Near(south.y, 60.0f / 256),
            "Windurst Waters uses distinct north and south map origins");
        Check(FindMinimapRecord(records, 184, 15) == &records[3] && FindMinimapRecord(records, 184, 1) == &records[4] &&
            FindMinimapRecord(records, 184, {}) == nullptr && FindMinimapRecord(records, 184, 9) == nullptr &&
            FindMinimapRecord(records, 100, {}) == &records[0] && FindMinimapRecord(records, 999, 0) == nullptr,
            "nonsequential dungeon floor IDs are exact; unresolved floors never select the first map");
        const std::array<ui::point, 5> contour{{{0, 0}, {1, 0}, {1, 1}, {0.5f, 0.5f}, {0, 1}}};
        const ui::rect contourBounds{30, 40, 200, 100};
        Check(ui::ContourContains(contourBounds, contour, {130, 65}, 10) &&
            !ui::ContourContains(contourBounds, contour, {130, 120}) &&
            !ui::ContourContains(contourBounds, contour, {250, 65}),
            "burnt contour hit testing retains concave cutouts and translated wide-map bounds");
        Check(ui::ContourContains(contourBounds, contour, {130, 50}, 10) &&
            !ui::ContourContains(contourBounds, contour, {130, 49}, 10) &&
            ui::ContourContains(contourBounds, contour, {130, 49}),
            "marker margins retain the exact inset boundary without changing plain hit testing");
        const auto outer = ui::ContourPoint(contourBounds, {1, 0.5f});
        const auto inset = ui::ContourPoint(contourBounds, {1, 0.5f}, 8);
        const auto centerPoint = ui::ContourPoint(contourBounds, {0.5f, 0.5f}, 8);
        Check(Near(outer.x, 230) && Near(outer.y, 90) && Near(inset.x, 222) && Near(inset.y, 90) &&
            Near(centerPoint.x, 130) && Near(centerPoint.y, 90),
            "contour feathering keeps its radial inset and a stable center");
        const ui::rect view{10, 20, 240, 240};
        Check(MinimapHeight(240, 0) == 240 && MinimapHeight(240, 1) == 240 && MinimapHeight(240, 2) == 135 &&
            MinimapContains(view, 0, {15, 25}) && !MinimapContains(view, 1, {15, 25}) &&
            MinimapContains(view, 1, {130, 140}, 5) && !MinimapContains(view, 1, {249, 140}, 5),
            "wide maps keep 16:9 dimensions and circle markers respect the entire circular boundary");
        Check(!MinimapContains(view, 0, {14, 24}, 0, 32) && MinimapContains(view, 0, {42, 20}, 0, 32) &&
            !MinimapContains(view, 2, {17, 27}, 6, 32) && MinimapContains(view, 2, {130, 140}, 6, 32) &&
            MinimapCornerRadius({0, 0, 120, 67.5f}, 2, 48) == 33.75f && MinimapCornerRadius(view, 1, 12) == 120,
            "rounded marker boundaries respect corner arcs and clamp to each map shape");
        for (const int shape : {0, 1, 2})
            for (const float zoom : {1.0f, 2.0f, 8.0f})
            {
                const ui::rect viewport{10, 20, 240, MinimapHeight(240, shape)};
                const float size = std::min(viewport.width, viewport.height) * zoom;
                const ui::rect map{130 - 0.6f * size, viewport.y + viewport.height / 2 - 0.4f * size, size, size};
                for (const auto edge : {minimap_coordinate_edge::left, minimap_coordinate_edge::top, minimap_coordinate_edge::right, minimap_coordinate_edge::bottom})
                {
                    const auto strip = MinimapCoordinateStrip(viewport, map, edge);
                    const bool horizontal = edge == minimap_coordinate_edge::top || edge == minimap_coordinate_edge::bottom;
                    Check(Near(horizontal ? strip.bounds.height : strip.bounds.width, size * minimap_coordinate_band),
                        "coordinate strips magnify at the map's scale in every shape");
                    Check(Near(horizontal ? strip.bounds.x - map.x : strip.bounds.y - map.y, (horizontal ? strip.uv.x : strip.uv.y) * size),
                        "coordinate letters and numbers remain aligned with the scrolling map grid");
                    Check(strip.uv.x >= 0 && strip.uv.y >= 0 && strip.uv.x + strip.uv.width <= 1.00001f && strip.uv.y + strip.uv.height <= 1.00001f,
                        "map limits clip coordinate strips instead of stretching texture edge pixels");
                    if (edge == minimap_coordinate_edge::left) Check(Near(strip.bounds.x, viewport.x), "left strip stays pinned");
                    if (edge == minimap_coordinate_edge::top) Check(Near(strip.bounds.y, viewport.y), "top strip stays pinned");
                    if (edge == minimap_coordinate_edge::right) Check(Near(strip.bounds.x + strip.bounds.width, viewport.x + viewport.width), "right strip stays pinned");
                    if (edge == minimap_coordinate_edge::bottom) Check(Near(strip.bounds.y + strip.bounds.height, viewport.y + viewport.height), "bottom strip stays pinned");
                }
            }
        Check(MinimapCoordinateStrip(view, {500, 500, 240, 240}, minimap_coordinate_edge::top).bounds.width == 0 &&
            MinimapCoordinateStrip(view, {500, 500, 240, 240}, minimap_coordinate_edge::left).bounds.height == 0,
            "coordinate strips disappear beyond their map's extent");
        for (const float radius : {1.0f, 12.0f, 48.0f})
            for (size_t i = 0; i < ui::rounded_rect_points; ++i)
            {
                const auto edge = ui::RoundedRectPoint(view, radius, i), inner = ui::RoundedRectPoint(view, radius, i, 1);
                Check(MinimapContains(view, 0, edge, 0, radius) && MinimapContains(view, 0, inner, 1, radius),
                    "map image, frame and shadow share a bounded rounded contour with an inset antialias fringe");
            }
        for (const int shape : {0, 1, 2})
            for (const float heading : std::array<float, 4>{0, 1.570796327f, 3.141592654f, -1.570796327f})
            {
                const ui::rect bounds{10, 20, 240, MinimapHeight(240, shape)};
                const auto cone = MinimapVisionArc(bounds, heading, 1, 160);
                for (const auto point : cone) Check(MinimapContains(bounds, shape, point, 4), "vision cones stay inside every map shape at every heading");
                const float dx = cone[12].x - bounds.x - bounds.width / 2, dy = cone[12].y - bounds.y - bounds.height / 2;
                Check(dx * std::cos(heading) + dy * std::sin(heading) > 0 &&
                    std::abs(dx * std::sin(heading) - dy * std::cos(heading)) < 0.001f, "the vision cone points in the same direction as the player arrow");
            }
        Check(IsMinimapNpc(0x02, 0x200) && IsMinimapNpc(0x100, 0x200) && !IsMinimapNpc(0x01, 0x200) &&
            !IsMinimapNpc(0x10, 0x200) && !IsMinimapNpc(0x02, 0) && !IsMinimapNpc(0x02, 0x4200) && !IsMinimapNpc(0x1100, 0x200),
            "NPC markers exclude players, enemies, unloaded/hidden actors and trusts");

        ui::design_system design;
        minimap_maps maps;
        Check(maps.Count() == 0, "the map atlas is untouched at startup");
        Check(maps.Acquire(100, 0, true).known && maps.Acquire(100, 0, true).enhanced && maps.Count() == 662 &&
            !maps.Acquire(100, 0, false).enhanced && maps.Acquire(1, 0, true).known && !maps.Acquire(1, 0, true).enhanced &&
            !maps.Acquire(999, 0, true).known && !maps.Acquire(184, 99, true).known,
            "both embedded sets resolve by native floor ID with a regular fallback and no stale-map substitution");
        for (int repeat = 0; repeat < 3; ++repeat)
            Check(!maps.Acquire(184, 99, true).known && maps.Acquire(184, 15, true).enhanced &&
                !maps.Acquire(184, 15, false).enhanced && maps.Acquire(100, 0, true).enhanced,
                "cached artwork selection follows floor, zone and style changes after both hits and misses");
        const auto com = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        const bool decoded = SUCCEEDED(com) && minimap_checks::Decode(maps, 100, 0, 0) && minimap_checks::Decode(maps, 184, 15, 1);
        if (SUCCEEDED(com)) ::CoUninitialize();
        Check(decoded, "the production decoder reads regular field art and enhanced nonsequential dungeon art without external files");
        minimap_state live{}; live.available = true; live.zone = 100; live.name = "West Ronfaure"; live.player = {0.5f, 0.5f};
        minimap_module module; module.ApplyTextDefaults(design.GetTheme()); module.SetSource(live, maps);
        constexpr size_t textSelector = minimap_module::text_selector_option, zoomVisibility = minimap_module::zoom_readout_option;
        constexpr size_t coordinateEdges = minimap_module::coordinate_edges_option;
        for (size_t edge = 0; edge < 4; ++edge)
        {
            Check(module.Option(coordinateEdges).button_toggles[edge] && module.ConfigSectionContains(0, coordinateEdges),
                "all four coordinate edges default on and share one General button row");
            module.Option(coordinateEdges).button_toggles[edge] = edge % 2 == 0;
        }
        auto* labelX = module.Option(minimap_module::text_x_option).slider;
        auto* labelY = module.Option(minimap_module::text_y_option).slider;
        *labelX = 4; *labelY = 6;
        Check(module.Option(textSelector).selector == module_selector::element && module.ConfigSectionContains(3, textSelector),
            "minimap text offers the shared Labels/Zoom selector for every input method");
        *module.Option(textSelector).choice = 1;
        *module.Option(minimap_module::text_x_option).slider = -230; *module.Option(minimap_module::text_y_option).slider = -180;
        *module.Option(minimap_module::text_size_option).slider = 1.1f;
        Check(*labelX == 4 && *labelY == 6 && module.Option(minimap_module::text_x_option).minimum == -600 && module.Option(minimap_module::text_y_option).maximum == 600,
            "zoom text moves independently across the full map without changing other label offsets");
        *module.Option(zoomVisibility).toggle = false;
        Check(module.Option(zoomVisibility).visible && !module.Option(minimap_module::text_x_option).visible && !module.Option(minimap_module::zone_name_option).visible,
            "Zoom has its own visibility toggle and hides inactive controls");
        *module.Option(zoomVisibility).toggle = true;
        size_t textProfiles{}; module.VisitTextSettings([&](ui::text_style&) { ++textProfiles; });
        Check(textProfiles == 2, "global typography includes the independent zoom profile");
        Check(!*module.Option(minimap_module::vision_cone_option).toggle && module.NpcsEnabled(), "vision cone is optional and NPC dots start enabled");
        Check(*module.Option(minimap_module::edge_style_option).choice == 0 && !*module.Option(minimap_module::shadow_option).toggle && !module.Option(minimap_module::corner_radius_option).visible && !module.Option(minimap_module::shadow_opacity_option).visible,
            "rounding and shadow are opt-in and hide their dependent controls");
        *module.Option(minimap_module::edge_style_option).choice = 1; *module.Option(minimap_module::corner_radius_option).slider = 24;
        *module.Option(minimap_module::shadow_option).toggle = true; *module.Option(minimap_module::shadow_opacity_option).slider = 0.65f; *module.Option(minimap_module::shadow_softness_option).slider = 8;
        *module.Option(minimap_module::shadow_x_option).slider = -3; *module.Option(minimap_module::shadow_y_option).slider = 5;
        *module.Option(minimap_module::vision_cone_option).toggle = true; *module.Option(minimap_module::cone_angle_option).slider = 110;
        *module.Option(minimap_module::cone_size_option).slider = 0.6f; *module.Option(minimap_module::cone_opacity_option).slider = 0.35f; *module.Option(minimap_module::npcs_option).toggle = false;
        module.State().bounds.x = 123; module.State().bounds.y = 234;
        module_render_context context{design}; module.Render({}, context);
        bool save{};
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 122, 300, save) && !save &&
            !module.HandleMouseWheel(WHEEL_DELTA, 200, 460, save) &&
            !module.HandleMouseWheel(WHEEL_DELTA, 124, 235, save) && *module.Option(minimap_module::zoom_option).slider == 2,
            "wheel input outside the map, over its caption or beyond rounded corners leaves zoom and gameplay input alone");
        Check(module.HandleMouseWheel(WHEEL_DELTA, 200, 300, save) && save && *module.Option(minimap_module::zoom_option).slider == 2.25f &&
            module.HandleMouseWheel(-2 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(minimap_module::zoom_option).slider == 1.75f,
            "hovered wheel zoom uses the shared quarter-step slider in both directions");
        Check(module.HandleMouseWheel(100 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(minimap_module::zoom_option).slider == 8 &&
            module.HandleMouseWheel(WHEEL_DELTA, 200, 300, save) && !save &&
            module.HandleMouseWheel(-100 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(minimap_module::zoom_option).slider == 1,
            "wheel zoom clamps to the configured limits and retains event ownership without redundant saves");
        *module.Option(minimap_module::shape_option).choice = 1;
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 124, 235, save) &&
            module.HandleMouseWheel(WHEEL_DELTA / 2, 233, 344, save) && save && *module.Option(minimap_module::zoom_option).slider == 1.125f,
            "Circle excludes its outer corners and high-resolution wheel deltas retain their proportion");
        *module.Option(minimap_module::shape_option).choice = 2;
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 200, 400, save), "Wide excludes the area below its actual map height");
        *module.Option(minimap_module::shape_option).choice = 0;
        *module.Option(minimap_module::zoom_option).slider = 3.75f; module.OnOptionChanged(minimap_module::zoom_option);
        live.zone = 238; module.Render({}, context);
        Check(*module.Option(minimap_module::zoom_option).slider == 2, "a new zone starts at its default zoom");
        *module.Option(minimap_module::zoom_option).slider = 5.5f; module.OnOptionChanged(minimap_module::zoom_option);
        *module.Option(minimap_module::shape_option).choice = 2; *module.Option(minimap_module::artwork_option).choice = 0;
        auto store = settings_store::FromIniText(""); module.Save(store);
        minimap_module loaded; loaded.ApplyTextDefaults(design.GetTheme()); loaded.SetSource(live, maps); loaded.Load(store);
        loaded.Render({}, context);
        for (size_t edge = 0; edge < 4; ++edge)
            Check(loaded.Option(coordinateEdges).button_toggles[edge] == (edge % 2 == 0), "coordinate edges persist independently through settings/profile reloads");
        Check(*loaded.Option(textSelector).choice == 1 && *loaded.Option(zoomVisibility).toggle &&
            *loaded.Option(minimap_module::text_x_option).slider == -230 && *loaded.Option(minimap_module::text_y_option).slider == -180 && *loaded.Option(minimap_module::text_size_option).slider == 1.1f,
            "zoom visibility, large position offsets, size and selected text survive settings/profile reloads");
        *loaded.Option(textSelector).choice = 0;
        Check(*loaded.Option(minimap_module::text_x_option).slider == 4 && *loaded.Option(minimap_module::text_y_option).slider == 6,
            "the existing minimap label profile retains its independent saved offsets");
        Check(*loaded.Option(minimap_module::edge_style_option).choice == 1 && *loaded.Option(minimap_module::corner_radius_option).slider == 24 && *loaded.Option(minimap_module::shadow_option).toggle &&
            *loaded.Option(minimap_module::shadow_opacity_option).slider == 0.65f && *loaded.Option(minimap_module::shadow_softness_option).slider == 8 && *loaded.Option(minimap_module::shadow_x_option).slider == -3 && *loaded.Option(minimap_module::shadow_y_option).slider == 5 &&
            loaded.ConfigSectionContains(1, minimap_module::edge_style_option) && loaded.ConfigSectionContains(1, minimap_module::shadow_y_option) && loaded.ConfigSectionContains(3, minimap_module::text_font_option),
            "rounding and shadow controls persist in Appearance without changing text or layout preferences");
        *loaded.Option(minimap_module::shape_option).choice = 1;
        Check(!loaded.Option(minimap_module::edge_style_option).visible && !loaded.Option(minimap_module::corner_radius_option).visible && loaded.Option(minimap_module::shadow_opacity_option).available && loaded.Option(minimap_module::shadow_opacity_option).visible,
            "Circle keeps its geometry while still offering a matching drop shadow");
        *loaded.Option(minimap_module::shape_option).choice = 2;
        Check(*loaded.Option(minimap_module::vision_cone_option).toggle && *loaded.Option(minimap_module::cone_angle_option).slider == 110 && *loaded.Option(minimap_module::cone_size_option).slider == 0.6f &&
            *loaded.Option(minimap_module::cone_opacity_option).slider == 0.35f && !loaded.NpcsEnabled() && loaded.ConfigSectionContains(2, minimap_module::npcs_option) && !loaded.ConfigSectionContains(3, minimap_module::vision_cone_option),
            "cone controls and NPC visibility use the Markers section and survive profiles/reload");
        Check(*loaded.Option(minimap_module::zoom_option).slider == 5.5f && *loaded.Option(minimap_module::shape_option).choice == 2 && *loaded.Option(minimap_module::artwork_option).choice == 0 &&
            loaded.State().bounds.x == 123 && loaded.State().bounds.y == 234 && loaded.State().bounds.height == 220 * 9.0f / 16 + 20,
            "map artwork, shape, position and zone zoom survive a settings round trip");
        live.zone = 100; loaded.Render({}, context);
        Check(*loaded.Option(minimap_module::zoom_option).slider == 3.75f, "returning to another zone restores its own zoom");
        live.zone = 201;
        Check(loaded.HandleMouseWheel(WHEEL_DELTA, 200, 280, save) && save && *loaded.Option(minimap_module::zoom_option).slider == 2.25f,
            "wheel input synchronizes a new zone before changing its zoom, even before the next render");
        loaded.Save(store); loaded.Load(store); loaded.Render({}, context);
        Check(*loaded.Option(minimap_module::zoom_option).slider == 2.25f, "wheel zoom survives the normal settings/profile round trip");
        *loaded.Option(minimap_module::edge_style_option).choice = 2;
        Check(!loaded.Option(minimap_module::corner_radius_option).visible && !loaded.Option(minimap_module::frame_option).visible && !loaded.Option(coordinateEdges).visible,
            "Burnt replaces the rounded contour, native frame and coordinate strips without changing their saved preferences");
        loaded.Save(store); loaded.Load(store);
        Check(*loaded.Option(minimap_module::edge_style_option).choice == 2 && loaded.Option(coordinateEdges).button_toggles[0] && !loaded.Option(coordinateEdges).button_toggles[1],
            "Burnt persists through profiles while retaining independent coordinate edge choices");
        *loaded.Option(minimap_module::edge_style_option).choice = 1;
        Check(loaded.Option(minimap_module::corner_radius_option).visible && loaded.Option(minimap_module::frame_option).visible && loaded.Option(coordinateEdges).visible,
            "leaving Burnt restores the existing rounded, frame and coordinate controls");
        minimap_module legacy;
        legacy.Load(settings_store::FromIniText("[minimap]\nrounded_corners=1\n"));
        Check(*legacy.Option(minimap_module::edge_style_option).choice == 1, "existing rounded-corner preferences migrate to the edge style selector");
        *loaded.Option(minimap_module::enabled_option).toggle = false;
        Check(!loaded.ShouldRender({}) && !loaded.PreviewVisible() && !loaded.HandleMouseWheel(WHEEL_DELTA, 200, 280, save),
            "disabled minimaps leave gameplay, wheel input and edit mode");
        loaded.ResetDefaults(design.GetTheme()); loaded.Save(store);
        module.Load(store); module.Render({}, context);
        Check(*module.Option(minimap_module::zoom_option).slider == 2 && module.State().bounds.x == 123 && module.State().bounds.y == 234,
            "reset clears remembered zoom while preserving the movement anchor");
        minimap_source source; source.Update(nullptr, 100);
        Check(!source.State().available && source.State().zone == 0, "logout and missing native state clear map identity and markers");
        maps.Shutdown(); Check(maps.Count() == 0, "shutdown clears the bounded map cache and resource index");
        Check(maps.Acquire(100, 0, true).enhanced && maps.Acquire(100, 0, true).known,
            "reinitialization discards cached record identities before reusing the embedded catalog");
        maps.Shutdown();
        std::puts("Minimap checks passed."); return true;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "Minimap: %s\n", error.what()); return false; }
}
