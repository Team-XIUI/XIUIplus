#include "modules/minimap/minimap_module.hpp"
#include "modules/minimap/minimap_source.hpp"
#include "ui/rounded_rect.hpp"

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
                    return image && image->width == 512 && image->height == 512 && image->pixels.size() == 512 * 512 &&
                        image->pixels[0] >> 24 == 255 && image->pixels[0] != image->pixels[128 * 512 + 128];
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
        const ui::rect view{10, 20, 240, 240};
        Check(MinimapHeight(240, 0) == 240 && MinimapHeight(240, 1) == 240 && MinimapHeight(240, 2) == 135 &&
            MinimapContains(view, 0, {15, 25}) && !MinimapContains(view, 1, {15, 25}) &&
            MinimapContains(view, 1, {130, 140}, 5) && !MinimapContains(view, 1, {249, 140}, 5),
            "wide maps keep 16:9 dimensions and circle markers respect the entire circular boundary");
        Check(!MinimapContains(view, 0, {14, 24}, 0, 32) && MinimapContains(view, 0, {42, 20}, 0, 32) &&
            !MinimapContains(view, 2, {17, 27}, 6, 32) && MinimapContains(view, 2, {130, 140}, 6, 32) &&
            MinimapCornerRadius({0, 0, 120, 67.5f}, 2, 48) == 33.75f && MinimapCornerRadius(view, 1, 12) == 120,
            "rounded marker boundaries respect corner arcs and clamp to each map shape");
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
        constexpr size_t textSelector = 32 + text_profile_option_count, zoomVisibility = textSelector + 1;
        auto* labelX = module.Option(34).slider;
        auto* labelY = module.Option(35).slider;
        *labelX = 4; *labelY = 6;
        Check(module.Option(textSelector).selector == module_selector::element && module.ConfigSectionContains(3, textSelector),
            "minimap text offers the shared Labels/Zoom selector for every input method");
        *module.Option(textSelector).choice = 1;
        *module.Option(34).slider = -230; *module.Option(35).slider = -180;
        *module.Option(33).slider = 1.1f;
        Check(*labelX == 4 && *labelY == 6 && module.Option(34).minimum == -600 && module.Option(35).maximum == 600,
            "zoom text moves independently across the full map without changing other label offsets");
        *module.Option(zoomVisibility).toggle = false;
        Check(module.Option(zoomVisibility).visible && !module.Option(34).visible && !module.Option(15).visible,
            "Zoom has its own visibility toggle and hides inactive controls");
        *module.Option(zoomVisibility).toggle = true;
        size_t textProfiles{}; module.VisitTextSettings([&](ui::text_style&) { ++textProfiles; });
        Check(textProfiles == 2, "global typography includes the independent zoom profile");
        Check(!*module.Option(19).toggle && module.NpcsEnabled(), "vision cone is optional and NPC dots start enabled");
        Check(!*module.Option(24).toggle && !*module.Option(26).toggle && !module.Option(25).visible && !module.Option(27).visible,
            "rounding and shadow are opt-in and hide their dependent controls");
        *module.Option(24).toggle = true; *module.Option(25).slider = 24;
        *module.Option(26).toggle = true; *module.Option(27).slider = 0.65f; *module.Option(28).slider = 8;
        *module.Option(29).slider = -3; *module.Option(30).slider = 5;
        *module.Option(19).toggle = true; *module.Option(20).slider = 110;
        *module.Option(21).slider = 0.6f; *module.Option(22).slider = 0.35f; *module.Option(23).toggle = false;
        module.State().bounds.x = 123; module.State().bounds.y = 234;
        module_render_context context{design}; module.Render({}, context);
        bool save{};
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 122, 300, save) && !save &&
            !module.HandleMouseWheel(WHEEL_DELTA, 200, 460, save) &&
            !module.HandleMouseWheel(WHEEL_DELTA, 124, 235, save) && *module.Option(4).slider == 2,
            "wheel input outside the map, over its caption or beyond rounded corners leaves zoom and gameplay input alone");
        Check(module.HandleMouseWheel(WHEEL_DELTA, 200, 300, save) && save && *module.Option(4).slider == 2.25f &&
            module.HandleMouseWheel(-2 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(4).slider == 1.75f,
            "hovered wheel zoom uses the shared quarter-step slider in both directions");
        Check(module.HandleMouseWheel(100 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(4).slider == 8 &&
            module.HandleMouseWheel(WHEEL_DELTA, 200, 300, save) && !save &&
            module.HandleMouseWheel(-100 * WHEEL_DELTA, 200, 300, save) && save && *module.Option(4).slider == 1,
            "wheel zoom clamps to the configured limits and retains event ownership without redundant saves");
        *module.Option(1).choice = 1;
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 124, 235, save) &&
            module.HandleMouseWheel(WHEEL_DELTA / 2, 233, 344, save) && save && *module.Option(4).slider == 1.125f,
            "Circle excludes its outer corners and high-resolution wheel deltas retain their proportion");
        *module.Option(1).choice = 2;
        Check(!module.HandleMouseWheel(WHEEL_DELTA, 200, 400, save), "Wide excludes the area below its actual map height");
        *module.Option(1).choice = 0;
        *module.Option(4).slider = 3.75f; module.OnOptionChanged(4);
        live.zone = 238; module.Render({}, context);
        Check(*module.Option(4).slider == 2, "a new zone starts at its default zoom");
        *module.Option(4).slider = 5.5f; module.OnOptionChanged(4);
        *module.Option(1).choice = 2; *module.Option(2).choice = 0;
        auto store = settings_store::FromIniText(""); module.Save(store);
        minimap_module loaded; loaded.ApplyTextDefaults(design.GetTheme()); loaded.SetSource(live, maps); loaded.Load(store);
        loaded.Render({}, context);
        Check(*loaded.Option(textSelector).choice == 1 && *loaded.Option(zoomVisibility).toggle &&
            *loaded.Option(34).slider == -230 && *loaded.Option(35).slider == -180 && *loaded.Option(33).slider == 1.1f,
            "zoom visibility, large position offsets, size and selected text survive settings/profile reloads");
        *loaded.Option(textSelector).choice = 0;
        Check(*loaded.Option(34).slider == 4 && *loaded.Option(35).slider == 6,
            "the existing minimap label profile retains its independent saved offsets");
        Check(*loaded.Option(24).toggle && *loaded.Option(25).slider == 24 && *loaded.Option(26).toggle &&
            *loaded.Option(27).slider == 0.65f && *loaded.Option(28).slider == 8 && *loaded.Option(29).slider == -3 && *loaded.Option(30).slider == 5 &&
            loaded.ConfigSectionContains(1, 24) && loaded.ConfigSectionContains(1, 30) && loaded.ConfigSectionContains(3, 32),
            "rounding and shadow controls persist in Appearance without changing text or layout preferences");
        *loaded.Option(1).choice = 1;
        Check(!loaded.Option(24).visible && !loaded.Option(25).visible && loaded.Option(27).available && loaded.Option(27).visible,
            "Circle keeps its geometry while still offering a matching drop shadow");
        *loaded.Option(1).choice = 2;
        Check(*loaded.Option(19).toggle && *loaded.Option(20).slider == 110 && *loaded.Option(21).slider == 0.6f &&
            *loaded.Option(22).slider == 0.35f && !loaded.NpcsEnabled() && loaded.ConfigSectionContains(2, 23) && !loaded.ConfigSectionContains(3, 19),
            "cone controls and NPC visibility use the Markers section and survive profiles/reload");
        Check(*loaded.Option(4).slider == 5.5f && *loaded.Option(1).choice == 2 && *loaded.Option(2).choice == 0 &&
            loaded.State().bounds.x == 123 && loaded.State().bounds.y == 234 && loaded.State().bounds.height == 220 * 9.0f / 16 + 20,
            "map artwork, shape, position and zone zoom survive a settings round trip");
        live.zone = 100; loaded.Render({}, context);
        Check(*loaded.Option(4).slider == 3.75f, "returning to another zone restores its own zoom");
        live.zone = 201;
        Check(loaded.HandleMouseWheel(WHEEL_DELTA, 200, 280, save) && save && *loaded.Option(4).slider == 2.25f,
            "wheel input synchronizes a new zone before changing its zoom, even before the next render");
        loaded.Save(store); loaded.Load(store); loaded.Render({}, context);
        Check(*loaded.Option(4).slider == 2.25f, "wheel zoom survives the normal settings/profile round trip");
        *loaded.Option(0).toggle = false;
        Check(!loaded.ShouldRender({}) && !loaded.PreviewVisible() && !loaded.HandleMouseWheel(WHEEL_DELTA, 200, 280, save),
            "disabled minimaps leave gameplay, wheel input and edit mode");
        loaded.ResetDefaults(design.GetTheme()); loaded.Save(store);
        module.Load(store); module.Render({}, context);
        Check(*module.Option(4).slider == 2 && module.State().bounds.x == 123 && module.State().bounds.y == 234,
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
