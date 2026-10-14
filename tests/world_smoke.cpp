#include "world/world_state.hpp"
#include "modules/compass/compass_module.hpp"
#include "modules/world/world_info_module.hpp"
#include "resources/menu_window.hpp"
#include "resources/menu_sprite.hpp"

#include <cstdio>
#include <fstream>
#include <stdexcept>

namespace
{
    auto Check(const bool condition, const char* message) -> void { if (!condition) throw std::runtime_error{message}; }
}

int main(const int argc, char** argv)
{
    try
    {
        using namespace xiui;
        const auto epoch = DecodeVanaTime(0), day = DecodeVanaTime(3456), month = DecodeVanaTime(103680), year = DecodeVanaTime(1244160);
        Check(epoch.year == 886 && epoch.month == 1 && epoch.day == 1 && epoch.weekday == 0, "Vana epoch");
        Check(day.day == 2 && day.weekday == 1 && day.hour == 0 && day.minute == 0, "day rollover");
        Check(month.month == 2 && month.day == 1 && year.year == 887 && year.month == 1, "calendar rollover");
        const auto time = DecodeVanaTime(144 + 72);
        Check(time.hour == 1 && time.minute == 30, "Vana hour and minute rate");
        Check(std::string{HeadingName(-1.570796327f)} == "N" && std::string{HeadingName(0)} == "E" && std::string{HeadingName(1.570796327f)} == "S", "client heading convention");
        Check(WeatherElement(0) == -1 && WeatherElement(6) == 2 && WeatherElement(8) == 1 && WeatherElement(19) == 7, "weather icon mapping");
        const auto north = ProjectCompassPoint(0, 140, 3.141592654f, 0.4f);
        const auto east = ProjectCompassPoint(-140, 0, 3.141592654f, 0.4f);
        const auto turned = ProjectCompassPoint(0, 140, 1.570796327f, 0.4f);
        Check(std::abs(north[0]) < 0.001f && north[1] < -25 && east[0] > 55 && std::abs(east[1]) < 0.001f && turned[0] < -55,
            "native compass cardinal placement and camera rotation");
        const auto theme = ui::theme::CreateDefault();
        Check(theme.colors.accent != theme.colors.gold && theme.colors.selected != 0x40F4DA97, "neutral interaction colors");
        settings_store settings{""};
        compass_module compass; compass.ApplyTextDefaults(theme);
        compass.State().bounds = {10, 20, 180, 200}; compass.State().enabled = false;
        *compass.Option(5).slider = 75; *compass.Option(2).toggle = false;
        compass.Save(settings);
        compass_module restored; restored.ApplyTextDefaults(theme); restored.Load(settings);
        Check(!restored.State().enabled && restored.State().bounds.x == 10 && restored.State().bounds.width == 180 && *restored.Option(5).slider == 75 && !*restored.Option(2).toggle, "compass layout and options persist");
        world_info_module info; info.ApplyTextDefaults(theme); info.State().bounds = {30, 40, 260, 90};
        *info.Option(4).toggle = true; *info.Option(5).choice = 2;
        *info.Option(6 + 10).toggle = true;
        info.Save(settings);
        world_info_module restoredInfo; restoredInfo.ApplyTextDefaults(theme); restoredInfo.Load(settings);
        Check(restoredInfo.State().bounds.x == 30 && *restoredInfo.Option(4).toggle && *restoredInfo.Option(5).choice == 2 && *restoredInfo.Option(6 + 10).toggle, "world text profiles and options persist");
        ui::design_system design;
        module_render_context context{design, nullptr, true};
        hud_snapshot snapshot{};
        restoredInfo.Render(snapshot, context);
        Check(restoredInfo.State().bounds.height > 45 && restoredInfo.State().bounds.height < 51 && !settings.ReadBool("world", "background", true), "compact frameless date row");
        *restoredInfo.Option(4).toggle = false; restoredInfo.Render(snapshot, context);
        Check(restoredInfo.State().bounds.height < 35, "compact time and weather rows");
        restored.State().enabled = true; restored.Render(snapshot, context);
        Check(restored.State().bounds.height < 180 * 0.6f, "native compass is flattened rather than circular");
        settings_store legacy{""};
        legacy.WriteRect("layout.world", {42, 56, 236, 74}); legacy.WriteBool("world", "background", true);
        legacy.WriteFloat("text.world.time", "scale", 0.95f); legacy.WriteFloat("text.world.weather", "scale", 1.1f);
        world_info_module compact; compact.ApplyTextDefaults(theme); compact.Load(legacy); compact.Save(legacy);
        Check(compact.State().bounds.x == 42 && compact.State().bounds.y == 56 && compact.State().bounds.width == 160 &&
            std::abs(legacy.ReadFloat("text.world.time", "scale", 0) - 0.85f) < 0.001f &&
            std::abs(legacy.ReadFloat("text.world.weather", "scale", 0) - 1.1f) < 0.001f, "compact migration preserves positions and custom text sizes");
        legacy.WriteRect("layout.compass", {44, 66, 144, 164}); legacy.WriteBool("compass", "heading", true);
        compass_module nativeCompass; nativeCompass.ApplyTextDefaults(theme); nativeCompass.Load(legacy); nativeCompass.Save(legacy);
        Check(nativeCompass.State().bounds.x == 44 && nativeCompass.State().bounds.y == 66 && nativeCompass.State().bounds.width == 128 &&
            !*nativeCompass.Option(3).toggle, "old compass defaults migrate to native proportions");
        if (argc > 1)
        {
            std::ifstream file{argv[1], std::ios::binary | std::ios::ate}; Check(static_cast<bool>(file), "open native window DAT");
            std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg())); file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            const auto decoded = resources::DecodeMenuWindow(bytes);
            Check(decoded && decoded->textures[0].width > 0 && decoded->patches[0].width > 0, "shared texture decoder retains native windows");
        }
        if (argc > 2)
        {
            std::ifstream file{argv[2], std::ios::binary | std::ios::ate}; Check(static_cast<bool>(file), "open native UI DAT");
            std::vector<uint8_t> bytes(static_cast<size_t>(file.tellg())); file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
            std::optional<resources::menu_sprite> compassDefinition;
            for (size_t at = 0; at + 16 <= bytes.size();)
            {
                uint32_t meta{}; std::memcpy(&meta, bytes.data() + at + 4, 4);
                const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
                Check(size >= 16 && size <= bytes.size() - at, "native section bounds");
                if ((meta & 0x7F) == 0x31 && size >= 35 && std::string_view{reinterpret_cast<const char*>(bytes.data() + at + 16), 16} == "menu    compass ")
                {
                    const auto payload = std::span{bytes}.subspan(at + 16, size - 16);
                    compassDefinition = resources::DecodeMenuSprite(payload);
                    Check(!resources::DecodeMenuSprite(payload.first(80)), "truncated compass definition rejected");
                    break;
                }
                at += size;
            }
            Check(compassDefinition && compassDefinition->size() == 6 && (*compassDefinition)[0].size() == 4, "native compass has four rim quadrants and five glyph groups");
            const auto& rim = (*compassDefinition)[0];
            Check(rim[0].points[0] == -128 && rim[0].points[1] == -128 && rim[3].points[6] == 128 && rim[3].points[7] == 128 &&
                rim[0].uv_flags == 0 && rim[1].uv_flags == 1 && rim[2].uv_flags == 2 && rim[3].uv_flags == 3, "DAT rim geometry and UV mirrors retained");
            const auto& northGlyph = (*compassDefinition)[4][0]; const auto& marker = (*compassDefinition)[5][0];
            Check(northGlyph.texture == "news    " && northGlyph.width == 16 && northGlyph.colors[0] == 0x7F7F4040 &&
                marker.texture == "marker  " && marker.width == 6 && marker.u == 48 && marker.v == 21 && marker.additive, "native direction tint and additive marker retained");
        }
        std::puts("world smoke passed"); return 0;
    }
    catch (const std::exception& error) { std::fprintf(stderr, "world smoke failed: %s\n", error.what()); return 1; }
}
