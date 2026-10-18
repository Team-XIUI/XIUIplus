#include "ui/text_fit.hpp"
#include "hud_runtime.hpp"
#include "config/contributor_list.hpp"
#include "modules/bar_config.hpp"
#include "BinaryData.h"
#include "resources/menu_texture_scale.hpp"
#include "resources/item_icon.hpp"
#include "settings/profile_share.hpp"
#include "settings/profile_share_delta.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>

auto RunPlayerResourceChecks(void) -> bool;
auto RunMinimapChecks(void) -> bool;
auto RunSkillchainChecks(void) -> bool;
auto RunPhoenixIntegrationChecks(void) -> bool;
auto RunMpCostPreviewChecks(void) -> bool;
auto RunHealPredictionChecks(void) -> bool;
auto RunTargetRingChecks(const char* client_root) -> bool;
auto RunTargetRingStateChecks(void) -> bool;
auto RunBlueMagicChecks(void) -> bool;
auto RunPhantomRollChecks(void) -> bool;
auto RunIconThemeChecks(void) -> bool;
auto RunConfigResponsiveChecks(void) -> bool;
namespace xiui { auto RunTargetChildChecks(void) -> bool; }
namespace xiui
{
    auto RunTargetDisplayChecks(void) -> bool;
    auto RunReadyCheckSoundChecks(void) -> bool;
    auto RunMobInfoChecks(void) -> bool;
    auto RunPetHudChecks(void) -> bool;
    auto RunNotificationFeedbackChecks(void) -> bool;
}

namespace
{
    auto check(bool condition, const char* message) -> bool
    {
        if (!condition) std::fprintf(stderr, "config input check failed: %s\n", message);
        return condition;
    }

    auto cursor_ownership_cases(void) -> bool
    {
        const auto count = [] {
            const int value = ::ShowCursor(TRUE) - 1;
            ::ShowCursor(FALSE);
            return value;
        };
        const auto initial = count();
        const auto original = ::GetCursor();
        struct cleanup
        {
            int count;
            HCURSOR cursor;
            ~cleanup()
            {
                int current = ::ShowCursor(TRUE) - 1;
                ::ShowCursor(FALSE);
                while (current < this->count) current = ::ShowCursor(TRUE);
                while (current > this->count) current = ::ShowCursor(FALSE);
                ::SetCursor(this->cursor);
            }
        } restore{initial, original};
        xiui::cursor_renderer cursor;
        cursor.SetNativeCursorHidden(true);
        const auto hidden = count();
        cursor.SetNativeCursorHidden(true);
        if (!check(hidden < 0 && count() == hidden, "repeated cursor capture does not accumulate Win32 hides")) return false;
        bool workerPreserved{};
        std::thread worker{[&] {
            const auto before = count();
            cursor.ReleaseNativeCursorOnOwnerThread();
            workerPreserved = count() == before;
        }};
        worker.join();
        if (!check(workerPreserved && count() == hidden, "foreign callbacks leave both cursor counters with their owning threads")) return false;
        cursor.ReleaseNativeCursorOnOwnerThread();
        if (!check(count() == initial, "the render thread completes a deferred cursor release exactly once")) return false;
        cursor.ReleaseNativeCursorOnOwnerThread();
        if (!check(count() == initial, "repeated cursor release does not force native visibility")) return false;
        ::ShowCursor(FALSE);
        const auto nativeHidden = count();
        cursor.SetNativeCursorHidden(true);
        const auto hand = ::LoadCursor(nullptr, IDC_HAND);
        ::SetCursor(hand);
        cursor.ReleaseNativeCursorOnOwnerThread();
        const bool preserved = count() == nativeHidden && ::GetCursor() == hand;
        ::ShowCursor(TRUE);
        return check(preserved, "releasing XIUI preserves a native hide request and a newer native cursor shape");
    }

    auto text_rasterization_cases(void) -> bool
    {
        using namespace xiui;
        ui::text_rasterizer rasterizer;
        ui::design_system design;
        for (int family = 0; family < ui::font_count; ++family)
        {
            float previousHeight{}, previousAdvance{};
            for (const int pixels : {8, 15, 23, 38})
            {
                const auto metrics = rasterizer.Metrics(family, pixels);
                if (!check(metrics.available && metrics.height > previousHeight, "every bundled font has increasing native size metrics")) return false;
                for (const auto codepoint : {'S', 'W', 'g', 'j'})
                {
                    const auto glyph = rasterizer.Rasterize(family, pixels, codepoint);
                    if (!check(glyph && glyph->advance > 0 && glyph->height > metrics.height, "native glyphs reserve effect padding")) return false;
                    const auto& base = glyph->masks[0];
                    if (!check(std::any_of(base.begin(), base.end(), [](uint8_t alpha) { return alpha > 0 && alpha < 255; }),
                        "all font families and sizes contain antialiased edge coverage")) return false;
                    for (size_t i = 0; i < base.size(); ++i)
                        if (!check(glyph->masks[1][i] >= base[i], "outline preserves base coverage")) return false;
                    if (!check(std::any_of(glyph->masks[2].begin(), glyph->masks[2].end(), [](uint8_t alpha) { return alpha != 0; }),
                        "glow remains available at every size")) return false;
                }
                ui::text_style style; style.font = family; style.scale = pixels / ui::text_base_pixels;
                const auto advance = rasterizer.Advance(family, pixels, 'W');
                if (!check(advance > previousAdvance && design.MeasureText("WWW", style) == advance * 3 && design.TextHeight(style) == metrics.height,
                    "measurement uses the same native pixel size as glyph rasterization")) return false;
                style.tabular_numbers = true;
                if (!check(design.MeasureText("11111", style) == design.MeasureText("88888", style), "stable digits retain equal advances for all fonts")) return false;
                previousHeight = metrics.height; previousAdvance = advance;
            }
            const auto unicode = rasterizer.Rasterize(family, 23, 0x3042);
            if (!check(unicode && unicode->advance > 0 && unicode->ascent > 0, "Unicode fallback retains native size and baseline metrics")) return false;
        }
        float scale = 1;
        ui::slider_value_input input;
        input.Open(TextSizeOption("Size (px)", "", &scale));
        if (!check(input.Editor().Text() == "15", "font size controls display pixels")) return false;
        input.Insert("23");
        if (!check(input.Value() && ui::TextPixelSize(*input.Value()) == 23, "typed pixel size maps to the saved scale")) return false;
        input.Open(module_option::Slider("Ordinary", "", &scale, 0, 10, 0.1f)); input.Insert("2.5");
        return check(input.Value() && *input.Value() == 2.5f, "ordinary sliders retain their units");
    }

    auto enemy_cases(void) -> bool
    {
        std::array<uint8_t, 80> packet{};
        uint32_t offset = 40;
        const auto pack = [&](uint32_t value, uint8_t bits) {
            offset = Ashita::BinaryData::PackBitsBE(packet.data(), value, offset, bits);
        };
        pack(0x01065022, 32); pack(2, 6); pack(0, 4); pack(8, 4); pack(0x68776163, 32); pack(0, 32);
        for (uint32_t target = 0; target < 2; ++target)
        {
            pack(target == 0 ? 0x01065030 : 0x12345678, 32); pack(1, 4);
            pack(0, 5); pack(0, 12); pack(0, 7); pack(0, 3); pack(144 + target, 17); pack(1, 10); pack(0, 31);
            pack(target == 0 ? 1 : 0, 1);
            if (target == 0) { pack(3, 10); pack(42, 17); pack(160, 10); }
            pack(target == 0 ? 1 : 0, 1);
            if (target == 0) { pack(5, 10); pack(10, 14); pack(44, 10); }
        }
        const size_t size = (offset + 7) / 8;
        const auto action = xiui::DecodeCombatAction({packet.data(), size});
        if (!check(action && action->actor == 0x01065022 && action->category == 8 && action->parameter == 0x68776163 &&
            action->target_count == 2 && action->targets[0].parameter == 144 && action->targets[1].id == 0x12345678 &&
            action->targets[1].parameter == 145, "enemy actions retain every target across additional/spikes effect blocks")) return false;
        if (!check(!xiui::DecodeCombatAction({packet.data(), size - 1}) && !xiui::DecodeCombatAction({}),
            "truncated enemy actions cannot produce partial tracking events")) return false;
        xiui::enemy_entry cast{};
        cast.cast_name = "Stone"; cast.cast_seconds = 2; cast.cast_started = 1000;
        if (!check(cast.HasCast(4999) && !cast.HasCast(5000), "stale enemy casts expire after their grace period")) return false;
        cast.interrupted_at = 2000;
        if (!check(cast.HasCast(2999) && !cast.HasCast(3000), "interrupt indicators expire independently of cast duration")) return false;
        const xiui::ui::config_layout layout{{0, 0, 680, 350}, 7};
        const auto last = layout.ModuleTab(6), reset = layout.ProfileSelector();
        if (!check(last.y + last.height <= reset.y, "seven sidebar tabs cannot overlap profile controls at minimum height")) return false;

        xiui::enemy_list_module module;
        module.ResetDefaults(xiui::ui::theme::CreateDefault());
        xiui::hud_snapshot snapshot{};
        if (!check(!module.ShouldRender(snapshot), "an empty enemy list stays hidden")) return false;
        xiui::ui::design_system design;
        const xiui::module_render_context context{design, nullptr, true};
        module.Render(snapshot, context);
        if (!check(module.State().bounds.height > 180 && module.State().bounds.width == 220,
            "edit samples lay out three independent cards without changing card width")) return false;
        const float withTargets = module.State().bounds.height;
        *module.Option(3).toggle = false;
        module.Render(snapshot, context);
        if (!check(module.State().bounds.height == withTargets, "enemy targets share the name row without adding a line")) return false;
        *module.Option(8).slider = 1;
        module.Render(snapshot, context);
        return check(module.State().bounds.height < 150, "visible-enemy limit also controls edit preview bounds");
    }

    auto meter_cases(const char* dat) -> bool
    {
        using namespace xiui;
        const resources::menu_texture solid{2, 2, std::vector<uint32_t>(4, 0x80406080)};
        const auto scaled = resources::UpscaleMenuTexture(solid, 4, true);
        if (!check(scaled && scaled->width == 8 && scaled->height == 8 &&
            std::all_of(scaled->pixels.begin(), scaled->pixels.end(), [](const auto pixel) { return pixel == 0x80406080; }),
            "higher resolution bar textures preserve solid theme colors and opacity")) return false;
        const auto transparent = resources::UpscaleMenuTexture({2, 1, {0xFFFF0000, 0x000000FF}}, 4);
        if (!check(transparent && std::all_of(transparent->pixels.begin(), transparent->pixels.end(), [](const auto pixel) { return (pixel & 0xFFFF) == 0; }),
            "smooth texture scaling cannot bleed colors from transparent source pixels")) return false;
        const resources::menu_texture seam{2, 1, {0xFFFF0000, 0xFF0000FF}};
        const auto tiled = resources::UpscaleMenuTexture(seam, 4, true), clamped = resources::UpscaleMenuTexture(seam, 4);
        if (!check(tiled && clamped && (tiled->pixels.front() & 255) != 0 && (clamped->pixels.front() & 255) == 0 &&
            !resources::UpscaleMenuTexture({}, 4) && !resources::UpscaleMenuTexture(seam, 0),
            "theme backgrounds interpolate across tile seams while gauge pieces retain clamped edges")) return false;
        resources::menu_texture atlas{8, 8, std::vector<uint32_t>(64, 0xFFFF00FF)};
        atlas.pixels[18] = 0xFFFF0000; atlas.pixels[19] = 0xFF00FF00;
        atlas.pixels[26] = 0xFF0000FF; atlas.pixels[27] = 0xFFFFFFFF;
        const auto patch = resources::ExtractMenuTexture(atlas, 2, 2, 2, 2);
        if (!check(patch && patch->width == 4 && patch->height == 4 && patch->pixels[0] == 0xFFFF0000 &&
            patch->pixels[3] == 0xFF00FF00 && patch->pixels[12] == 0xFF0000FF && patch->pixels[15] == 0xFFFFFFFF &&
            std::find(patch->pixels.begin(), patch->pixels.end(), 0xFFFF00FF) == patch->pixels.end(),
            "isolated gauge pieces retain all edge pixels and exclude neighboring atlas colors")) return false;
        if (!check(!resources::ExtractMenuTexture(atlas, 7, 0, 2, 2) && !resources::ExtractMenuTexture(atlas, 0, 0, 0, 2),
            "invalid gauge regions are rejected before copying")) return false;
        const ui::meter_skin skin;
        const auto full = ui::LayoutMeterSkin({10, 20, 200, 16}, skin);
        const auto narrow = ui::LayoutMeterSkin({10, 20, 4, 16}, skin);
        const auto empty = ui::LayoutMeterSkin({10, 20, -1, 0}, skin);
        const auto uncapped = ui::LayoutMeterSkin({10, 20, 200, 16}, skin, false);
        const auto thin = ui::LayoutMeterSkin({10, 20, 1, 1}, skin, false);
        if (!check(full.frame[0].width == 8 && full.frame[2].x == 202 && full.content.x == 19 && full.content.y == 22 &&
            full.content.width == 182 && full.content.height == 12 && narrow.content.width == 0 && empty.content.width == 0 &&
            full.frame[1].x - (full.frame[0].x + full.frame[0].width) == 1 &&
            full.frame[2].x - (full.frame[1].x + full.frame[1].width) == 1,
            "native caps keep their proportions with one pixel of clearance on each side of the body")) return false;
        if (!check(uncapped.frame[0].width == 0 && uncapped.frame[2].width == 0 && uncapped.frame[1].x == 10 &&
            uncapped.frame[1].width == 200 && uncapped.content.x == 10 && uncapped.content.width == 200 &&
            uncapped.content.y == 22 && uncapped.content.height == 12 && thin.content.width == 1 && thin.content.height > 0,
            "uncapped themed bars reclaim both caps and gaps while retaining the native vertical inset")) return false;

        const auto theme = ui::theme::CreateDefault();
        player_module player;
        player.ResetDefaults(theme);
        auto appearance = player.Option(9);
        if (!check(appearance.choice_count == 2 && *appearance.choice == ui::meter_style::window_theme,
            "shared bars default to Window theme and expose Modern")) return false;
        *appearance.choice = ui::meter_style::modern;
        player.OnOptionChanged(9);
        if (!check(player.Settings().hp_style.appearance == ui::meter_style::modern &&
            player.Settings().mp_style.appearance == ui::meter_style::modern && player.Settings().tp_style.appearance == ui::meter_style::modern,
            "the shared Player style updates HP, MP and TP together")) return false;
        player.Settings().style_edit_mode = 1; player.Settings().selected_bar = 1;
        *player.Option(9).choice = ui::meter_style::window_theme;
        player.OnOptionChanged(9);
        if (!check(player.Settings().mp_style.appearance == ui::meter_style::window_theme && player.Settings().hp_style.appearance == ui::meter_style::modern,
            "individual bar styling keeps each bar's choice independent")) return false;
        auto style = theme.meter;
        if (!check(MeterStyleOption(2, style).available && *MeterStyleOption(2, style).toggle &&
            !MeterStyleOption(3, style).visible, "themed bars expose bookends without enabling modern geometry controls")) return false;
        *MeterStyleOption(2, style).toggle = false;
        style.appearance = ui::meter_style::modern;
        *MeterStyleOption(2, style).toggle = true;
        if (!check(style.bookends && !style.themed_bookends && !MeterStyleOption(3, style).visible,
            "each bar style keeps its own bookend preference")) return false;
        for (const float height : {1.0f, 8.0f, 32.0f})
        {
            style.height = height;
            const auto track = ui::LayoutMeterTrack({10, 20, 200, 40}, style);
            if (!check(track.height == height && track.y + track.height * 0.5f == 40,
                "bar thickness follows its pixel setting independently of the reserved text row")) return false;
        }
        style.height = 0;
        if (!check(ui::LayoutMeterTrack({10, 20, 200, 18}, style).height == 16,
            "automatic bar height retains the existing layout fallback")) return false;

        const auto path = std::filesystem::temp_directory_path() / ("xiuinative-bar-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
        struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
        const settings_store store{path.string()};
        style.corner_radius = 13; style.bookends = true; style.animation_speed = 17; style.sheen_speed = 0.4f;
        for (const int appearanceValue : {ui::meter_style::window_theme, ui::meter_style::modern})
        {
            style.appearance = appearanceValue;
            SaveMeterStyle(store, "meter.test", style);
            if (!check(store.Flush(), "bar preferences save")) return false;
            const auto restored = LoadMeterStyle(settings_store{path.string()}, "meter.test", theme.meter);
            if (!check(restored.appearance == appearanceValue && restored.bookends && !restored.themed_bookends && restored.corner_radius == 13 &&
                restored.animation_speed == 17 && restored.sheen_speed == 0.4f,
                "both bar styles preserve custom modern geometry and animation settings across reloads")) return false;
        }
        store.WriteInt("legacy", "style_version", 4); store.WriteFloat("legacy", "corner_radius", 13);
        const auto legacy = LoadMeterStyle(store, "legacy", theme.meter);
        if (!check(legacy.appearance == ui::meter_style::window_theme && legacy.themed_bookends && legacy.corner_radius == 13,
            "older bar settings adopt the theme without replacing their custom geometry")) return false;

        if (dat == nullptr) return true;
        std::ifstream file{dat, std::ios::binary};
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
        bool haveTexture{}, haveFrame{};
        for (size_t at = 0; at + 32 <= bytes.size();)
        {
            uint32_t meta{}; std::memcpy(&meta, bytes.data() + at + 4, 4);
            const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
            if (size < 16 || size > bytes.size() - at) break;
            const auto payload = std::span{bytes}.subspan(at + 16, size - 16);
            if ((meta & 0x7F) == 0x20 && payload.size() >= 69 && std::string_view{reinterpret_cast<const char*>(payload.data() + 9), 8} == "gauge   ")
            {
                const auto image = resources::DecodeMenuTexture(payload);
                if (!check(image && image->width == 64 && image->height == 64, "native gauge texture decodes")) return false;
                const auto cap = resources::ExtractMenuTexture(*image, 0, 8, 4, 8);
                if (!check(cap && cap->width == 8 && cap->height == 16 && cap->pixels[2] == image->pixels[8 * 64 + 1] &&
                    cap->pixels[2] != image->pixels[7 * 64 + 1] && cap->pixels[cap->width + 2] == cap->pixels[2],
                    "native cap padding repeats its own top row rather than the bright atlas strip above it")) return false;
                haveTexture = true;
            }
            if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(payload.data()), 16} == "menu    windowps")
            {
                const auto frame = resources::DecodeMenuSprite(payload, 38), fill = resources::DecodeMenuSprite(payload, 97);
                if (!check(frame && fill && frame->front().size() == 3 && fill->front().size() == 1,
                    "native meter frame and fill groups decode")) return false;
                const auto& caps = frame->front(); const auto& strip = fill->front().front();
                if (!check(caps[0].width == 4 && caps[0].height == 8 && caps[0].u == 0 && caps[0].v == 8 && caps[1].u == 4 &&
                    caps[2].width == 64 && caps[2].height == 8 && caps[2].additive && strip.width == 64 && strip.height == 6 && strip.v == 1,
                    "native gauge artwork retains end caps, frame, fill inset and blending")) return false;
                haveFrame = true;
            }
            at += size;
        }
        return check(haveTexture && haveFrame, "installed DAT contains native meter artwork");
    }

    auto meter_text_cases(void) -> bool
    {
        const xiui::ui::meter_skin skin;
        const xiui::ui::rect themedBar{10, 20, 200, 20};
        const auto themed = xiui::ui::LayoutMeterText(themedBar, 16, 16, 32,
            xiui::ui::meter_text_anchor::automatic, xiui::ui::meter_text_anchor::automatic, &skin);
        if (!check(themed.label.x == 25 && themed.value.x + themed.value.width == 195 && themed.label.y == 12 && themed.value.y == 20 &&
            themed.placement.x == 21 && themed.placement.width == 178,
            "themed labels straddle the top edge while labels and values stay inset beyond the caps")) return false;
        const auto uncapped = xiui::ui::LayoutMeterText(themedBar, 16, 16, 32,
            xiui::ui::meter_text_anchor::automatic, xiui::ui::meter_text_anchor::automatic, &skin, false);
        if (!check(uncapped.placement.x == 10 && uncapped.placement.width == 200 && uncapped.label.x == 14 &&
            uncapped.value.x + uncapped.value.width == 206 && uncapped.label.y == themed.label.y,
            "uncapped labels and values use the full bar with normal text padding and top-edge placement")) return false;
        const auto tall = xiui::ui::LayoutMeterText({10, 20, 200, 40}, 16, 16, 32,
            xiui::ui::meter_text_anchor::top, xiui::ui::meter_text_anchor::bottom, &skin);
        if (!check(tall.label.x == 35 && tall.value.x + tall.value.width == 185 && tall.label.y == 12 && tall.value.y == 52,
            "themed text clearance scales with cap size and retains explicit above and below anchors")) return false;
        const auto narrow = xiui::ui::LayoutMeterText({10, 20, 4, 20}, 16, 16, 32,
            xiui::ui::meter_text_anchor::automatic, xiui::ui::meter_text_anchor::automatic, &skin);
        if (!check(narrow.label.width == 0 && narrow.value.width == 0 && narrow.label.x == 12 && narrow.value.x == 12,
            "narrow themed bars do not allocate text over their caps")) return false;
        const xiui::ui::rect bar{10, 20, 200, 10};
        const auto player = xiui::ui::LayoutMeterText(bar, 16, 16, 32);
        if (!check(player.label.x == 14 && player.label.y == 12 && player.value.y == 20,
            "player meter text keeps its existing placement")) return false;
        const auto target = xiui::ui::LayoutMeterText(bar, 16, 16, 32, xiui::ui::meter_text_anchor::bottom);
        if (!check(target.label.y == target.value.y && target.label.y == 22 &&
            target.value.x + target.value.width == 206, "target HP and percent share the lower row with an inset right edge")) return false;
        const auto above = xiui::ui::LayoutMeterText(bar, 16, 16, 32, xiui::ui::meter_text_anchor::top,
            xiui::ui::meter_text_anchor::top);
        const auto below = xiui::ui::LayoutMeterText(bar, 16, 16, 32, xiui::ui::meter_text_anchor::top,
            xiui::ui::meter_text_anchor::bottom);
        return check(above.value.x == 14 && above.value.x + above.value.width == 206 &&
            above.value.y == 12 && below.value.y == 22 && above.label.y == below.label.y &&
            above.value.x == below.value.x && above.value.width == below.value.width,
            "EXP info uses the bar inset at either edge without moving its independent label");
    }

    auto text_profile_cases(const xiui::settings_store& store) -> bool
    {
        auto profile = xiui::module_text_profile{xiui::ui::theme::CreateDefault().meter_text};
        profile.style.top_color = 0x80FFFFFF;
        profile.style.bottom_color = 0x40505050;
        profile.style.glow.enabled = true;
        profile.offset_x = 3; profile.offset_y = -2; profile.placement = 3;
        const auto tinted = xiui::TintTextProfile(profile, 0xFFFF8080);
        if (!check(tinted.style.top_color == 0x80FF8080 && tinted.style.bottom_color == 0x40502828 &&
            tinted.style.shadow.enabled && tinted.style.stroke.enabled && tinted.style.glow.enabled &&
            tinted.offset_x == 3 && tinted.offset_y == -2 && tinted.placement == 3 && profile.style.top_color == 0x80FFFFFF,
            "state tints retain gradients, alpha, effects, placement, and the original profile")) return false;

        const auto theme = xiui::ui::theme::CreateDefault();
        const std::array roles{theme.title_text, theme.meter_text, theme.caption_text, theme.meter_text};
        constexpr size_t textBase = 18 + xiui::meter_style_option_count;
        xiui::enemy_list_module legacy;
        legacy.ResetDefaults(theme);
        for (int i = 0; i < 4; ++i)
        {
            *legacy.Option(6).choice = i;
            if (!check(*legacy.Option(textBase + 7).toggle == roles[i].shadow.enabled &&
                *legacy.Option(textBase + 10).toggle == roles[i].stroke.enabled &&
                *legacy.Option(textBase + 6).color == roles[i].bottom_color,
                "enemy text defaults inherit their normal theme roles")) return false;
            *legacy.Option(textBase + 1).slider = i == 0 ? 0.9f : 0.75f;
            *legacy.Option(textBase + 2).slider = static_cast<float>(i);
            *legacy.Option(textBase + 5).color = *legacy.Option(textBase + 6).color = 0xFFFFFDF8;
            *legacy.Option(textBase + 7).toggle = *legacy.Option(textBase + 10).toggle = *legacy.Option(textBase + 12).toggle = false;
        }
        legacy.Save(store);
        store.WriteInt("enemylist", "text_style_version", 0);
        store.Flush();
        xiui::enemy_list_module upgraded;
        upgraded.ResetDefaults(theme);
        upgraded.Load(store);
        for (int i = 0; i < 4; ++i)
        {
            *upgraded.Option(6).choice = i;
            if (!check(*upgraded.Option(textBase + 7).toggle == roles[i].shadow.enabled &&
                *upgraded.Option(textBase + 10).toggle == roles[i].stroke.enabled &&
                *upgraded.Option(textBase + 6).color == roles[i].bottom_color &&
                *upgraded.Option(textBase + 1).slider == (i == 0 ? 0.9f : 0.75f) &&
                *upgraded.Option(textBase + 2).slider == static_cast<float>(i),
                "legacy plain profiles regain normal styling without changing size or offsets")) return false;
        }
        *upgraded.Option(6).choice = 1;
        *upgraded.Option(textBase + 5).color = *upgraded.Option(textBase + 6).color = 0x80445566;
        *upgraded.Option(textBase + 7).toggle = *upgraded.Option(textBase + 10).toggle = false;
        upgraded.Save(store);
        store.Flush();
        xiui::enemy_list_module customized;
        customized.ResetDefaults(theme);
        customized.Load(store);
        return check(*customized.Option(textBase + 5).color == 0x80445566 &&
            !*customized.Option(textBase + 7).toggle && !*customized.Option(textBase + 10).toggle,
            "later styling choices persist without another default migration");
    }

    template<typename Module, typename Configure>
    auto persistence(const xiui::settings_store& store, int contexts, Configure configure, size_t& checked) -> bool
    {
        const auto theme = xiui::ui::theme::CreateDefault();
        for (int context = 0; context < contexts; ++context)
        {
            for (int pattern = 0; pattern < 3; ++pattern)
            {
                Module source{}, loaded{};
                source.ResetDefaults(theme);
                loaded.ResetDefaults(theme);
                configure(source, context);
                for (size_t i = 0; i < source.OptionCount(); ++i)
                {
                    auto option = source.Option(i);
                    if (option.kind != xiui::module_option_kind::toggle || option.toggle == nullptr) continue;
                    *option.toggle = pattern == 1 || (pattern == 2 && i % 2 == 0);
                    source.OnOptionChanged(i);
                }
                source.Save(store);
                store.Flush();
                loaded.Load(store);
                for (size_t i = 0; i < source.OptionCount(); ++i)
                {
                    auto before = source.Option(i), after = loaded.Option(i);
                    if (before.kind == xiui::module_option_kind::choice && before.choice != nullptr)
                    {
                        if (!check(after.choice != nullptr && *before.choice == *after.choice, "selected profile and info-position choices persist")) return false;
                    }
                    if (before.kind != xiui::module_option_kind::toggle || before.toggle == nullptr) continue;
                    if (after.toggle == nullptr || *before.toggle != *after.toggle)
                    {
                        std::fprintf(stderr, "%s: %s did not persist (context %d, pattern %d).\n", source.Id(), before.label, context, pattern);
                        return false;
                    }
                    ++checked;
                }
            }
        }
        return true;
    }
}

namespace xiui
{
    struct config_input_test
    {
        static auto StartupWiring(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-startup-wiring-" + std::to_string(::GetCurrentProcessId()));
            std::filesystem::create_directories(root);
            struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } files{root};
            auto runtime = std::make_unique<hud_runtime>();
            runtime->Initialize(nullptr, root.string().c_str());
            if (!check(runtime->player_.timing_ == &runtime->timing_source_ && runtime->player_.sounds_ == &runtime->ui_sounds_ &&
                runtime->target_.Settings().world_bar.healing == &runtime->heal_prediction_source_ && runtime->target_.timing_ == &runtime->timing_source_ && runtime->enemy_list_.timing_ == &runtime->timing_source_,
                "real startup retains timing, healing and sound dependencies")) return false;
            for (auto* module : runtime->modules_.All())
            {
                const auto original = module->ConfigView();
                for (size_t view = 0; view < std::max(size_t{1}, module->ConfigViews().size()); ++view)
                {
                    module->SelectConfigView(view);
                    std::set<std::string> ids;
                    for (const auto& topic : module->ConfigSections())
                        if (!check(!topic.Id().empty() && ids.insert(std::string{topic.Id()}).second, "every settings view has unique stable topic identities")) return false;
                }
                module->SelectConfigView(original);
            }
            ui::design_system design;
            const auto style = ui::theme::CreateDefault().body_text;
            const std::string letter = "\xC3\xA9";
            const auto fitted = ui::FitText(design, letter + letter + letter + letter, design.MeasureText((letter + "...").c_str(), style), style);
            if (!check(fitted == letter + "...", "shared text fitting keeps whole UTF-8 characters")) return false;
            runtime->FlushSettings();
            runtime->Shutdown();
            return true;
        }

        static auto TreasureInput(void) -> bool
        {
            hud_runtime runtime;
            runtime.visible_ = true;
            runtime.editing_ = false;
            runtime.treasure_.Open();
            if (!check(runtime.TreasureInputAvailable(), "a visible forced pool can be closed even when empty")) return false;
            runtime.visible_ = false;
            if (!check(!runtime.TreasureInputAvailable(), "hidden XIUI suspends treasure input")) return false;
            runtime.visible_ = true;
            runtime.editing_ = true;
            if (!check(!runtime.TreasureInputAvailable(), "edit previews cannot take treasure actions")) return false;
            runtime.editing_ = false;
            runtime.native_ui_.menu_visible = true;
            if (!check(!runtime.TreasureInputAvailable(), "native menus retain treasure input ownership")) return false;
            runtime.native_ui_.menu_visible = false;
            for (auto* chat : {&runtime.chat_, &runtime.chat_second_})
            {
                chat->State().enabled = true;
                if (!check(chat->BeginControllerHistory(), "chat history can be focused for input ownership")) return false;
                chat->HandleControllerAction(controller_action::confirm, false, runtime.design_system_);
                if (!check(chat->HistoryExpanded() && !runtime.TreasureInputAvailable(), "either expanded chat window owns input ahead of treasure")) return false;
                chat->Focus(false);
            }
            if (!check(runtime.TreasureInputAvailable(), "closing expanded chat restores treasure availability")) return false;
            runtime.treasure_.HandleMouse(WM_LBUTTONDOWN, 1, 1);
            runtime.native_ui_.menu_visible = true;
            if (!check(runtime.HandleTreasureMouse(WM_LBUTTONDOWN, 1, 1, true) && runtime.treasure_.PointerDown(), "blocked treasure gestures retain only release ownership")) return false;
            if (!check(runtime.HandleTreasureMouse(WM_LBUTTONUP, 1, 1, true) && !runtime.treasure_.PointerDown(), "blocked treasure gestures drain their release")) return false;
            runtime.native_ui_.menu_visible = false;
            runtime.treasure_key_release_[VK_ESCAPE] = true;
            if (!check(runtime.HandleTreasureKey(VK_ESCAPE, 0x40000000u, false), "held Cancel stays consumed after leaving treasure focus")) return false;
            if (!check(runtime.HandleTreasureKey(VK_ESCAPE, 0xC0000000u, false) && !runtime.treasure_key_release_[VK_ESCAPE], "the owned Cancel release drains once")) return false;
            if (!check(!runtime.HandleTreasureKey(VK_ESCAPE, 0, false), "a fresh unfocused Cancel returns to gameplay")) return false;
            runtime.treasure_.State().enabled = false;
            return check(!runtime.TreasureInputAvailable(), "disabled treasure cannot capture input");
        }

        static auto ControlCount(hud_runtime& runtime) -> size_t
        {
            const auto slots = runtime.GroupOptionSlots();
            return static_cast<size_t>(std::count_if(slots.begin(), slots.end(), [&](const size_t index) { return index < runtime.active_module_->OptionCount(); }));
        }

        static auto PartyNameCast(void) -> bool
        {
            party_module party;
            party.ApplyPreset(0);
            party.State().bounds.width = 360;
            party.job_icons_ = true; party.icon_size_ = 20; party.icon_follow_name_ = false;
            party.cast_placement_ = party_module::cast_name;
            party.text_[0].offset_x = 44; party.text_[0].offset_y = 6;
            party.text_[party_module::tp_text].offset_x = 37;
            const auto separateIcon = party.LayoutContainer();
            party.icon_follow_name_ = true;
            const auto sharedIcon = party.LayoutContainer();
            if (!check(separateIcon.width == sharedIcon.width && sharedIcon.canvas_width == separateIcon.canvas_width,
                "job icon anchoring does not allocate horizontal entry space")) return false;
            const ui::rect row{0, 0, sharedIcon.canvas_width, 0};
            const auto entry = party.LayoutEntry(row, true, {true, true, true, true});
            ui::design_system design;
            module_render_context context{design, nullptr, true};
            for (int font = 0; font < ui::font_count; ++font)
            {
                auto& profile = party.text_[party_module::tp_text];
                profile.style.font = font; profile.style.scale = 0.867f;
                for (const int placement : {0, 1, 2, 3})
                {
                    profile.placement = placement;
                    const auto number = party.TPNumberBounds({0, 0, 28, 20});
                    const auto visible = context.TextBounds("3000", number, number, profile);
                    if (!check(visible.width == design.MeasureText("3000", profile.style),
                        "default/left/center/right TP placement retains all four digits after text padding")) return false;
                }
            }
            party.text_[party_module::tp_text].style.font = 0;
            party.text_[party_module::tp_text].placement = 0;
            for (const int placement : {0, 1, 2, 3})
            {
                party.text_[0].placement = placement;
                const auto name = context.TextBounds("White Mage", entry.header, entry.header, party.text_[0]);
                const auto area = party.NameCastArea(entry.header, name.x);
                const auto cast = party.CastBounds(area);
                if (!check(cast.x == name.x && cast.y == entry.header.y + 6 && cast.width >= 0,
                    "replacement casting starts at the actual name anchor, including offsets and alignment")) return false;
            }
            party.text_[0].placement = 0;
            const auto name = context.TextBounds("White Mage", entry.header, entry.header, party.text_[0]);
            if (!check(party.JobIconBounds(entry, name, 0).x >= row.x &&
                party.LayoutEntry(row, false, {}).zone.x >= row.x && entry.resources.bars[0].width == 152 &&
                entry.resources.bars[1].width == 92 && entry.resources.bars[2].width == 28,
                "fitted icons, out-of-zone panels and unchanged HP/MP/TP widths stay inside the compact row")) return false;
            party.bar_offsets_x_[3] = 9; party.bar_offsets_y_[3] = 3;
            const auto custom = party.CastBounds(party.NameCastArea(entry.header, name.x));
            if (!check(custom.x == name.x + 9 && custom.y == entry.header.y + 9,
                "independent cast offsets remain relative to the name anchor")) return false;
            auto legacy = settings_store::FromIniText("");
            party.ApplyPreset(1); party.bar_offsets_x_[3] = 4; party.bar_offsets_y_[3] = 8;
            party.Save(legacy); legacy.WriteInt("party", "name_cast_anchor_version", 0);
            party_module migrated; migrated.Load(legacy);
            if (!check(migrated.text_[0].offset_x == 4 && migrated.text_[0].offset_y == 8 &&
                migrated.bar_offsets_x_[3] == 0 && migrated.bar_offsets_y_[3] == 0,
                "old XIUI preset offsets migrate once without applying the name offset twice")) return false;
            migrated.Save(legacy); legacy.WriteFloat("meter.party.cast", "offset_x", 9);
            party_module customized; customized.Load(legacy);
            if (!check(customized.bar_offsets_x_[3] == 9, "later custom cast offsets survive reload")) return false;
            for (const bool follow : {false, true})
            {
                party.icon_follow_name_ = follow;
                party.icon_offset_x_ = party.icon_offset_y_ = 0;
                const auto before = party.LayoutEntry(row, true, {true, true, true, true, true});
                const auto icon = party.JobIconBounds(before, name, 0);
                party.icon_offset_x_ = 12; party.icon_offset_y_ = -6;
                const auto after = party.LayoutEntry(row, true, {true, true, true, true, true});
                const auto moved = party.JobIconBounds(after, name, 0);
                if (!check(moved.x == icon.x + 12 && moved.y == icon.y - 6 && before.height == after.height &&
                    before.header.y == after.header.y && before.resources.bars[0].y == after.resources.bars[0].y,
                    "fixed and name-following icon offsets move only the icon, preserving entry height and bars")) return false;
            }
            for (const int placement : {party_module::cast_separate, party_module::cast_name, party_module::cast_mp, party_module::cast_tp})
            {
                party.cast_placement_ = placement; party.text_[5].offset_y = 0;
                const auto before = party.LayoutEntry(row, true, {true, true, true, true, true});
                const auto label = context.TextBounds("Cure IV", before.header, before.header, party.text_[5]);
                for (const float offset : {-6.0f, 12.0f})
                {
                    party.text_[5].offset_y = offset;
                    const auto after = party.LayoutEntry(row, true, {true, true, true, true, true});
                    const auto moved = context.TextBounds("Cure IV", after.header, after.header, party.text_[5]);
                    if (!check(before.height == after.height && before.header.y == after.header.y &&
                        before.resources.bars[0].y == after.resources.bars[0].y && moved.y == label.y + offset,
                        "cast text offsets move its label without changing entry height or resource positions in any cast mode")) return false;
                }
            }
            auto offsets = settings_store::FromIniText("");
            party.Save(offsets);
            party_module alliance{1}; alliance.icon_offset_x_ = -9; alliance.icon_offset_y_ = 4; alliance.Save(offsets);
            party_module restoredParty, restoredAlliance{1}; restoredParty.Load(offsets); restoredAlliance.Load(offsets);
            if (!check(restoredParty.icon_offset_x_ == 12 && restoredParty.icon_offset_y_ == -6 &&
                restoredAlliance.icon_offset_x_ == -9 && restoredAlliance.icon_offset_y_ == 4 &&
                restoredParty.text_[5].offset_y == 12 &&
                party.ConfigSectionContains(party_module::jobs_section, party_module::icon_offset_options_start) &&
                party.ConfigSectionContains(party_module::jobs_section, party_module::icon_offset_options_start + 1),
                "Party and alliance icon offsets persist independently in Jobs, along with cast text offsets")) return false;
            party.ApplyPreset(0);
            if (!check(party.icon_offset_x_ == 9 && party.icon_offset_y_ == 1, "Classic restores captured icon offsets")) return false;
            ui::player_timing_settings timing;
            if (!check(timing.attack_layout == 2 && timing.recovery_layout == 2 && timing.mini_gap == 2 && timing.recovery_mini_gap == 2,
                "Player tickers default to Mini bar with two-pixel gaps")) return false;
            timing.Load(settings_store::FromIniText(""));
            if (!check(timing.attack_layout == 2 && timing.recovery_layout == 2 && timing.mini_gap == 2 && timing.recovery_mini_gap == 2,
                "missing timer settings use the same Mini bar defaults")) return false;
            std::puts("Party name/cast alignment, visual-only icon/cast offsets, independent persistence, Classic gutter and Player mini-ticker defaults passed.");
            return true;
        }

        static auto SettingsPresentation(const char* reportPath) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {20, 20, 1100, 600};
            for (auto* module : runtime.modules_.All()) module->Option(0);
            const auto originalSettings = runtime.CaptureSettings().Preferences("").ToIniText();
            std::ostringstream report;
            report << "# Settings presentation audit\n\nAudited October 15, 2026. Each bullet is one visual row; [empty] is an empty cell kept for a related pair. Disabled controls retain their normal position.\n\n";
            size_t pages{}, controls{}, variants{};
            const auto audit = [&](const std::string& title, std::set<std::string>& seen) {
                const auto slots = runtime.GroupOptionSlots();
                std::set<size_t> covered;
                std::ostringstream rows;
                for (size_t at = 0; at < slots.size(); ++at)
                {
                    const auto index = slots[at];
                    if (at % 2 == 0) rows << "- "; else rows << " | ";
                    if (index >= runtime.active_module_->OptionCount()) rows << "[empty]";
                    else
                    {
                        if (!check(covered.insert(index).second, "presentation never duplicates an option")) return false;
                        const auto option = runtime.active_module_->Option(index);
                        if (!check(runtime.MatchesConfigOption(index) && option.selector == module_selector::none,
                            "presentation retains original IDs and current section membership")) return false;
                        rows << option.label;
                        if (option.pair_next && at + 1 < slots.size() && slots[at + 1] < runtime.active_module_->OptionCount() &&
                            runtime.active_module_->Option(slots[at + 1]).display_order == option.display_order + 1)
                            if (!check(at % 2 == 0, "related pairs share the left and right cells of one row")) return false;
                    }
                    if (at % 2 == 1 || at + 1 == slots.size()) rows << '\n';
                }
                for (size_t index = 0; index < runtime.active_module_->OptionCount(); ++index)
                    if (runtime.MatchesConfigOption(index) && runtime.active_module_->Option(index).selector == module_selector::none)
                        if (!check(covered.contains(index), "presentation does not hide existing settings")) return false;
                if (seen.insert(rows.str()).second)
                {
                    report << "### " << title << "\n\n" << rows.str() << '\n';
                    ++pages; controls += covered.size();
                }
                ++variants;
                return true;
            };
            for (auto* module : runtime.modules_.All())
            {
                runtime.active_module_ = module;
                const auto originalView = module->ConfigView();
                for (size_t view = 0; view < std::max<size_t>(1, module->ConfigViews().size()); ++view)
                {
                    module->SelectConfigView(view);
                    report << "## " << module->DisplayName();
                    if (!module->ConfigViews().empty()) report << " / " << module->ConfigViews()[view];
                    report << "\n\n";
                    for (size_t section = 0; section < std::max<size_t>(1, module->ConfigSections().size()); ++section)
                    {
                        runtime.active_config_section_ = section;
                        runtime.active_option_group_ = module_option_group::general;
                        const auto title = module->ConfigSections().empty() ? "General" : module->ConfigSections()[section].name;
                        std::set<std::string> seen;
                        const auto run = [&]() {
                            static constexpr const char* displayTabs[]{"Bar info", "Readouts", "Layout"};
                            const auto pageTitle = std::string{title} + (module == &runtime.general_ && section == general_module::appearance_section ?
                                std::string{" / "} + general_module::appearance_tabs[runtime.general_.AppearanceTab()] : "");
                            const auto viewTitle = pageTitle + (module == &runtime.experience_ && std::strcmp(title, "Display") == 0 ?
                                std::string{" / "} + displayTabs[static_cast<size_t>(runtime.experience_display_.View())] : "");
                            if (!audit(viewTitle, seen)) return false;
                            for (size_t index = 0; index < module->OptionCount(); ++index)
                            {
                                if (!runtime.MatchesConfigOption(index)) continue;
                                const auto option = module->Option(index);
                                const bool selector = option.selector != module_selector::none ||
                                    (option.label != nullptr && (std::strcmp(option.label, "Status order") == 0 || std::strcmp(option.label, "Native filter group") == 0));
                                if (!selector || option.choice == nullptr) continue;
                                const auto original = *option.choice;
                                for (size_t choice = 0; choice < option.choice_count; ++choice)
                                {
                                    *option.choice = static_cast<int>(choice);
                                    if (!audit(pageTitle + " / " + option.choices[choice], seen)) return false;
                                }
                                *option.choice = original;
                            }
                            return true;
                        };
                        if (module == &runtime.general_ && section == general_module::appearance_section)
                        {
                            for (size_t tab = 0; tab < std::size(general_module::appearance_tabs); ++tab)
                            {
                                runtime.general_.SelectAppearanceTab(tab);
                                if (!run()) return false;
                            }
                            runtime.general_.SelectAppearanceTab(0);
                        }
                        else if (module == &runtime.experience_ && std::strcmp(title, "Display") == 0)
                        {
                            for (int tab = 0; tab < 3; ++tab)
                            {
                                runtime.experience_display_.SelectView(static_cast<experience_display_view>(tab));
                                if (!run()) return false;
                            }
                            runtime.experience_display_.SelectView(experience_display_view::bar_info);
                        }
                        else if (!run()) return false;
                    }
                }
                module->SelectConfigView(originalView);
            }
            if (!check(runtime.CaptureSettings().Preferences("").ToIniText() == originalSettings,
                "browsing every reordered group preserves all saved settings and positions")) return false;
            runtime.active_module_ = &runtime.party_;
            runtime.active_config_section_ = party_module::layout_section;
            const auto partySlots = runtime.GroupOptionSlots();
            const auto at = [&](const size_t index) { return static_cast<size_t>(std::find(partySlots.begin(), partySlots.end(), index) - partySlots.begin()); };
            if (!check(at(52) % 2 == 0 && at(party_module::base_option_count + 1) == at(52) + 1 && at(55) == at(54) + 1 && at(54) % 2 == 0,
                "Party bar width/height and X/Y offsets share rows without moving window width into the bar pair")) return false;
            runtime.active_config_section_ = party_module::text_section;
            const auto textSlots = runtime.GroupOptionSlots();
            if (!check(std::string_view{runtime.party_.Option(textSlots[0]).label} == "Font" &&
                std::string_view{runtime.party_.Option(textSlots[1]).label} == "Size (px)", "text controls start with font and size")) return false;
            runtime.controller_config_.region = config_controller_region::options;
            auto items = runtime.ControllerConfigItems(config_controller_region::options);
            if (!check(items.size() == 15, "controller navigation skips layout placeholders and exposes every text control")) return false;
            runtime.controller_config_.id = items[0].id;
            runtime.HandleControllerConfig(controller_action::right, false);
            if (!check(runtime.controller_config_.id == items[1].id, "controller follows the displayed font/size row")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            if (!check(runtime.controller_config_.adjusting, "controller activates the original size setting after reordering")) return false;
            runtime.controller_config_.adjusting = false;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto value = ui::config_layout::SliderValue(runtime.ConfigLayout().OptionRow(1));
            runtime.mouse_x_ = static_cast<int>(value.x + 3); runtime.mouse_y_ = static_cast<int>(value.y + 3);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.slider_input_.Focused() && runtime.slider_input_.Option().slider == runtime.party_.Option(textSlots[1]).slider,
                "pointer edits the same size setting at its new position")) return false;
            runtime.FinishConfigInput(false);
            config_controller_focus focus;
            const std::vector<config_controller_item> grid{{"a", {0, 0, 100, 30}}, {"b", {110, 0, 100, 30}},
                {"c", {0, 30, 100, 30}}, {"d", {0, 60, 100, 30}}, {"e", {110, 60, 100, 30}}};
            focus.id = "b";
            if (!check(focus.MoveGrid(grid, 0, 1) && focus.id == "c" && focus.MoveGrid(grid, 0, 1) && focus.id == "d" &&
                focus.MoveGrid(grid, 1, 0) && focus.id == "e", "controller directions follow actual rows and skip empty cells")) return false;
            report << "## Coverage\n\n" << runtime.modules_.Count() << " registered modules, " << pages << " distinct pages, " << controls << " controls across those pages, and " << variants << " selector/view combinations.\n";
            if (reportPath != nullptr) { std::ofstream output{reportPath, std::ios::binary}; output << report.str(); }
            std::printf("Settings presentation: %zu modules, %zu pages, %zu controls, %zu variants; pairing and pointer/controller routes passed.\n",
                runtime.modules_.Count(), pages, controls, variants);
            return true;
        }

        static auto ConfigText(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.active_module_ = &runtime.general_;
            runtime.active_config_section_ = general_module::appearance_section; runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto click = [&](const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f); runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
                const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
            };
            if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 2, std::size(general_module::appearance_tabs))) &&
                runtime.general_.AppearanceTab() == 2 && ControlCount(runtime) == ui::config_text_settings::option_count && runtime.GroupOptionIndex(0) == runtime.general_.ConfigTextStart() &&
                runtime.GroupOptionIndex(1) == runtime.general_.ConfigTextStart() + 15,
                "Appearance exposes Config Text independently of Window and Global Text")) return false;
            auto& hud = runtime.player_.Settings().name_text.style; hud.font = 2; hud.letter_spacing = 0.75f; hud.scale = 1.1f;
            if (!check(click(ui::config_layout::ChoiceButton(runtime.ConfigLayout().OptionRow(0))) && runtime.choice_input_.Opened(),
                "Config font opens the shared four-family dropdown")) return false;
            runtime.SelectChoiceInput(3);
            *runtime.general_.Option(runtime.general_.ConfigTextStart() + 1).slider = 1.5f;
            runtime.general_.OnOptionChanged(runtime.general_.ConfigTextStart() + 1);
            const auto size = runtime.general_.Option(runtime.general_.ConfigTextStart() + 15);
            if (!check(size.kind == module_option_kind::slider && *size.slider == 14 && size.minimum == 8 && size.maximum == 18,
                "Config text size appears beside font with the 14-pixel default")) return false;
            *size.slider = 16;
            runtime.general_.OnOptionChanged(runtime.general_.ConfigTextStart() + 15);
            if (!check(hud.font == 2 && hud.letter_spacing == 0.75f && hud.scale == 1.1f && runtime.general_.GlobalFont() == 0,
                "config typography never changes HUD text preferences")) return false;
            for (const float scale : {0.68f, 0.72f, 0.82f, 1.0f})
            {
                auto source = runtime.design_system_.GetTheme().body_text; source.scale = scale;
                const auto style = runtime.config_controls_.Style(source, true);
                auto unspaced = style; unspaced.letter_spacing = 0;
                if (!check(style.font == 3 && style.shadow.enabled && style.stroke.enabled && !style.glow.enabled &&
                    std::abs(style.scale - scale * 16 / 12) < 0.0001f &&
                    std::abs(style.letter_spacing * ui::TextPixelScale(style.scale) - 1.5f) < 0.0001f &&
                    std::abs(runtime.design_system_.MeasureText("ABC", style) - runtime.design_system_.MeasureText("ABC", unspaced) - 3) < 0.0001f,
                    "every config size measures the chosen font with consistent pixel spacing and normal config effects")) return false;
            }
            ui::share_key_view view;
            const std::string key(96, 'A'); const ui::rect keyBounds{0, 0, 140, 100};
            view.Draw(runtime.design_system_, runtime.config_controls_, keyBounds, key, false);
            *runtime.general_.Option(runtime.general_.ConfigTextStart()).choice = 1;
            *runtime.general_.Option(runtime.general_.ConfigTextStart() + 1).slider = 4;
            view.Draw(runtime.design_system_, runtime.config_controls_, keyBounds, key, false);
            auto keyStyle = runtime.design_system_.GetTheme().body_text; keyStyle.scale = 0.74f;
            keyStyle = runtime.config_controls_.Style(keyStyle);
            for (const auto& row : view.Lines())
                if (!check(runtime.design_system_.MeasureText(row.c_str(), keyStyle) <= keyBounds.width - 32,
                    "share keys reflow when font or letter spacing changes")) return false;
            const auto effect = [&](const size_t index) { return runtime.general_.Option(runtime.general_.ConfigTextStart() + index); };
            *effect(3).color = 0x80403020; *effect(4).slider = -3; *effect(5).slider = 4; *effect(6).toggle = true;
            *effect(7).choice = 2; *effect(8).color = 0xA0102030;
            *effect(9).toggle = true; *effect(10).color = 0x40506070;
            *effect(11).toggle = true; *effect(12).color = 0x708090A0; *effect(13).slider = 28; *effect(14).slider = 0.5f;
            const auto styled = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text);
            if (!check(styled.shadow.color == 0x80403020 && styled.shadow.offset_x == -3 && styled.shadow.offset_y == 4 && styled.shadow.soft &&
                styled.stroke.enabled && styled.stroke.color == 0xA0102030 && styled.glow.enabled && styled.glow.color == 0x40506070 &&
                styled.sheen.enabled && styled.sheen.color == 0x708090A0 && styled.sheen.width == 28 && styled.sheen.speed == 0.5f &&
                hud.font == 2 && hud.letter_spacing == 0.75f,
                "config controls resolve live shared effects, offsets and alpha independently of HUD preferences")) return false;
            auto store = settings_store::FromIniText(""); runtime.general_.Save(store);
            general_module restored; restored.Load(store);
            const auto& saved = restored.ConfigText();
            if (!check(saved.font == 1 && saved.size == 16 && saved.letter_spacing == 4 && saved.shadow.color == styled.shadow.color && saved.shadow.offset_x == -3 &&
                saved.shadow.offset_y == 4 && saved.shadow.soft && saved.outline == 2 && saved.outline_color == styled.stroke.color &&
                saved.glow.enabled && saved.glow.color == styled.glow.color && saved.sheen.enabled && saved.sheen.color == styled.sheen.color &&
                saved.sheen.width == 28 && saved.sheen.speed == 0.5f,
                "config typography and effects round trip through normal profile preferences")) return false;
            auto legacy = settings_store::FromIniText("[config.text]\nfont=2\n"); restored.Load(legacy);
            if (!check(restored.ConfigText().size == 14, "profiles without a config size use the 14-pixel default")) return false;
            *effect(2).toggle = false; *effect(7).choice = 0; *effect(9).toggle = false; *effect(11).toggle = false;
            const auto disabled = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text, true);
            if (!check(!disabled.shadow.enabled && !disabled.stroke.enabled && !disabled.glow.enabled && !disabled.sheen.enabled,
                "config effects can all be disabled, including button outlines")) return false;
            runtime.general_.ResetDefaults(runtime.design_system_.GetTheme());
            const auto reset = runtime.config_controls_.Style(runtime.design_system_.GetTheme().body_text);
            runtime.editing_ = false;
            return check(reset.font == 0 && std::abs(reset.scale - runtime.design_system_.GetTheme().body_text.scale * 14 / 12) < 0.0001f && reset.letter_spacing == 0 && reset.shadow.enabled && !reset.stroke.enabled &&
                runtime.config_controls_.Style(reset, true).stroke.enabled && !reset.glow.enabled && !reset.sheen.enabled,
                "Reset defaults restores config typography and button-only outlines through the live settings binding");
        }

        static auto ConfigScrollbar(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 1100, 500};
            runtime.active_module_ = &runtime.chat_; runtime.active_config_section_ = 7;
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto top = runtime.ConfigScrollbar();
            if (!check(top.Visible() && top.Thumb().y == top.track.y, "long config pages show a scrollbar at the current position")) return false;
            const auto lastColumn = runtime.ConfigLayout().OptionRow(1);
            if (!check(lastColumn.x + lastColumn.width < top.track.x, "scrollbar gutter keeps both option columns unobscured")) return false;
            runtime.mouse_x_ = static_cast<int>(top.Thumb().x + top.Thumb().width / 2);
            runtime.mouse_y_ = static_cast<int>(top.Thumb().y + top.Thumb().height / 2);
            if (!check(runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) &&
                runtime.interaction_ == hud_runtime::interaction::option_scrollbar, "thumb press captures config scrolling")) return false;
            runtime.mouse_y_ = static_cast<int>(top.track.y + top.track.height + 30);
            runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport);
            const auto bottom = runtime.ConfigScrollbar();
            if (!check(runtime.option_scroll_ == bottom.Maximum() * 2 && std::abs(bottom.Thumb().y + bottom.Thumb().height - bottom.track.y - bottom.track.height) < 0.1f,
                "thumb dragging reaches the final option row and stays bounded outside the track")) return false;
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.interaction_ == hud_runtime::interaction::none, "thumb release ends pointer capture")) return false;
            runtime.mouse_y_ = static_cast<int>(bottom.track.y + 1);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.option_scroll_ < bottom.Maximum() * 2 && runtime.option_scroll_ % 2 == 0,
                "track clicks page without swapping option columns")) return false;
            runtime.config_collapsed_ = true;
            if (!check(!runtime.ConfigScrollbar().Visible(), "collapsed config hides the scrollbar")) return false;
            runtime.config_collapsed_ = false; runtime.active_module_ = &runtime.player_; runtime.active_config_section_ = 0;
            return check(!runtime.ConfigScrollbar().Visible(), "short config pages do not reserve a scrollbar");
        }

        static auto HeaderInputMode(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto original = runtime.player_.State().bounds;
            const auto click = [&](const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f); runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
                const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
            };
            for (const bool collapsed : {false, true})
            {
                runtime.config_collapsed_ = collapsed;
                for (int mode = 0; mode < 3; ++mode)
                {
                    if (!check(click(runtime.ConfigLayout().InputMode()) && runtime.input_mode_menu_, "pointer opens the header input dropdown")) return false;
                    controller_status unavailable{}; unavailable.enumeration_disabled = true; unavailable.connection_known = true;
                    runtime.UpdateControllerStatus(unavailable); runtime.UpdateControllerFocus();
                    if (!check(runtime.input_mode_menu_, "unavailable controller updates cannot dismiss a mouse-owned mode dropdown")) return false;
                    if (!check(click(runtime.ConfigLayout().InputModeRow(mode)) && runtime.general_.InputMode() == mode && !runtime.input_mode_menu_,
                        "pointer selects every input mode with expanded or collapsed config")) return false;
                }
            }
            if (!check(runtime.player_.State().bounds.x == original.x && runtime.player_.State().bounds.y == original.y,
                "mode selection leaves HUD positions unchanged")) return false;
            runtime.controller_surface_ = hud_runtime::controller_surface::config; runtime.controller_config_.active = true; runtime.input_mode_menu_ = true;
            controller_status disconnected{}; disconnected.connection_known = true;
            runtime.UpdateControllerStatus(disconnected);
            if (!check(runtime.controller_surface_ == hud_runtime::controller_surface::none && !runtime.input_mode_menu_,
                "disconnect still releases a controller-owned popup")) return false;
            return check(ui::config_controls::IconId("statuses") == ui::icon_glyph::statuses && ui::config_controls::IconId("buffs") == ui::icon_glyph::statuses &&
                ui::config_controls::IconId("debuffs") == ui::icon_glyph::statuses, "status groups use their distinct effects icon");
        }

        static auto AllBarsScope(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 1100, 600};
            const auto snapshot = [](const auto& styles) {
                auto store = settings_store::FromIniText("");
                for (size_t i = 0; i < styles.size(); ++i) SaveMeterStyle(store, ("meter." + std::to_string(i)).c_str(), styles[i]);
                return store.ToIniText();
            };
            for (auto* module : {static_cast<hud_module*>(&runtime.player_), static_cast<hud_module*>(&runtime.party_),
                static_cast<hud_module*>(&runtime.alliance_first_), static_cast<hud_module*>(&runtime.alliance_second_)})
            {
                auto* party = module == &runtime.player_ ? nullptr : static_cast<party_module*>(module);
                std::vector<ui::meter_style*> styles = party != nullptr ? std::vector<ui::meter_style*>{&party->meters_[0], &party->meters_[1], &party->meters_[2], &party->meters_[3]} :
                    std::vector<ui::meter_style*>{&runtime.player_.Settings().hp_style, &runtime.player_.Settings().mp_style, &runtime.player_.Settings().tp_style};
                std::vector<ui::meter_style> expected;
                for (size_t i = 0; i < styles.size(); ++i)
                {
                    styles[i]->height = 12 + static_cast<float>(i) * 5;
                    styles[i]->appearance = i % 2 == 0 ? ui::meter_style::modern : ui::meter_style::window_theme;
                    styles[i]->shadow_strength = 0.1f * static_cast<float>(i + 1);
                    styles[i]->sheen = 0xFF102030 + static_cast<D3DCOLOR>(i);
                    styles[i]->bookends = styles[i]->themed_bookends = false;
                    expected.push_back(*styles[i]);
                }
                const auto actual = [&] { std::vector<ui::meter_style> values; for (const auto* style : styles) values.push_back(*style); return snapshot(values); };
                runtime.active_module_ = module;
                runtime.active_config_section_ = party != nullptr ? party_module::bar_style_section : static_cast<size_t>(bar_config_section::bar_style);
                const size_t scope = party != nullptr ? 24 : 4, selected = party != nullptr ? 25 : 5, start = party != nullptr ? party_module::base_option_count : 9;
                *module->Option(scope).choice = 1; *module->Option(selected).choice = 0;
                if (party != nullptr)
                {
                    party->bar_widths_ = {170, 120, 60, 140}; party->bar_positions_ = {0, 1, 2, 3};
                    party->bar_offsets_x_ = {1, 2, 3, 4}; party->bar_offsets_y_ = {-1, -2, -3, -4};
                }
                const auto selector = runtime.ElementSelectorLayout(runtime.ElementSelector(), runtime.ConfigLayout());
                runtime.mouse_x_ = static_cast<int>(selector.tabs[0].x + 5); runtime.mouse_y_ = static_cast<int>(selector.tabs[0].y + 5);
                if (!check(runtime.ProcessElementSelectorClick() && actual() == snapshot(expected), "selecting All bars by pointer preserves every existing meter field")) return false;
                *module->Option(scope).choice = 1;
                runtime.controller_config_.region = config_controller_region::elements;
                const auto items = runtime.ControllerConfigItems(config_controller_region::elements);
                if (!check(!items.empty() && runtime.ActivateControllerConfigItem(items.front(), controller_action::confirm, false) && actual() == snapshot(expected),
                    "selecting All bars by controller preserves every existing meter field")) return false;
                const auto edit = [&](const size_t index) { module->OnOptionChanged(start + index); return actual() == snapshot(expected); };
                *module->Option(start + 1).slider = 19; for (auto& style : expected) style.height = 19;
                if (!check(edit(1), "editing shared height preserves each bar's appearance, effects and colors")) return false;
                *module->Option(start + 6).toggle = false; for (auto& style : expected) style.shadow_enabled = false;
                if (!check(edit(6), "editing one shared effect preserves all other properties")) return false;
                *module->Option(start + 25).color = 0xFFABCDEF; for (auto& style : expected) style.sheen = 0xFFABCDEF;
                if (!check(edit(25), "editing one shared color preserves all other properties")) return false;
                *module->Option(start + 2).toggle = true;
                for (auto& style : expected) (style.appearance == ui::meter_style::modern ? style.bookends : style.themed_bookends) = true;
                if (!check(edit(2), "shared Bookends applies to each bar's current appearance without copying styles")) return false;
                *module->Option(start).choice = ui::meter_style::window_theme; for (auto& style : expected) style.appearance = ui::meter_style::window_theme;
                if (!check(edit(0), "shared appearance changes preserve independent bar geometry and effects")) return false;
                if (party != nullptr && !check(party->bar_widths_ == std::array<float,4>{170,120,60,140} && party->bar_positions_ == std::array<int,4>{0,1,2,3} &&
                    party->bar_offsets_x_ == std::array<float,4>{1,2,3,4} && party->bar_offsets_y_ == std::array<float,4>{-1,-2,-3,-4},
                    "All bars selection and style edits preserve independent Party widths, positions and offsets")) return false;
                *module->Option(scope).choice = 1; *module->Option(start + 1).slider = 27; module->OnOptionChanged(start + 1); expected[0].height = 27;
                if (!check(actual() == snapshot(expected), "returning to an individual bar retains independent edits")) return false;
            }
            return true;
        }

        static auto ElementTabs(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto click = [&](const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + at.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(at.y + at.height * 0.5f);
                const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
            };
            size_t selectors{};
            for (auto* module : runtime.modules_.All())
            {
                module->State().enabled = true;
                const auto topics = module->ConfigSections();
                runtime.active_module_ = module;
                for (size_t topic = 0; topic < topics.size(); ++topic)
                {
                    runtime.active_config_section_ = topic;
                    const auto selector = runtime.ElementSelector();
                    if (selector.count == 0) continue;
                    ++selectors;
                    const auto layout = runtime.ConfigLayout();
                    const auto tabs = runtime.ElementSelectorLayout(selector, layout);
                    if (!check(tabs.count == selector.count && tabs.tabs.back().width == 0 &&
                        tabs.tabs[tabs.count - 1].y + 28 <= layout.OptionRow(0).y,
                        "all editable elements have pinned tabs above their settings")) return false;
                    for (size_t i = 0; i < runtime.GroupOptionCount(); ++i)
                        if (!check(runtime.GroupOptionIndex(i) >= module->OptionCount() || module->Option(runtime.GroupOptionIndex(i)).selector == module_selector::none,
                            "element selectors do not leave duplicate cycling rows")) return false;
                }
            }
            if (!check(selectors >= 18, "shared selectors cover text and bar editors across modules")) return false;
            runtime.active_module_ = &runtime.party_; runtime.active_config_section_ = party_module::text_section;
            for (const float width : {936.0f, 1100.0f})
            {
                runtime.config_bounds_.width = width;
                const auto selector = runtime.ElementSelector();
                const auto tabs = runtime.ElementSelectorLayout(selector, runtime.ConfigLayout());
                if (!check(selector.count == 7 && click(tabs.tabs[6]) && runtime.party_.selected_text_ == 6 &&
                    runtime.ElementSelector().selected == 6 && runtime.ElementSelectorTarget() != 0 &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                    "Heading is directly selectable with matching highlight, sound and cursor at both widths")) return false;
                const auto layout = runtime.ConfigLayout();
                const auto first = layout.Section(0, 3), last = layout.Section(2, 3), menu = layout.Submenu(0, runtime.party_.ConfigSections().size());
                if (!check(first.x == menu.x && first.y + first.height < menu.y && layout.ElementTabs().y == menu.y &&
                    last.x + last.width <= runtime.config_bounds_.x + width - 8,
                    "top-level tabs span both secondary navigation and settings without overlapping either")) return false;
                if (!check(click(last) && runtime.active_module_ == &runtime.alliance_second_,
                    "full-width header tabs route to the correct alliance")) return false;
                runtime.active_module_ = &runtime.party_; runtime.active_config_section_ = party_module::text_section;
            }
            runtime.active_module_ = &runtime.player_; runtime.active_config_section_ = static_cast<size_t>(bar_config_section::bar_style);
            auto selector = runtime.ElementSelector();
            auto tabs = runtime.ElementSelectorLayout(selector, runtime.ConfigLayout());
            if (!check(selector.combined && selector.count == 4 && std::strcmp(selector.labels[0], "All bars") == 0 &&
                click(tabs.tabs[2]) && runtime.player_.Settings().style_edit_mode == 1 && runtime.player_.Settings().selected_bar == 1,
                "All bars and HP/MP/TP share one direct selector")) return false;
            runtime.player_.Settings().hp_style.height = 12; runtime.player_.Settings().mp_style.height = 23;
            const auto tpHeight = runtime.player_.Settings().tp_style.height;
            tabs = runtime.ElementSelectorLayout(runtime.ElementSelector(), runtime.ConfigLayout());
            if (!check(click(tabs.tabs[0]) && runtime.player_.Settings().style_edit_mode == 0 &&
                runtime.player_.Settings().hp_style.height == 12 && runtime.player_.Settings().mp_style.height == 23 && runtime.player_.Settings().tp_style.height == tpHeight,
                "All bars changes editing scope without changing individual bar heights")) return false;
            const std::array<float, 4> widths{100, 100, 100, 100};
            const ui::element_selector_layout wrapped{{0, 0, 220, 28}, widths};
            if (!check(wrapped.count == 4 && wrapped.tabs[2].y == 32 && wrapped.height == 68 && !wrapped.IndexAt(102, 10),
                "narrow selectors wrap and gaps have no hidden click target")) return false;
            auto store = settings_store::FromIniText(""); runtime.player_.Save(store); runtime.party_.Save(store);
            player_module restoredPlayer; restoredPlayer.Load(store);
            party_module restoredParty; restoredParty.Load(store);
            runtime.editing_ = false;
            return check(restoredPlayer.Settings().style_edit_mode == 0 && restoredPlayer.Settings().selected_bar == 1 &&
                *restoredParty.Option(26).choice == 6, "direct selection retains the existing saved preferences");
        }

        static auto PartyEntries(void) -> bool
        {
            ui::design_system design;
            module_render_context context{design};
            party_module jobLayout;
            for (const bool fit : {false,true}) for (const int layout : {0,1,2})
            {
                jobLayout.fit_width_=fit; jobLayout.layout_=layout;
                jobLayout.bar_widths_={140,92,32,0}; jobLayout.job_icons_=false;
                const auto plain=jobLayout.LayoutContainer();
                const auto plainEntry=jobLayout.LayoutEntry({20,0,plain.canvas_width,0},true,{true,true,true,true});
                jobLayout.job_icons_=true; jobLayout.icon_size_=64;
                jobLayout.icon_offset_x_=-90;
                for (const bool follow : {false,true})
                {
                    jobLayout.icon_follow_name_=follow;
                    const auto icons=jobLayout.LayoutContainer();
                    const auto iconEntry=jobLayout.LayoutEntry({20,0,icons.canvas_width,0},true,{true,true,true,true});
                    if (!check(icons.width==plain.width && icons.canvas_width==plain.canvas_width && icons.inset==plain.inset &&
                        iconEntry.header.x==plainEntry.header.x && iconEntry.header.width==plainEntry.header.width &&
                        iconEntry.resources.bars[0].x==plainEntry.resources.bars[0].x && iconEntry.resources.bars[0].width==plainEntry.resources.bars[0].width,
                        "job icon visibility, size, anchor and offsets cannot widen entries or push their content right")) return false;
                }
            }
            jobLayout=party_module{};
            party_member jobMember{};
            jobMember.main_job=3; jobMember.main_level=99;
            jobMember.sub_job=5; jobMember.sub_level=49; jobMember.master_level=20;
            if (!check(jobLayout.JobLines(jobMember)[0]=="WHM99/RDM49 ML20" && jobLayout.JobLines(jobMember)[1].empty(),
                "existing inline job formatting is retained by default")) return false;
            const auto inlineJobEntry=jobLayout.LayoutEntry({0,0,300,0},true,{});
            jobLayout.stack_jobs_=true;
            const auto stackedJobs=jobLayout.JobLines(jobMember);
            if (!check(stackedJobs[0]=="WHM99 ML20" && stackedJobs[1]=="RDM49" &&
                jobLayout.LayoutEntry({0,0,300,0},true,{}).height>inlineJobEntry.height,
                "stacked job rows keep master level with main job and reserve both text lines")) return false;
            jobLayout.sub_job_=false;
            if (!check(jobLayout.JobLines(jobMember)[1].empty() && jobLayout.JobTextHeight()==jobLayout.LineHeight(jobLayout.text_[1]),
                "hiding subjob removes the second line and its reserved height")) return false;
            jobLayout.sub_job_=true;
            jobLayout.job_anchor_=party_module::job_anchor_hp;
            jobLayout.bar_offsets_x_[0]=26; jobLayout.bar_offsets_y_[0]=7;
            const auto hpEntry=jobLayout.LayoutEntry({0,0,300,0},true,{});
            const auto hpJobs=jobLayout.HPJobTextBounds(hpEntry.resources.bars[0]);
            const auto track=ui::LayoutMeterTrack(hpEntry.resources.bars[0],jobLayout.meters_[0]);
            jobLayout.text_[0].offset_x+=33;
            const auto movedName=jobLayout.LayoutEntry({0,0,300,0},true,{});
            const auto stillHP=jobLayout.HPJobTextBounds(movedName.resources.bars[0]);
            if (!check(hpJobs.x==track.x && hpJobs.y==track.y && stillHP.x==hpJobs.x && stillHP.y==hpJobs.y && !jobLayout.Option(49).visible,
                "HP-anchored job text follows the actual shifted HP fill independently of name positioning")) return false;
            auto jobStore=settings_store::FromIniText(""); jobLayout.Save(jobStore);
            party_module savedJobs; savedJobs.Load(jobStore);
            savedJobs.selected_text_=1;
            if (!check(savedJobs.stack_jobs_ && savedJobs.job_anchor_==party_module::job_anchor_hp &&
                savedJobs.ConfigSectionContains(party_module::text_section,party_module::job_text_options_start) &&
                savedJobs.ConfigSectionContains(party_module::text_section,party_module::job_text_options_start+1),
                "job stack and anchor preferences persist and appear under Text > Jobs")) return false;
            for (const size_t option : std::array<size_t,8>{7,8,9,10,11,49,party_module::job_text_options_start,party_module::job_text_options_start+1})
            {
                size_t appearances{};
                for (size_t section=0;section<savedJobs.ConfigSections().size();++section)
                    appearances+=savedJobs.ConfigSectionContains(section,option);
                if (!check(appearances==1 && !savedJobs.ConfigSectionContains(party_module::jobs_section,option),
                    "job text display and positioning controls appear once alongside their text styles")) return false;
                savedJobs.selected_text_=0;
                if (!check(!savedJobs.ConfigSectionContains(party_module::text_section,option),
                    "job controls stay out of other text elements")) return false;
                savedJobs.selected_text_=1;
            }
            if (!check(std::string_view{savedJobs.ConfigSections()[party_module::jobs_section].name}=="Job Icons" &&
                savedJobs.ConfigSectionContains(party_module::jobs_section,6) &&
                savedJobs.ConfigSectionContains(party_module::jobs_section,party_module::icon_offset_options_start) &&
                std::string_view{TextProfileOption(0,savedJobs.text_[1]).label}=="Alignment",
                "icon settings have an explicit home and text alignment is distinct from placement")) return false;
            party_module captions;
            captions.text_[0].offset_x = 44; captions.text_[0].offset_y = 8;
            const ui::rect header{20, 50, 220, 24};
            for (const int placement : {0, 1, 2, 3})
            {
                captions.text_[0].placement = placement;
                const auto name = context.TextBounds("Shuu", header, header, captions.text_[0]);
                const auto area = captions.JobTextBounds(header, name, header.x + header.width);
                const auto jobs = context.TextBounds("WHM99", area, area, captions.text_[1]);
                if (!check(captions.job_placement_ == party_module::jobs_name && jobs.x == name.x + name.width + 6 &&
                    area.y == header.y + 8, "default job text follows the displayed name and its offsets in every name alignment")) return false;
            }
            const auto remote = captions.LayoutEntry({0, 0, 300, 0}, false, {});
            const auto zoneArea = ui::container_renderer::ContentBounds(remote.zone, captions.ZoneStyle());
            const auto zone = context.TextBounds("Ru'Lude Gardens", zoneArea, zoneArea, captions.text_[party_module::zone_text]);
            for (const int placement : {0, 1, 2, 3})
            {
                captions.text_[1].placement = placement; captions.text_[1].offset_x = 40; captions.text_[1].offset_y = -30;
                captions.text_[1].style.scale = 3;
                const auto after = captions.LayoutEntry({0, 0, 300, 0}, false, {});
                const auto content = ui::container_renderer::ContentBounds(after.zone, captions.ZoneStyle());
                const auto visible = context.TextBounds("Ru'Lude Gardens", content, content, captions.text_[party_module::zone_text]);
                if (!check(after.height == remote.height && after.zone.y == remote.zone.y && after.zone.height == remote.zone.height &&
                    visible.x == zone.x && visible.y == zone.y, "Jobs placement, offsets and font size leave remote zone text and its entry unchanged")) return false;
            }
            auto legacyCaptions = settings_store::FromIniText("[text.party.jobs]\nfont=2\noffset_x=40\noffset_y=-30\nplacement=3\n");
            party_module migratedCaptions; migratedCaptions.Load(legacyCaptions);
            if (!check(migratedCaptions.text_[party_module::zone_text].style.font == 2 &&
                migratedCaptions.text_[party_module::zone_text].offset_x == 0 && migratedCaptions.text_[party_module::zone_text].offset_y == 0 &&
                migratedCaptions.text_[party_module::zone_text].placement == 0, "initial Zone styling retains legacy font effects without inheriting Jobs placement or offsets")) return false;
            migratedCaptions.text_[party_module::zone_text].offset_y = 4;
            migratedCaptions.text_[party_module::zone_text].placement = 2;
            migratedCaptions.Save(legacyCaptions);
            legacyCaptions.WriteInt("text.party.jobs", "font", 3);
            party_module restoredCaptions; restoredCaptions.Load(legacyCaptions);
            if (!check(restoredCaptions.text_[1].style.font == 3 && restoredCaptions.text_[1].placement == 3 &&
                restoredCaptions.text_[party_module::zone_text].style.font == 2 && restoredCaptions.text_[party_module::zone_text].offset_y == 4 &&
                restoredCaptions.text_[party_module::zone_text].placement == 2, "Jobs and Zone text persist independently after the initial split")) return false;
            party_module party;
            party.jobs_ = party.buffs_ = party.tp_ = party.job_icons_ = party.casts_ = party.distance_ = false;
            party.selection_ = false; party.hp_display_ = 5;
            party.text_[0].style.scale = 1;
            party.meters_[0].height = 12; party.meters_[1].height = party.meters_[2].height = 80;
            party.bar_offsets_y_[0] = -party.HeaderHeight(false, false) - 1;
            auto entry = party.LayoutEntry({0, 100, 300, 0}, true, {}, false);
            if (!check(entry.height == design.TextHeight(party.text_[0].style),
                "overlapping name and HP reserve their union, and hidden tall MP/TP do not enlarge the row")) return false;
            party.job_icons_ = party.icon_follow_name_ = true;
            party.text_[0].offset_y = 27; party.text_[0].offset_x = 18;
            for (const int placement : {0, 1, 2, 3})
            {
                party.text_[0].placement = placement;
                entry = party.LayoutEntry({20, 100, 300, 0}, true, {}, false);
                const auto name = context.TextBounds("Player", entry.header, entry.header, party.text_[0]);
                const auto icon = party.JobIconBounds(entry, name, 0);
                if (!check(icon.x + icon.width + 7 == name.x && icon.y + icon.height * 0.5f == name.y + name.height * 0.5f &&
                    icon.y >= 100 && icon.y + icon.height <= 100 + entry.height && name.y + name.height <= 100 + entry.height,
                    "following icons track positioned/aligned name bounds and fit the measured entry")) return false;
            }
            party.icon_follow_name_ = false;
            if (!check(party.JobIconBounds(entry, {999, 999, 30, 20}, 0).x == entry.icon.x,
                "turning follow off retains the fixed job-icon column")) return false;
            party_member member; member.in_zone = member.active = member.entity_valid = true;
            member.id = 123; member.index = 7; member.name = "Player"; member.main_job = 1; member.sync = member.distance_known = true; member.distance = 12.3f;
            party.distance_ = true;
            if (!check(party.DetailText(member) == "SYNC  12.3y", "SYNC is enabled by default")) return false;
            *party.Option(party_module::sync_option).toggle = false;
            if (!check(party.DetailText(member) == "12.3y" && party.ConfigSectionContains(party_module::general_section, party_module::sync_option) &&
                party.ConfigSectionContains(party_module::jobs_section, party_module::icon_follow_option), "SYNC hides independently of distance and both settings are reachable")) return false;
            party.casts_ = true; party.cast_placement_ = party_module::cast_separate;
            party.text_[0].offset_y = 0; party.bar_offsets_y_ = {};
            const auto idle = party.MemberHeight(member);
            member.cast = {"Cure", 1, ::GetTickCount64() - 1000, 0, 4};
            if (!check(party.MemberHeight(member) > idle, "separate casts reserve height only while visible")) return false;
            party.cast_placement_ = party_module::cast_mp;
            if (!check(party.ResourceSlots(member).mp && !party.ResourceSlots(member).mp_values, "an active cast can replace an otherwise hidden MP slot")) return false;
            member.cast = {};
            if (!check(!party.ResourceSlots(member).mp, "an unused cast replacement slot does not leave an empty row")) return false;
            party.icon_follow_name_ = true;
            auto store = settings_store::FromIniText(""); party.Save(store);
            party_module restored; restored.Load(store);
            party_module alliance{1}; alliance.Save(store);
            party_module restoredAlliance{1}; restoredAlliance.Load(store);
            return check(restored.icon_follow_name_ && !restored.show_sync_ && !restoredAlliance.icon_follow_name_ && restoredAlliance.show_sync_,
                "party and alliance preferences persist independently");
        }

        static auto ChoiceArrows(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.active_module_ = &runtime.player_;
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::bar_style);
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto at = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
            };
            const auto click = [&](const ui::rect& bounds) {
                at(bounds);
                const bool press = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && press;
            };
            const auto mode = runtime.ConfigLayout().OptionRow(0), resource = runtime.ConfigLayout().OptionRow(1);
            if (!check(click(ui::config_layout::ChoiceNext(mode)) && runtime.player_.Settings().style_edit_mode == 1,
                "next arrow changes to Per bar through normal captured input")) return false;
            if (!check(click(ui::config_layout::ChoicePrevious(resource)) && runtime.player_.Settings().selected_bar == 2 &&
                click(ui::config_layout::ChoiceNext(resource)) && runtime.player_.Settings().selected_bar == 0,
                "previous and next arrows wrap both ends of the resource choices")) return false;
            if (!check(click(ui::config_layout::ChoiceLabel(resource)) && runtime.player_.Settings().selected_bar == 1,
                "clicking the value retains next-choice behavior")) return false;
            at(ui::config_layout::ChoicePrevious(resource)); const auto previous = runtime.ConfigSoundTarget(viewport);
            at(ui::config_layout::ChoiceNext(resource)); const auto next = runtime.ConfigSoundTarget(viewport);
            if (!check(previous != 0 && next != previous && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                "arrows have distinct hover feedback and the hand cursor")) return false;
            runtime.player_.Settings().hp_style.height = 17; runtime.player_.Settings().mp_style.height = 29;
            runtime.player_.Settings().tp_style.height = 41;
            if (!check(click(ui::config_layout::ChoicePrevious(mode)) && runtime.player_.Settings().style_edit_mode == 0 &&
                runtime.player_.Settings().hp_style.height == 29 && runtime.player_.Settings().tp_style.height == 29,
                "previous changes still invoke the shared-bar callback")) return false;
            runtime.active_module_ = &runtime.chat_; runtime.active_config_section_ = 5;
            if (!check(click(ui::config_layout::ChoicePrevious(runtime.ConfigLayout().OptionRow(1))) && runtime.choice_input_.Opened(),
                "real dropdowns retain their normal popup behavior")) return false;
            runtime.CloseChoiceInput(); runtime.editing_ = false;
            return true;
        }

        inline static size_t preview_sounds{};
        static auto __cdecl PreviewSound(void) -> void { ++preview_sounds; }
        static auto ChatSoundPreview(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.active_module_ = &runtime.chat_; runtime.active_config_section_ = 5;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.ui_sounds_.sounds_[3].function = reinterpret_cast<uintptr_t>(&PreviewSound);
            runtime.ui_sounds_.sounds_[7].function = reinterpret_cast<uintptr_t>(&PreviewSound);
            runtime.ui_sounds_.sounds_[1].function = reinterpret_cast<uintptr_t>(&SelectSound);
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + 3); runtime.mouse_y_ = static_cast<int>(at.y + 3);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            const auto click = [&](const ui::rect& at) { send(WM_LBUTTONDOWN, MK_LBUTTON, at); return send(WM_LBUTTONUP, 0, at); };
            const auto option = runtime.chat_.Option(runtime.GroupOptionIndex(1));
            click(runtime.ConfigLayout().OptionRow(1));
            const auto button = runtime.choice_input_.PreviewButton(4, viewport);
            const auto confirmations = select_sounds; preview_sounds = 0;
            if (!check(click(button) && preview_sounds == 1 && select_sounds == confirmations && runtime.choice_input_.Opened() &&
                *option.choice == 1 && runtime.KeyboardFocused() && runtime.mouse_buttons_ == 0,
                "a play icon previews once without a confirm cue, selection, close or leaked pointer release")) return false;
            send(WM_LBUTTONDOWN, MK_LBUTTON, button); send(WM_LBUTTONDOWN, MK_LBUTTON, button); send(WM_LBUTTONUP, 0, button);
            if (!check(preview_sounds == 2, "duplicate pointer messages cannot repeat a preview")) return false;
            runtime.HandleChoiceKey(VK_SPACE, 0); runtime.HandleChoiceKey(VK_SPACE, 0x40000000);
            runtime.HandleChoiceKey(VK_SPACE, static_cast<LPARAM>(0x80000000u));
            if (!check(preview_sounds == 3 && *option.choice == 1 && runtime.choice_input_.Opened(),
                "Space previews the highlighted sound once and held/released keys do not repeat it")) return false;
            if (!check(!runtime.PreviewChoiceInput(0), "Off has no sound preview")) return false;
            runtime.choice_input_.Scroll(1);
            if (!check(click(runtime.choice_input_.PreviewButton(8, viewport)) && preview_sounds == 4 && *option.choice == 1,
                "scrolled sound rows keep independent preview hit areas")) return false;
            click(runtime.choice_input_.Row(8, viewport));
            if (!check(*option.choice == 8 && !runtime.choice_input_.Opened() && !runtime.KeyboardFocused(),
                "clicking the row still selects the clip normally after previewing")) return false;
            runtime.editing_ = false;
            return true;
        }

        static auto ProfileSharing(void) -> bool
        {
            std::string error;
            const auto presetResource = ::FindResourceW(nullptr, MAKEINTRESOURCEW(305), MAKEINTRESOURCEW(10));
            if (!check(presetResource != nullptr, "the portable sharing preset is embedded separately from release defaults")) return false;
            const auto* presetBytes = static_cast<const char*>(::LockResource(::LoadResource(nullptr, presetResource)));
            const std::string_view preset{presetBytes, ::SizeofResource(nullptr, presetResource)};
            if (!check(profile_share_detail::Checksum(preset) == 0xB0E857FE, "XIUI2 retains its immutable portable preset")) return false;
            auto compactSource = settings_store::FromIniText(preset);
            const auto compactKey = EncodeProfileKey("Default", compactSource, error);
            auto compactDecoded = DecodeProfileKey(compactKey, error);
            if (!check(compactKey.starts_with("XIUI2.") && compactKey.size() < 200 && compactDecoded &&
                compactDecoded->preferences.ToIniText() == compactSource.ToIniText(),
                "a default profile shares as a short key and restores every preference without relying on current defaults")) return false;
            compactSource.WriteFloat("text.player.name", "shadow_x", -4);
            compactSource.WriteRect("layout.exp", {337, 1402, 860, 145});
            compactSource.WriteString("future.module", "custom", "settings from a newer module");
            const auto customizedKey = EncodeProfileKey("Custom setup", compactSource, error);
            compactDecoded = DecodeProfileKey(customizedKey, error);
            if (!check(customizedKey.starts_with("XIUI2.") && customizedKey.size() < 500 && compactDecoded &&
                compactDecoded->name == "Custom setup" && compactDecoded->preferences.ToIniText() == compactSource.ToIniText(),
                "compact keys retain changed text styles, positions and fields added by future modules")) return false;
            auto damagedCompact = customizedKey; damagedCompact[6] = damagedCompact[6] == 'A' ? 'B' : 'A';
            const auto patch = profile_share_detail::EncodeDelta("Custom setup", compactSource);
            if (!check(!DecodeProfileKey(damagedCompact, error) && !DecodeProfileKey(customizedKey.substr(0, customizedKey.size() - 4), error) &&
                !profile_share_detail::DecodeDelta(patch.substr(0, patch.size() - 1)) && !profile_share_detail::DecodeDelta(std::string(5, '\xFF')),
                "compact imports reject damaged keys, truncated patches and oversized integer fields")) return false;
            std::printf("Compact default key: %zu characters; customized key: %zu characters.\n", compactKey.size(), customizedKey.size());
            auto source = settings_store::FromIniText("[player]\nenabled=0\n[layout.player]\nx=123\n[chat.sounds]\nparty=3\n[chat.color.party]\noverride=1\ncolor=FF5599BB\n[session.exp.42]\nreceived=123\n[session.gil]\nnet=456\n[profiles]\nactive=foreign\n");
            const auto key = EncodeProfileKey("Shared setup", source, error);
            const auto partial = profile_share_detail::DecodeDelta(profile_share_detail::EncodeDelta("Partial", source));
            if (!check(partial && partial->preferences.ToIniText() == source.Preferences("").ToIniText() && key.starts_with("XIUI1."),
                "removed dictionary fields round trip and sparse profiles retain the smaller legacy format")) return false;
            ui::share_key_view keyView;
            std::array<float, 128> advances{}; advances.fill(1);
            keyView.Reflow(key, 32, 2, advances);
            std::string visibleKey;
            for (const auto& line : keyView.Lines()) visibleKey += line;
            keyView.End();
            if (!check(visibleKey == key && keyView.Lines().size() > 2 && keyView.First() > 0,
                "the wrapped key view retains every character and can scroll to the end without ellipses")) return false;
            keyView.Home();
            if (!check(keyView.First() == 0, "key navigation returns to the beginning")) return false;
            auto decoded = DecodeProfileKey(key, error);
            if (!check(!key.empty() && key == EncodeProfileKey("Shared setup", source, error) && decoded && decoded->name == "Shared setup" &&
                !decoded->preferences.ReadBool("player", "enabled", true) && decoded->preferences.ReadInt("chat.sounds", "party", 0) == 3 &&
                decoded->preferences.ReadArgb("chat.color.party", "color", 0) == 0xFF5599BB,
                "portable keys round-trip layout, chat sounds and custom colors deterministically")) return false;
            if (!check(decoded->preferences.ReadInt("session.exp.42", "received", -1) == -1 &&
                decoded->preferences.ReadInt("session.gil", "net", -1) == -1 && decoded->preferences.ReadString("profiles", "active", "missing") == "missing",
                "share keys omit private session totals and active-profile metadata")) return false;
            if (!check(DecodeProfileKey(" \n" + key.substr(0, 30) + "\n" + key.substr(30) + "\t", error).has_value(),
                "share keys tolerate whitespace introduced while copying")) return false;
            auto damaged = key; damaged[6] = damaged[6] == '0' ? '1' : '0';
            if (!check(!DecodeProfileKey(damaged, error) && !DecodeProfileKey("XIUI2." + key.substr(6), error) &&
                !DecodeProfileKey(key + "!", error) && !DecodeProfileKey(std::string(410 * 1024, 'a'), error),
                "damaged, unsupported and oversized keys are rejected")) return false;
            auto bulky = settings_store::FromIniText(""); bulky.WriteString("large", "value", std::string(300 * 1024, 'a'));
            if (!check(EncodeProfileKey("Too large", bulky, error).empty(), "share payloads have a bounded decompressed size")) return false;

            const auto root = std::filesystem::temp_directory_path() / ("xiui-sharing-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            if (!check(std::filesystem::create_directory(root), "sharing checks use an isolated settings directory")) return false;
            struct cleanup
            {
                std::filesystem::path root;
                ~cleanup()
                {
                    std::error_code ignored;
                    const auto directory = this->root / "xiuinative-profiles";
                    for (const auto& file : std::filesystem::directory_iterator{directory, ignored}) std::filesystem::remove(file.path(), ignored);
                    std::filesystem::remove(directory, ignored); std::filesystem::remove(this->root / "xiuinative.ini", ignored);
                    std::filesystem::remove(this->root, ignored);
                }
            } cleanupFiles{root};
            hud_runtime runtime; runtime.editing_ = true; runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.config_bounds_ = {30, 30, 1100, 600}; runtime.profiles_.Initialize(runtime.settings_path_);
            runtime.player_.State().bounds.x = 123; runtime.SaveSettings(); runtime.DrainSettings();
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            click(runtime.ConfigLayout().ProfileActionsButton());
            if (!check(runtime.profile_actions_ && runtime.KeyboardFocused(), "profile actions menu owns keyboard input")) return false;
            click(runtime.ConfigLayout().ProfileActionRow(0));
            if (!check(runtime.profile_name_.Focused() && runtime.profile_name_.Editor().Text() == "Default Copy" && runtime.CreateProfile() &&
                runtime.profiles_.Current().name == "Default Copy" && runtime.profiles_.Entries().size() == 2,
                "Duplicate creates and selects an independently named copy of the selected profile")) return false;
            runtime.player_.State().bounds.x = 321; runtime.SaveSettings(); runtime.DrainSettings();
            if (!check(runtime.SelectProfile(0) && runtime.player_.State().bounds.x == 123,
                "editing a duplicated profile leaves its source unchanged")) return false;
            if (!check(runtime.OpenProfileDialog(hud_runtime::profile_dialog::share) && !runtime.profile_share_key_.empty() && runtime.KeyboardFocused(),
                "Share opens a copyable portable key for the complete current setup")) return false;
            const auto completeKey = runtime.profile_share_key_;
            const ui::profile_dialog_layout sharingDialog{runtime.config_bounds_, viewport, true};
            runtime.DrawProfileOverlay(viewport);
            visibleKey.clear();
            for (const auto& line : runtime.profile_key_view_.Lines()) visibleKey += line;
            if (!check(visibleKey == completeKey, "Share displays the complete key immediately")) return false;
            runtime.mouse_x_ = static_cast<int>(sharingDialog.Key().x + 3); runtime.mouse_y_ = static_cast<int>(sharingDialog.Key().y + 3);
            if (!check(runtime.ProfileSoundTarget(viewport) == (0xF000000000000000ULL | 14),
                "the visible share key is directly copyable")) return false;
            decoded = DecodeProfileKey(completeKey, error);
            if (!check(decoded && decoded->preferences.ReadRect("layout.player", {}).x == 123,
                "a full runtime snapshot compresses and imports correctly")) return false;
            runtime.HandleProfileKey(VK_ESCAPE, 0);
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::import_profile);
            const ui::profile_dialog_layout dialog{runtime.config_bounds_, viewport, true};
            if (!check(dialog.Paste().y + dialog.Paste().height < dialog.Key().y,
                "Paste key sits above the import key field")) return false;
            runtime.mouse_x_ = static_cast<int>(dialog.Create().x + 3); runtime.mouse_y_ = static_cast<int>(dialog.Create().y + 3);
            if (!check(runtime.ProfileSoundTarget(viewport) == 0, "Import remains disabled until a valid key is loaded")) return false;
            if (!check(runtime.LoadProfileKey(" \n" + completeKey.substr(0, 30) + "\n" + completeKey.substr(30)) && runtime.profile_share_key_ == completeKey,
                "pasted wrapping is removed before displaying the complete import key")) return false;
            if (!check(runtime.LoadProfileKey(completeKey) && runtime.profile_name_.Editor().Text() == "Default 2" &&
                !runtime.LoadProfileKey("damaged") && !runtime.CreateProfile() && runtime.profiles_.Entries().size() == 2 && runtime.player_.State().bounds.x == 123,
                "imports suggest unique names and invalid keys cannot create or overwrite profiles")) return false;
            if (!check(runtime.LoadProfileKey(completeKey) && runtime.CreateProfile() && runtime.profiles_.Current().name == "Default 2" &&
                runtime.profiles_.Entries().size() == 3 && runtime.player_.State().bounds.x == 123 && runtime.SelectProfile(1) && runtime.player_.State().bounds.x == 321,
                "Import creates a separate profile while preserving existing originals and duplicates")) return false;
            runtime.FlushSettings(); runtime.editing_ = false;
            return true;
        }

        static auto ChatCustomization(void) -> bool
        {
            hud_runtime runtime; runtime.editing_ = true; runtime.active_module_ = &runtime.chat_; runtime.active_config_section_ = 5;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            const auto click = [&](const ui::rect& bounds) {
                const bool press = send(WM_LBUTTONDOWN, MK_LBUTTON, bounds);
                return send(WM_LBUTTONUP, 0, bounds) && press;
            };
            const auto soundRow = runtime.ConfigLayout().OptionRow(1);
            const auto sound = runtime.chat_.Option(runtime.GroupOptionIndex(1));
            if (!check(sound.kind == module_option_kind::dropdown && *sound.choice == 1 && click(ui::config_layout::ChoiceButton(soundRow)) &&
                runtime.choice_input_.Opened() && runtime.KeyboardFocused(), "sound selection opens a real dropdown and owns keyboard input")) return false;
            runtime.HandleChoiceKey(VK_DOWN, 0); runtime.HandleChoiceKey(VK_RETURN, 0);
            if (!check(*sound.choice == 2 && !runtime.choice_input_.Opened() && !runtime.KeyboardFocused(), "arrows and Enter commit the highlighted sound")) return false;
            click(soundRow); runtime.HandleChoiceKey(VK_END, 0); runtime.HandleChoiceKey(VK_ESCAPE, 0);
            if (!check(*sound.choice == 2 && !runtime.KeyboardFocused(), "Escape dismisses sound selection without applying it")) return false;
            click(soundRow);
            const auto row = runtime.choice_input_.Row(3, viewport);
            if (!check(click(row) && *sound.choice == 3 && !runtime.choice_input_.Opened() && runtime.mouse_buttons_ == 0,
                "pointer selection consumes both click halves and commits the chosen sound")) return false;
            click(soundRow);
            const bool enabled = runtime.chat_.State().enabled;
            if (!check(click(runtime.ConfigLayout().ModuleTab(0)) && !runtime.choice_input_.Opened() && runtime.active_module_ == &runtime.chat_ &&
                runtime.chat_.State().enabled == enabled, "an outside dropdown click closes it without activating the config beneath")) return false;
            ui::choice_dropdown nearEdge; nearEdge.Open(sound, {1550, 880, 148, 24});
            const auto menu = nearEdge.Bounds(viewport);
            if (!check(menu.x >= 0 && menu.y >= 0 && menu.x + menu.width <= 1600 && menu.y + menu.height <= 900,
                "dropdown menus stay inside the viewport near its bottom/right edges")) return false;
            click(soundRow); runtime.active_config_section_ = 6; runtime.NormalizeConfigSelection();
            if (!check(!runtime.choice_input_.Opened() && !runtime.KeyboardFocused(), "changing config context dismisses stale sound choices")) return false;
            const auto color = runtime.chat_.Option(runtime.GroupOptionIndex(0));
            const auto colorRow = runtime.ConfigLayout().OptionRow(0);
            if (!check(color.kind == module_option_kind::color_override && !*color.toggle, "chat colors start in Game mode")) return false;
            click(ui::config_layout::OverrideSource(colorRow));
            if (!check(*color.toggle, "source control enables the XIUI color override")) return false;
            click(ui::config_layout::OverrideSwatch(colorRow));
            if (!check(runtime.active_color_ == color.color, "override swatches open the shared color picker")) return false;
            runtime.color_picker_.Open(0xFF5599BB); runtime.ApplyPickedColor();
            const auto custom = *color.color;
            click(ui::config_layout::OverrideSource(colorRow));
            if (!check(!*color.toggle && *color.color == custom && runtime.active_color_ == nullptr,
                "returning to Game keeps the chosen custom color and closes its picker")) return false;
            runtime.editing_ = false;
            return true;
        }

        static auto ChatAlertInput(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.chat_;
            runtime.active_config_section_ = 5;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            const auto keywordIndex = runtime.GroupOptionIndex(0);
            const auto option = runtime.chat_.Option(keywordIndex);
            if (!check(option.kind == module_option_kind::text_input && option.text != nullptr, "Sounds starts with an editable keywords field")) return false;
            const auto field = ui::config_layout::TextValue(runtime.ConfigLayout().OptionRow(0));
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            click(field);
            if (!check(runtime.option_text_input_.Focused() && runtime.KeyboardFocused() && !runtime.ChatFocused(),
                "keyword editing owns the keyboard instead of the chat composer")) return false;
            runtime.option_text_input_.Insert("Alpha, beta");
            runtime.HandleConfigInputKey(VK_RETURN, 0);
            if (!check(*option.text == "Alpha, beta" && !runtime.KeyboardFocused(), "Enter applies keywords and releases keyboard ownership")) return false;
            click(field); runtime.option_text_input_.Insert("discard this"); runtime.HandleConfigInputKey(VK_ESCAPE, 0);
            if (!check(*option.text == "Alpha, beta" && !runtime.option_text_input_.Focused(), "Escape discards a keyword edit")) return false;
            click(field); runtime.option_text_input_.Insert(std::string(120, 'a'));
            if (!check(runtime.option_text_input_.Editor().Text().size() == 120, "keyword lists can exceed the profile-name field's length")) return false;
            runtime.option_text_input_.Insert(std::string(501, 'b'));
            if (!check(runtime.option_text_input_.Editor().Text().size() == 120, "overlong keyword input is rejected without truncating the draft")) return false;
            runtime.HandleConfigInputKey(VK_TAB, 0);
            if (!check(option.text->size() == 120 && !runtime.KeyboardFocused(), "Tab applies a longer keyword list")) return false;
            click(field); runtime.option_text_input_.Insert("outside, click");
            click(runtime.ConfigLayout().OptionRow(1));
            if (!check(*option.text == "outside, click" && !runtime.option_text_input_.Focused() && runtime.choice_input_.Opened(), "clicking another control applies keywords before its own action")) return false;
            runtime.CloseChoiceInput();
            click(field); runtime.option_text_input_.Insert("cancel on navigation");
            runtime.active_config_section_ = 0; runtime.NormalizeConfigSelection();
            if (!check(*option.text == "outside, click" && !runtime.KeyboardFocused(), "a changed config context cancels stale text editing")) return false;
            ui::text_input profileName; profileName.Open(); profileName.Insert(std::string(65, 'a'));
            if (!check(profileName.Editor().Text().empty(), "profile name inputs retain their original length limit")) return false;
            runtime.editing_ = false;
            return true;
        }

        static auto ConfigCollapse(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.active_module_ = &runtime.target_;
            runtime.active_config_section_ = 3;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto original = runtime.config_bounds_;
            const auto send = [&](uint32_t message, uint32_t buttons, const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + 3); runtime.mouse_y_ = static_cast<int>(at.y + 3);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            const auto click = [&](const ui::rect& at) {
                const bool down = send(WM_LBUTTONDOWN, MK_LBUTTON, at);
                return send(WM_LBUTTONUP, 0, at) && down;
            };
            const auto eye = runtime.ConfigLayout().HeaderAction(1);
            if (!check(!hud_runtime::Contains(runtime.ConfigLayout().HeaderDragArea(), eye.x + 3, eye.y + 3),
                "the eye button is excluded from title-bar dragging")) return false;
            if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, eye) && runtime.config_collapsed_ &&
                send(WM_LBUTTONDOWN, MK_LBUTTON, eye) && runtime.config_collapsed_ && send(WM_LBUTTONUP, 0, eye),
                "one physical eye-button click collapses once and captures its release")) return false;
            if (!check(runtime.ConfigVisibleBounds().height == 32 && runtime.config_bounds_.height == original.height &&
                runtime.active_module_ == &runtime.target_ && runtime.active_config_section_ == 3,
                "collapse preserves the full size and selected settings")) return false;
            for (auto* module : runtime.modules_.All()) { module->State().bounds = {-1000, -1000, 100, 60}; module->State().content_bounds = {}; }
            runtime.mouse_x_ = 60; runtime.mouse_y_ = 140;
            if (!check(!runtime.UpdateMouseCapture(viewport, 0) && runtime.ConfigSoundTarget(viewport) == 0 &&
                runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
                "the hidden sidebar leaves no mouse capture, sound or cursor targets")) return false;
            runtime.player_.State().bounds = {400, 180, 420, 110};
            const ui::rect preview{430, 200, 1, 1};
            if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, preview) && runtime.interaction_ == hud_runtime::interaction::drag_module &&
                runtime.active_module_ == &runtime.player_ && runtime.config_collapsed_,
                "HUD previews inside the former config body remain selectable and draggable")) return false;
            send(WM_LBUTTONUP, 0, preview);
            if (!check(click(runtime.ConfigLayout().HeaderAction(1)) && !runtime.config_collapsed_ &&
                runtime.config_bounds_.width == original.width && runtime.config_bounds_.height == original.height,
                "the eye restores the complete configuration at its saved size")) return false;
            if (!check(click(runtime.ConfigLayout().HeaderAction(1)) && runtime.config_collapsed_, "configuration can collapse again")) return false;
            const ui::rect title{runtime.config_bounds_.x + 20, runtime.config_bounds_.y + 5, 1, 1};
            if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, title) && runtime.interaction_ == hud_runtime::interaction::drag_config,
                "the collapsed title bar remains draggable")) return false;
            send(WM_LBUTTONUP, 0, title);
            if (!check(click(runtime.ConfigLayout().HeaderAction()) && !runtime.config_collapsed_ && runtime.IsAttribution(),
                "attribution expands the window into its settings")) return false;
            return true;
        }

        static auto CloseInput(void) -> bool
        {
            class mouse final : public IMouse
            {
            public:
                bool blocked{};
                void AddCallback(const char*, mousecallback_f) override {}
                void RemoveCallback(const char*) override {}
                bool GetBlockInput(void) const override { return this->blocked; }
                void SetBlockInput(bool value) override { this->blocked = value; }
            } input;
            hud_runtime runtime;
            runtime.mouse_ = &input;
            runtime.config_bounds_ = {40, 40, 680, 430};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto close = runtime.ConfigLayout().CloseButton();
            const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
                runtime.mouse_x_ = static_cast<int>(x);
                runtime.mouse_y_ = static_cast<int>(y);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            for (const bool previouslyBlocked : {false, true})
            {
                input.blocked = previouslyBlocked;
                runtime.SetEditing(true, false);
                if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 3, close.y + 3) && runtime.IsEditing() && input.blocked,
                    "pressing close keeps config open and game mouse input blocked until release")) return false;
                if (!check(send(WM_MOUSEMOVE, MK_LBUTTON, 1200, 5) && input.blocked && runtime.IsEditing(),
                    "moving off close while held cannot reach the game camera")) return false;
                if (!check(send(WM_LBUTTONUP, 0, 1200, 5) && runtime.IsEditing() && !runtime.mouse_down_,
                    "releasing outside cancels close and consumes the release")) return false;
                runtime.UpdateMouseCapture(viewport, 0);
                if (!check(input.blocked == previouslyBlocked, "cancelled close restores normal hover capture")) return false;
                if (!check(!send(WM_LBUTTONDOWN, MK_LBUTTON, 1200, 5) &&
                    !send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && runtime.IsEditing(),
                    "a game-owned press cannot close config by releasing over X")) return false;
                send(WM_LBUTTONDBLCLK, MK_LBUTTON, close.x + 3, close.y + 3);
                runtime.UpdateMouseCapture(viewport, MK_LBUTTON);
                if (!check(runtime.IsEditing() && input.blocked, "held-frame updates keep the close gesture captured")) return false;
                if (!check(send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && !runtime.IsEditing() &&
                    input.blocked == previouslyBlocked && !runtime.mouse_down_ && runtime.mouse_buttons_ == 0 &&
                    runtime.interaction_ == hud_runtime::interaction::none,
                    "close consumes its matching release before restoring the original game mouse block state")) return false;
            }
            runtime.SetEditing(true, false);
            send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 3, close.y + 3);
            runtime.FinishInteraction();
            if (!check(send(WM_LBUTTONUP, 0, close.x + 3, close.y + 3) && runtime.IsEditing(),
                "cancelling an interaction cannot later activate close")) return false;
            runtime.SetEditing(false);
            return true;
        }

        static auto ProfileDeletion(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-profile-delete-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            if (!check(std::filesystem::create_directory(root), "profile deletion uses an isolated directory")) return false;
            struct cleanup
            {
                std::filesystem::path root;
                ~cleanup()
                {
                    std::error_code error;
                    const auto profiles = this->root / "xiuinative-profiles";
                    for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                    std::filesystem::remove(profiles, error);
                    std::filesystem::remove(this->root / "xiuinative.ini", error);
                    std::filesystem::remove(this->root, error);
                }
            } cleanupFiles{root};
            hud_runtime runtime; runtime.editing_ = true; runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.config_bounds_ = {30, 30, 1100, 600}; runtime.profiles_.Initialize(runtime.settings_path_);
            runtime.player_.State().bounds.x = 1200;
            runtime.SaveSettings();
            if (!check(runtime.DrainSettings(), "Default is saved before testing deletion")) return false;
            const auto defaultPath = std::filesystem::path{runtime.profiles_.Current().path};
            std::string error;
            if (!check(!runtime.profiles_.Remove(0, error) && std::filesystem::exists(defaultPath), "Default is protected at the storage layer")) return false;
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::create); runtime.profile_name_.Insert("Temporary");
            if (!check(runtime.CreateProfile() && runtime.DrainSettings(), "a disposable profile can be created")) return false;
            runtime.player_.State().bounds.x = 1450;
            runtime.SaveSettings(); runtime.DrainSettings();
            const auto removedPath = runtime.profiles_.Current().path;
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
                const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                return runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && down;
            };
            const auto open = [&] {
                return click(runtime.ConfigLayout().ProfileActionsButton()) && click(runtime.ConfigLayout().ProfileActionRow(ui::profile_delete_action));
            };
            if (!check(open() && runtime.profile_dialog_ == hud_runtime::profile_dialog::delete_profile && runtime.KeyboardFocused() &&
                runtime.profile_name_.Editor().Text() == "Temporary" && std::filesystem::exists(removedPath),
                "Delete profile opens a named confirmation and owns input without removing the file")) return false;
            runtime.HandleProfileKey('A', 0);
            if (!check(runtime.profile_name_.Editor().Text() == "Temporary", "the profile being deleted is read-only")) return false;
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Enter defaults to Cancel")) return false;
            if (!check(open(), "the confirmation can be reopened")) return false;
            const ui::profile_dialog_layout dialog{runtime.config_bounds_, viewport};
            click(dialog.Cancel());
            if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Cancel keeps the selected profile")) return false;
            if (!check(open(), "a new delete confirmation can be requested")) return false;
            runtime.HandleProfileKey(VK_ESCAPE, 0);
            if (!check(!runtime.profile_name_.Focused() && std::filesystem::exists(removedPath), "Escape cancels deletion")) return false;
            const auto locked = ::CreateFileA(removedPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (!check(locked != INVALID_HANDLE_VALUE, "the disposable profile can simulate a deletion failure")) return false;
            const bool removed = runtime.profiles_.Remove(runtime.profiles_.Selected(), error);
            ::CloseHandle(locked);
            if (!check(!removed && !error.empty() && runtime.profiles_.Current().name == "Temporary" && std::filesystem::exists(removedPath),
                "failed file removal preserves the catalog and active profile")) return false;
            if (!check(open(), "the profile remains available after a failed removal")) return false;
            const auto held = defaultPath.string() + ".held";
            std::filesystem::rename(defaultPath, held);
            const bool missingFallback = runtime.DeleteProfile();
            std::filesystem::rename(held, defaultPath);
            if (!check(!missingFallback && !runtime.profile_error_.empty() && runtime.profile_name_.Focused() && std::filesystem::exists(removedPath),
                "an unavailable Default leaves the profile and confirmation intact")) return false;
            click(dialog.Create());
            if (!check(runtime.DrainSettings() && !runtime.profile_name_.Focused() && runtime.profiles_.Selected() == 0 &&
                runtime.player_.State().bounds.x == 1200 && !std::filesystem::exists(removedPath),
                "confirming Delete removes the file and restores Default without overwriting its layout")) return false;
            runtime.SaveSettings(); runtime.DrainSettings();
            settings_profiles catalog; catalog.Initialize(runtime.settings_path_);
            if (!check(catalog.Entries().size() == 1 && catalog.Selected() == 0 && !std::filesystem::exists(removedPath),
                "autosave and restarting the profile catalog do not recreate deleted profiles")) return false;
            click(runtime.ConfigLayout().ProfileActionsButton());
            const auto disabled = runtime.ConfigLayout().ProfileActionRow(ui::profile_delete_action);
            runtime.mouse_x_ = static_cast<int>(disabled.x + 3); runtime.mouse_y_ = static_cast<int>(disabled.y + 3);
            if (!check(runtime.ProfileSoundTarget(viewport) == 0 && !runtime.OpenProfileDialog(hud_runtime::profile_dialog::delete_profile),
                "Default has no enabled delete action")) return false;
            const auto& theme = runtime.design_system_.GetTheme();
            for (const bool highlighted : {false, true})
            {
                const auto color = ui::button_renderer::FaceColor(theme, highlighted, ui::button_tone::danger);
                const auto text = ui::button_renderer::TextStyle(theme, runtime.config_controls_.Style(theme.body_text, true), highlighted, ui::button_tone::danger);
                if (!check(((color >> 16) & 255) > ((color >> 8) & 255) && ((color >> 16) & 255) > (color & 255) && text.shadow.enabled && text.stroke.enabled,
                    "Delete uses a red native button tint in both states while retaining config text effects")) return false;
            }
            runtime.editing_ = false;
            return true;
        }

        static auto Profiles(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-profiles-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            if (!check(std::filesystem::create_directory(root), "profile check owns an isolated directory")) return false;
            struct cleanup
            {
                std::filesystem::path root;
                ~cleanup()
                {
                    std::error_code error;
                    const auto profiles = this->root / "xiuinative-profiles";
                    for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                    std::filesystem::remove(profiles, error);
                    std::filesystem::remove(this->root / "xiuinative.ini", error);
                    std::filesystem::remove(this->root, error);
                }
            } cleanupFiles{root};
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.config_bounds_ = {30, 30, 1100, 600};
            runtime.profiles_.Initialize(runtime.settings_path_);
            runtime.player_.State().bounds = {1210, 300, 400, 110};
            runtime.player_.Settings().show_identity = true;
            runtime.SaveSettings();
            if (!check(runtime.DrainSettings() && runtime.profiles_.Current().name == "Default", "existing settings become the initial Default profile")) return false;
            std::string renameError;
            if (!check(!runtime.ProfileActionAvailable(ui::profile_rename_action) &&
                !runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename) && !runtime.profiles_.Rename(0, "Other", renameError) &&
                runtime.profiles_.Current().name == "Default", "Default cannot be renamed through the menu, dialog or storage")) return false;
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            click(runtime.ConfigLayout().ProfileAdd());
            if (!check(runtime.profile_name_.Focused() && runtime.KeyboardFocused(), "plus opens the create dialog and captures keyboard input")) return false;
            const auto before = runtime.player_.State().bounds;
            click(before);
            if (!check(runtime.profile_name_.Focused() && runtime.interaction_ == hud_runtime::interaction::none && runtime.player_.State().bounds.x == before.x,
                "modal dialog prevents preview clicks and dragging underneath it")) return false;
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Entries().size() == 1, "blank names remain in the dialog with an error")) return false;
            runtime.profile_name_.Insert("  Combat  ");
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(!runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Combat" && runtime.profiles_.Entries().size() == 2,
                "Enter creates, trims and selects a copy of the current setup")) return false;
            runtime.player_.Settings().show_identity = false;
            runtime.player_.State().bounds.x = 1450;
            runtime.SaveSettings(); runtime.FlushSettings();
            if (!check(runtime.SelectProfile(0) && runtime.player_.Settings().show_identity && runtime.player_.State().bounds.x == 1210,
                "switching back restores Default's own preferences and layout")) return false;
            if (!check(runtime.SelectProfile(1) && !runtime.player_.Settings().show_identity && runtime.player_.State().bounds.x == 1450,
                "automatic profile saving retains edits when switching away and back")) return false;
            click(runtime.ConfigLayout().ProfileAdd());
            runtime.profile_name_.Insert("combat"); runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Entries().size() == 2,
                "case-insensitive duplicate names do not replace an existing profile")) return false;
            runtime.HandleProfileKey(VK_ESCAPE, 0);
            if (!check(!runtime.profile_name_.Focused() && !runtime.KeyboardFocused(), "Escape cancels and releases dialog keyboard ownership")) return false;
            click(runtime.ConfigLayout().ProfileSelector());
            if (!check(runtime.profile_dropdown_ && runtime.KeyboardFocused(), "the selector opens a real dropdown")) return false;
            click(runtime.ConfigLayout().ProfileRow(0, 2));
            if (!check(!runtime.profile_dropdown_ && runtime.profiles_.Selected() == 0, "clicking a dropdown item switches profiles")) return false;
            click(runtime.ConfigLayout().ProfileSelector());
            click({1750, 800, 10, 10});
            if (!check(!runtime.profile_dropdown_, "an outside click dismisses the dropdown without moving a preview")) return false;
            if (!check(runtime.SelectProfile(1), "named profile can be selected again")) return false;
            runtime.FlushSettings();
            settings_profiles restored; restored.Initialize(runtime.settings_path_);
            if (!check(restored.Current().name == "Combat" && restored.Entries().size() == 2, "profile catalog and active selection survive a fresh initialization")) return false;
            const auto original = runtime.profiles_.Current();
            if (!check(runtime.ObserveProfileCharacter("rename-check"), "renaming retains an existing character binding")) return false;
            click(runtime.ConfigLayout().ProfileActionsButton());
            click(runtime.ConfigLayout().ProfileActionRow(ui::profile_rename_action));
            if (!check(runtime.profile_dialog_ == hud_runtime::profile_dialog::rename && runtime.profile_name_.Editor().Text() == "Combat",
                "the custom profile menu opens Rename with its current name")) return false;
            runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("Discarded");
            runtime.HandleProfileKey(VK_ESCAPE, 0);
            if (!check(runtime.profiles_.Current().name == "Combat", "canceling Rename keeps the existing name")) return false;
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
            for (const auto* invalid : {"  ", "default"})
            {
                runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert(invalid);
                runtime.HandleProfileKey(VK_RETURN, 0);
                if (!check(runtime.profile_name_.Focused() && !runtime.profile_error_.empty() && runtime.profiles_.Current().name == "Combat",
                    "blank and duplicate rename attempts keep the dialog and profile intact")) return false;
            }
            runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("  Raid UI  ");
            click(ui::profile_dialog_layout{runtime.config_bounds_, viewport}.Create());
            if (!check(!runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Raid UI",
                "the Rename button trims and commits the name")) return false;
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
            runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("raid ui");
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(runtime.profiles_.Current().name == "raid ui", "Enter permits changing only the name's case")) return false;
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::rename);
            runtime.profile_name_.SelectAll(); runtime.profile_name_.Insert("Combat UI");
            runtime.HandleControllerConfig(controller_action::right, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "raid ui", "held controller confirmation cannot rename")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            if (!check(runtime.DrainSettings() && !runtime.profile_name_.Focused() && runtime.profiles_.Current().name == "Combat UI" &&
                runtime.profiles_.Current().id == original.id && runtime.profiles_.Current().path == original.path &&
                runtime.profiles_.Entries().size() == 2 && runtime.player_.State().bounds.x == 1450,
                "controller Rename preserves profile identity, count and layout")) return false;
            restored.Initialize(runtime.settings_path_);
            if (!check(restored.Current().name == "Combat UI" && restored.Current().id == original.id &&
                restored.BoundProfile("rename-check") == restored.Selected() &&
                settings_store{original.path}.ReadInt("layout.player", "x", 0) == 1450,
                "the renamed profile, preferences and character binding survive reloading")) return false;
            std::string error;
            auto snapshot = settings_store::FromIniText("[player]\nenabled=0\n[session.exp.42]\nreceived=123\n[session.gil]\nnet=456\n[profiles]\nactive=wrong\n");
            if (!check(restored.Create("Separate", snapshot, error), "profile snapshots can be created independently")) return false;
            const auto preferences = restored.Read(restored.Selected(), error);
            if (!check(preferences && !preferences->ReadBool("player", "enabled", true) && preferences->ReadInt("session.exp.42", "received", -1) == -1 &&
                preferences->ReadInt("session.gil", "net", -1) == -1 && preferences->ReadString("profiles", "active", "missing") == "missing",
                "profile files exclude EXP/gil sessions and active-profile metadata")) return false;
            auto merged = settings_store::FromIniText("[session.exp.42]\nreceived=999\n"); merged.OverlayPreferences(snapshot);
            if (!check(merged.ReadInt("session.exp.42", "received", 0) == 999 && !merged.ReadBool("player", "enabled", true), "applying profile preferences preserves live session data")) return false;
            const ui::config_layout compact{{0, 0, 936, 350}, 15, false, true};
            const auto lastTab = compact.ModuleTab(14), selector = compact.ProfileSelector();
            runtime.editing_ = false;
            return check(lastTab.y + lastTab.height + 8 <= selector.y && selector.x < ui::config_layout::sidebar_width,
                "profile controls fit the bottom-left sidebar at the minimum window height");
        }

        static auto CharacterProfiles(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-character-profiles-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            if (!check(std::filesystem::create_directory(root), "character profile check owns an isolated directory")) return false;
            struct cleanup
            {
                std::filesystem::path root;
                ~cleanup()
                {
                    std::error_code error;
                    const auto profiles = this->root / "xiuinative-profiles";
                    for (const auto& file : std::filesystem::directory_iterator{profiles, error}) std::filesystem::remove(file.path(), error);
                    std::filesystem::remove(profiles, error);
                    std::filesystem::remove(this->root / "xiuinative.ini", error);
                    std::filesystem::remove(this->root, error);
                }
            } cleanupFiles{root};
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.profiles_.Initialize(runtime.settings_path_);
            runtime.player_.State().bounds.x = 100;
            runtime.SaveSettings(); runtime.FlushSettings();
            const auto create = [&](const char* name) {
                return runtime.OpenProfileDialog(hud_runtime::profile_dialog::create) &&
                    (runtime.profile_name_.Insert(name), runtime.CreateProfile());
            };
            if (!check(runtime.ObserveProfileCharacter("world-1-11") && create("Character A"),
                "a first character adopts the compatibility profile and can select a new shared profile")) return false;
            const auto a = runtime.profiles_.Selected();
            const auto aId = runtime.profiles_.Current().id;
            runtime.player_.State().bounds.x = 137; runtime.SaveSettings();
            if (!check(runtime.ObserveProfileCharacter("world-1-22") && create("Character B"),
                "another character can select a different profile from the same library")) return false;
            const auto b = runtime.profiles_.Selected();
            const auto bId = runtime.profiles_.Current().id;
            runtime.player_.State().bounds.x = 240; runtime.SaveSettings();
            runtime.chat_.AdoptDraft({"/p unsent draft", 15});
            runtime.OpenProfileDialog(hud_runtime::profile_dialog::delete_profile);
            if (!check(runtime.ObserveProfileCharacter("world-1-11") && runtime.profiles_.Selected() == a &&
                runtime.player_.State().bounds.x == 137 && !runtime.profile_name_.Focused(),
                "character changes drain pending saves, restore the bound layout and cancel destructive dialogs")) return false;
            runtime_state draft{}; runtime.CaptureChat(draft);
            if (!check(std::strcmp(draft.chat_draft, "/p unsent draft") == 0 && !draft.chat_focused,
                "automatic profile changes retain unsent chat without submitting or capturing typing")) return false;
            runtime.FlushSettings();
            std::string error;
            const auto savedB = runtime.profiles_.Read(b, error);
            if (!check(savedB && savedB->ReadRect("layout.player", {}).x == 240,
                "the previous character's pending layout is saved into its own profile")) return false;

            runtime.ObserveProfileCharacter("");
            runtime.player_.State().bounds.x = 999; runtime.SaveSettings(); runtime.FlushSettings();
            const auto savedA = runtime.profiles_.Read(a, error);
            if (!check(savedA && savedA->ReadRect("layout.player", {}).x == 137 && !runtime.profile_character_ready_,
                "unknown login identity cannot overwrite the last character's profile")) return false;
            if (!check(runtime.ObserveProfileCharacter("world-1-22") && runtime.player_.State().bounds.x == 240,
                "the second character restores its own selection after an unknown login interval")) return false;
            runtime.FlushSettings();

            if (!check(runtime.profiles_.Create("Broken", runtime.CaptureSettings(), error), "create a separate invalid-load fixture")) return false;
            const auto broken = runtime.profiles_.Current();
            auto file = settings_store{broken.path}; file.WriteInt("profile", "version", 2);
            if (!check(file.Flush(), "mark only the fixture profile unsupported")) return false;
            runtime.profiles_.Select(b);
            auto bindings = settings_store{runtime.settings_path_};
            bindings.WriteString("profiles.characters", "world-2-11", broken.id);
            bindings.WriteString("profiles.characters", "world-1-33", "profile-missing");
            if (!check(bindings.Flush(), "write local character bindings")) return false;
            if (!check(!runtime.ObserveProfileCharacter("world-2-11") && !runtime.profile_character_ready_ &&
                runtime.profiles_.Selected() == b && runtime.player_.State().bounds.x == 240 && !runtime.profile_error_.empty(),
                "an unsupported bound profile keeps committed settings and suspends profile autosaving")) return false;
            runtime.player_.State().bounds.x = 777; runtime.SaveSettings(); runtime.FlushSettings();
            const auto protectedB = runtime.profiles_.Read(b, error);
            if (!check(protectedB && protectedB->ReadRect("layout.player", {}).x == 240 && runtime.SelectProfile(b),
                "failed character loads cannot contaminate another profile and explicit selection recovers")) return false;
            runtime.FlushSettings();
            if (!check(runtime.profiles_.BoundProfile("world-1-11") == a && runtime.profiles_.BoundProfile("world-2-11") == b,
                "the same character ID on two worlds has independent profile bindings")) return false;
            if (!check(runtime.ObserveProfileCharacter("world-1-33") && runtime.profiles_.Selected() == 0 &&
                runtime.player_.State().bounds.x == 100, "missing profile IDs fall back to Default through the normal load path")) return false;
            runtime.FlushSettings();
            const auto local = settings_store{runtime.settings_path_};
            if (!check(local.ReadString("profiles.characters", "world-1-11") == aId &&
                local.ReadString("profiles.characters", "world-1-22") == bId &&
                local.ReadString("profiles.characters", "world-1-33") == "default",
                "fallback repairs only the current binding and retains other characters' selections")) return false;

            hud_runtime fresh;
            fresh.settings_path_ = runtime.settings_path_;
            fresh.LoadSettings(); fresh.profiles_.Initialize(fresh.settings_path_);
            if (!check(fresh.ObserveProfileCharacter("world-1-11") && fresh.profiles_.Current().id == aId &&
                fresh.player_.State().bounds.x == 137, "a fresh runtime restores the character binding independently of the last global active profile")) return false;
            fresh.FlushSettings();
            auto portable = settings_store::FromIniText("[PROFILES.Characters]\nworld-1-11=foreign\n[profiles.other]\nlocal=private\n[player]\nenabled=0\n");
            const auto exported = portable.Preferences("");
            auto merged = settings_store::FromIniText("[profiles.characters]\nworld-1-11=local\n");
            merged.OverlayPreferences(portable);
            const auto key = EncodeProfileKey("Portable", portable, error);
            const auto decoded = DecodeProfileKey(key, error);
            return check(exported.ReadString("profiles.characters", "world-1-11").empty() &&
                exported.ReadString("profiles.other", "local").empty() && merged.ReadString("profiles.characters", "world-1-11") == "local" &&
                decoded && decoded->preferences.ReadString("profiles.characters", "world-1-11").empty(),
                "profile files, overlays and share keys exclude local character bindings case-insensitively");
        }

        static auto PreviewFocus(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {30, 30, 1100, 600};
            runtime.chat_second_.State().enabled = true;
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            for (auto* module : runtime.modules_.All()) { module->State().bounds = {-1000, -1000, 100, 60}; module->State().content_bounds = {}; }
            const auto send = [&](uint32_t message, uint32_t buttons, int x, int y) {
                runtime.mouse_x_ = x; runtime.mouse_y_ = y;
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            for (auto* module : runtime.modules_.All())
            {
                if (!module->HasEditableBounds() || !module->PreviewVisible()) continue;
                module->State().bounds = {1300, 200, 400, 260};
                runtime.active_module_ = &runtime.game_hud_;
                runtime.active_config_section_ = 7;
                runtime.option_scroll_ = 6;
                send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 220);
                const bool notificationGroup = std::string_view{module->Id()}.starts_with("notifications.group");
                const auto* expectedModule = notificationGroup ? &runtime.notifications_ : module;
                if (!check(runtime.active_module_ == expectedModule && runtime.ActiveConfigPage().Contains(expectedModule) &&
                    runtime.active_config_section_ == (notificationGroup ? 2 : 0) && runtime.option_scroll_ == 0 && runtime.interaction_module_ == module,
                    "a preview press focuses its module or parent group editor and begins dragging the exact module")) return false;
                send(WM_LBUTTONUP, 0, 1320, 220);
                if (!check(module->State().bounds.x == 1300 && module->State().bounds.y == 200, "a stationary preview click leaves its position unchanged")) return false;
                module->State().bounds = {-1000, -1000, 100, 60};
            }
            runtime.player_.State().bounds = {1300, 200, 400, 260};
            runtime.active_module_ = &runtime.player_;
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::text);
            send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 220);
            send(WM_MOUSEMOVE, MK_LBUTTON, 1370, 250);
            send(WM_LBUTTONUP, 0, 1380, 260);
            if (!check(runtime.player_.State().bounds.x == 1360 && runtime.player_.State().bounds.y == 240 &&
                runtime.active_config_section_ == static_cast<size_t>(bar_config_section::text), "dragging still uses the release position and keeps the selected module's topic")) return false;
            runtime.alliance_second_.hide_preview_ = true;
            runtime.alliance_second_.State().bounds = {1360, 240, 400, 260};
            send(WM_LBUTTONDOWN, MK_LBUTTON, 1380, 260);
            if (!check(runtime.active_module_ == &runtime.player_, "hidden previews cannot steal module selection")) return false;
            send(WM_LBUTTONUP, 0, 1380, 260);
            const auto active = runtime.active_module_;
            send(WM_LBUTTONDOWN, MK_LBUTTON, 80, 42); send(WM_LBUTTONUP, 0, 80, 42);
            if (!check(runtime.active_module_ == active, "config header clicks retain the current module")) return false;
            auto& world = runtime.world_info_;
            world.State().enabled = true;
            world.State().bounds = {1300, 700, 300, 60};
            runtime.FocusModuleConfig(world);
            const auto toggleWorld = [&] {
                const auto row = runtime.ConfigLayout().OptionRow(0);
                const int x = static_cast<int>(row.x + row.width * 0.5f), y = static_cast<int>(row.y + row.height * 0.5f);
                send(WM_LBUTTONDOWN, MK_LBUTTON, x, y); send(WM_LBUTTONUP, 0, x, y);
            };
            toggleWorld();
            const auto movable = runtime.ControllerMovableModules();
            if (!check(!world.State().enabled && !world.PreviewVisible() && !runtime.MovementToolbar(viewport) &&
                std::find(movable.begin(), movable.end(), &world) == movable.end(),
                "disabling World Info removes its edit preview, toolbar and controller movement target")) return false;
            runtime.active_module_ = &runtime.player_;
            send(WM_MOUSEMOVE, 0, 1320, 720);
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
                "disabled World Info has no drag cursor")) return false;
            send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 720);
            if (!check(runtime.active_module_ == &runtime.player_ && runtime.interaction_module_ == nullptr,
                "a disabled preview cannot capture clicks or begin dragging")) return false;
            send(WM_LBUTTONUP, 0, 1320, 720);
            runtime.FocusModuleConfig(world);
            toggleWorld();
            if (!check(world.PreviewVisible() && world.State().bounds.x == 1300 && world.State().bounds.y == 700,
                "reenabling World Info restores its preview at the same position")) return false;
            *world.Option(4).toggle = true;
            *world.Option(6).choice = 2; world.OnOptionChanged(6);
            for (auto& part : runtime.world_parts_) part.State().bounds = {-1000, -1000, 100, 22};
            for (size_t i = 0; i < runtime.world_parts_.size(); ++i)
            {
                auto& part = runtime.world_parts_[i];
                part.State().bounds = {1300, 700, 160, 22};
                send(WM_LBUTTONDOWN, MK_LBUTTON, 1320, 710);
                if (!check(runtime.active_module_ == &world && runtime.active_config_section_ == 1 && world.SelectedPart() == i &&
                    runtime.MovementSelectionModule() == &part && runtime.interaction_module_ == &part,
                    "independent World Info previews select their own movement handle within the shared Layout page")) return false;
                send(WM_MOUSEMOVE, MK_LBUTTON, 1340, 740); send(WM_LBUTTONUP, 0, 1340, 740);
                const auto targets = runtime.ControllerMovableModules();
                if (!check(part.State().bounds.x == 1320 && part.State().bounds.y == 730 && world.State().bounds.x == 1300 &&
                    std::find(targets.begin(), targets.end(), &part) != targets.end(),
                    "each World Info piece moves independently and remains available to controller movement")) return false;
                part.State().bounds = {-1000, -1000, 100, 22};
            }
            world.State().enabled = false;
            for (auto& part : runtime.world_parts_)
                if (!check(!part.PreviewVisible(), "disabling World Info removes every independent edit preview")) return false;
            const auto closedSelection = runtime.active_module_;
            runtime.editing_ = false;
            runtime.FocusModuleConfig(runtime.target_);
            return check(runtime.active_module_ == closedSelection, "preview focus is limited to edit mode");
        }

        static auto ExperienceDisplay(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.experience_;
            runtime.active_config_section_ = 1;
            runtime.config_bounds_ = {30, 30, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1600, 1000, 0, 1};
            auto& exp = runtime.experience_;
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                const auto target = runtime.ConfigSoundTarget(viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
                return target != 0;
            };
            if (!check(runtime.IsExperienceDisplay() && ControlCount(runtime) == 7, "bar-info tab exposes only current progress controls")) return false;
            const auto initial = exp.Settings().readouts;
            if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, 3)) &&
                ControlCount(runtime) == 0 && exp.Settings().readouts == initial,
                "Readouts tab replaces cycling selectors without changing saved preferences")) return false;
            const ui::experience_display_editor::geometry geometry{runtime.ConfigLayout()};
            if (!check(geometry.presets.y + geometry.presets.height <= runtime.ConfigLayout().ContentArea().y + runtime.ConfigLayout().ContentArea().height,
                "readout lists, help and actions stay inside the content area")) return false;
            const auto last = static_cast<size_t>(experience_readout::last);
            if (!check(click(geometry.Row(false, 0)) && exp.Settings().readouts[last], "available reading adds directly without a cycling selector")) return false;
            const auto lastOrder = exp.Settings().order[last];
            for (size_t index = 0; index < experience_readout_count; ++index)
                if (!check(!initial[index] || exp.Settings().order[index] < lastOrder, "newly added readouts append after visible readings")) return false;
            click(ui::config_controls::Segment(geometry.actions, 0, 3));
            if (!check(exp.Settings().order[last] < lastOrder, "Move up operates on adjacent visible items")) return false;
            click(ui::config_controls::Segment(geometry.actions, 2, 3));
            if (!check(!exp.Settings().readouts[last], "Remove hides just the selected reading")) return false;

            const auto allTarget = runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10);
            runtime.experience_display_.Scroll(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10, -3);
            if (!check(runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10) != allTarget,
                "available catalog scrolls independently")) return false;
            click(ui::config_controls::Segment(geometry.filters, 4, 5));
            const auto bonusTarget = runtime.experience_display_.Target(exp, runtime.ConfigLayout(), geometry.available.x + 8, geometry.available.y + 10);
            if (!check(bonusTarget >= 0x200 && experience_readouts[bonusTarget - 0x200].group == experience_readout_group::bonuses,
                "category filters reset scrolling and expose matching available readings")) return false;
            const ui::rect clear{geometry.presets.x + 138, geometry.presets.y, 72, 26};
            click(clear);
            if (!check(std::none_of(exp.Settings().readouts.begin(), exp.Settings().readouts.end(), [](bool value) { return value; }), "Clear removes readouts while retaining bar info")) return false;
            click({geometry.presets.x, geometry.presets.y, 132, 26});
            if (!check(exp.Settings().readouts == DefaultExperienceReadouts() && exp.Settings().order == DefaultExperienceReadoutOrder(),
                "Recommended restores the compact job/session/chain defaults")) return false;

            if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 2, 3)) && ControlCount(runtime) == 11,
                "Layout groups bar dimensions and readout spacing separately")) return false;
            exp.Settings().bar_gap = 17;
            exp.Settings().info_position = 1;
            exp.Settings().details_text.offset_y = 25;
            exp.Settings().readout_text.offset_y = -12;
            const ui::rect meter{0, 50, 500, 22};
            const auto infoBottom = meter.y + meter.height + std::max(16.0f, runtime.design_system_.TextHeight(exp.Settings().details_text.style)) * 0.5f + 25;
            if (!check(exp.ReadoutArea(runtime.design_system_, meter, true, 16).y == infoBottom + 17 - 12,
                "readout Y moves freely relative to the reserved bar-info space")) return false;
            exp.Settings().show_current = true;
            if (!check(exp.ReadoutRepeatsBar(static_cast<size_t>(experience_readout::buffer)) &&
                !exp.ReadoutRepeatsBar(static_cast<size_t>(experience_readout::received)), "current EXP overlap is identified separately from session gains")) return false;
            auto store = settings_store::FromIniText(""); exp.Save(store);
            experience_module restored; restored.ApplyTextDefaults(ui::theme::CreateDefault()); restored.Load(store);
            if (!check(restored.Settings().readouts == exp.Settings().readouts && restored.Settings().order == exp.Settings().order && restored.Settings().bar_gap == 17,
                "editor visibility, ordering and bar gap use existing settings persistence")) return false;
            const ui::config_layout compact{{0, 0, 936, 350}, 15, false, true, 40};
            const ui::experience_display_editor::geometry compactLists{compact};
            return check(compactLists.rows >= 1 && compactLists.presets.y + compactLists.presets.height <= compact.ContentArea().y + compact.ContentArea().height,
                "the editor remains usable at the minimum config size");
        }

        inline static size_t hover_sounds{}, select_sounds{}, menu_sounds{};
        static auto __cdecl HoverSound(void) -> void { ++hover_sounds; }
        static auto __cdecl SelectSound(void) -> void { ++select_sounds; }
        static auto __cdecl MenuSound(void) -> void { ++menu_sounds; }

        static auto Sounds(void) -> bool
        {
            class mouse final : public IMouse
            {
            public:
                bool blocked{};
                void AddCallback(const char*, mousecallback_f) override {}
                void RemoveCallback(const char*) override {}
                bool GetBlockInput(void) const override { return this->blocked; }
                void SetBlockInput(bool value) override { this->blocked = value; }
            } input;
            hud_runtime runtime;
            runtime.mouse_ = &input;
            auto& sounds = runtime.ui_sounds_;
            sounds.sounds_[0].function = reinterpret_cast<uintptr_t>(&HoverSound);
            sounds.sounds_[1].function = reinterpret_cast<uintptr_t>(&SelectSound);
            sounds.sounds_[2].function = reinterpret_cast<uintptr_t>(&MenuSound);
            hover_sounds = select_sounds = menu_sounds = 0;
            if (!check(runtime.SetEditing(true) && menu_sounds == 1, "opening config plays native menu feedback")) return false;
            runtime.SetEditing(true);
            if (!check(menu_sounds == 1, "already-open config does not replay its opening sound")) return false;
            runtime.SetEditing(false);
            runtime.RestoreEditing(true);
            if (!check(menu_sounds == 1, "hot-reload edit restoration stays silent")) return false;

            sounds.MenuOpened(1000, false);
            sounds.Hover(1, 1001); sounds.Hover(2, 1050); sounds.Hover(2, 1200);
            if (!check(hover_sounds == 0, "opening and stationary hover do not add incidental sounds")) return false;
            sounds.Hover(3, 1201); sounds.Hover(3, 1300);
            sounds.Hover(4, 1400, false); sounds.Hover(4, 1500);
            if (!check(hover_sounds == 1, "entering a control plays once and dragging stays quiet")) return false;
            sounds.Hover(0, 1600); sounds.Hover(4, 1700);
            sounds.Select(1800); sounds.Hover(5, 1850); sounds.Hover(6, 2000);
            if (!check(hover_sounds == 3 && select_sounds == 1, "re-entry plays cursor feedback and selection suppresses incidental hover")) return false;
            sounds.sounds_[0].cache_slot = 1;
            if (!check(!sounds.Play(ui::native_ui_sound::hover) && hover_sounds == 3, "unreadable native resource state fails silently")) return false;
            sounds.sounds_[0].cache_slot = 0;

            const D3DVIEWPORT8 viewport{0, 0, 1600, 900, 0, 1};
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const auto at = [&](const ui::rect& bounds) { runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3); };
            const auto press = [&] { runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport); };
            const auto release = [&] { runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport); };
            at(runtime.ConfigLayout().ModuleTab(0));
            if (!check(runtime.ConfigSoundTarget(viewport) != 0, "config navigation has a hover identity")) return false;
            const auto before = select_sounds;
            press(); press(); release();
            if (!check(select_sounds == before + 1, "duplicate pointer messages play only one selection")) return false;
            runtime.mouse_x_ = 1100; runtime.mouse_y_ = 630;
            if (!check(runtime.ConfigSoundTarget(viewport) == 0, "blank configuration space is silent")) return false;
            runtime.active_module_ = &runtime.player_;
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::bar_style);
            runtime.NormalizeConfigSelection();
            const auto row = runtime.ConfigLayout().OptionRow(3);
            at(ui::config_layout::SliderTrack(row));
            const auto sliderBefore = select_sounds;
            press(); runtime.mouse_x_ += 50; runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport); release();
            if (!check(select_sounds == sliderBefore + 1, "a complete slider drag plays only its initial click")) return false;
            runtime.active_color_ = &runtime.player_.Settings().hp_color;
            at(ui::color_picker_layout{runtime.ConfigLayout().ColorEditor(viewport)}.Close());
            if (!check(runtime.ConfigSoundTarget(viewport) != 0, "color-picker controls share sound hit testing")) return false;
            runtime.active_color_ = nullptr;

            runtime.equipment_.State().bounds = {1200, 300, 206, 226};
            at(runtime.equipment_.SlotBounds(0));
            const auto firstSlot = runtime.HudSoundTarget();
            at(runtime.equipment_.SlotBounds(1));
            if (!check(firstSlot != 0 && runtime.HudSoundTarget() != 0 && firstSlot != runtime.HudSoundTarget(), "equipment squares have separate hover identities")) return false;
            runtime.equipment_.State().enabled = false;
            if (!check(runtime.HudSoundTarget() == 0, "hidden equipment produces no hover feedback")) return false;

            runtime.SetEditing(false);
            auto& ready = runtime.ready_check_;
            const auto now = ::GetTickCount64();
            const std::array<ready_check_member, 2> roster{{{1, 0, 1, "One"}, {2, 1, 2, "Two"}}};
            if (!check(ready.check_.Start(roster, 1, 1, now), "ready-check mock starts without sending messages")) return false;
            ready.State().bounds = {0, 0, 300, 180}; ready.buttons_[0] = {20, 20, 100, 30};
            const auto readyBefore = select_sounds;
            ready.HandleMouse(WM_LBUTTONDOWN, 30, 30, now);
            ready.HandleMouse(WM_LBUTTONDOWN, 30, 30, now);
            ready.HandleMouse(WM_MOUSEMOVE, 40, 30, now);
            ready.HandleMouse(WM_LBUTTONUP, 400, 400, now);
            if (!check(select_sounds == readyBefore + 1 && ready.ButtonAt(30, 30) == 1 && ready.ButtonAt(10, 10) == 0,
                "Ready Check buttons play once per press, with no sound for window padding")) return false;
            runtime.mouse_ = nullptr;
            return true;
        }

        static auto SliderValues(void) -> bool
        {
            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-slider-input-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            hud_runtime runtime;
            runtime.settings_path_ = path.string();
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::bar_style);
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto row = runtime.ConfigLayout().OptionRow(3);
            const auto field = ui::config_layout::SliderValue(row);
            const auto click = [&](const float x, const float y) {
                runtime.mouse_x_ = static_cast<int>(x); runtime.mouse_y_ = static_cast<int>(y);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto open = [&] { click(field.x + field.width / 2, field.y + field.height / 2); };
            const auto height = runtime.player_.Settings().hp_style.height;
            open();
            if (!check(runtime.slider_input_.Focused() && runtime.KeyboardFocused() && !runtime.ChatFocused() &&
                runtime.player_.Settings().hp_style.height == height && runtime.interaction_ == hud_runtime::interaction::none,
                "clicking a slider value captures typing without dragging, changing its value or focusing chat")) return false;
            runtime.slider_input_.Insert("23");
            runtime.HandleConfigInputKey(VK_RETURN, 0x40000000);
            if (!check(runtime.slider_input_.Focused() && runtime.player_.Settings().hp_style.height == height,
                "a held Enter cannot commit a newly opened value")) return false;
            runtime.HandleConfigInputKey(VK_RETURN, 0);
            if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 23 &&
                runtime.player_.Settings().mp_style.height == 23 && runtime.player_.Settings().tp_style.height == 23,
                "typed values apply through the module callback and release keyboard capture")) return false;
            runtime.FlushSettings();
            player_module loaded;
            loaded.Load(settings_store{path.string()});
            if (!check(loaded.Settings().hp_style.height == 23, "typed slider values persist through the shared settings path")) return false;
            open(); runtime.slider_input_.Insert("-");
            runtime.HandleConfigInputKey(VK_RETURN, 0);
            if (!check(runtime.slider_input_.Focused() && runtime.slider_input_.Invalid() && runtime.player_.Settings().hp_style.height == 23,
                "incomplete numbers stay editable without modifying the saved value")) return false;
            runtime.HandleConfigInputKey(VK_ESCAPE, 0);
            if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 23, "Escape cancels numeric editing")) return false;
            open(); runtime.slider_input_.Insert("999"); runtime.HandleConfigInputKey(VK_RETURN, 0);
            if (!check(runtime.player_.Settings().hp_style.height == 80, "typed values respect the slider's maximum")) return false;
            open(); runtime.slider_input_.Insert("31"); click(1200, 5);
            if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 31, "clicking outside applies a valid pending value and releases typing")) return false;
            open(); runtime.slider_input_.Insert("42"); runtime.active_module_ = &runtime.target_;
            runtime.NormalizeConfigSelection();
            if (!check(!runtime.KeyboardFocused() && runtime.player_.Settings().hp_style.height == 31, "changing config pages cancels stale numeric input")) return false;
            runtime.active_module_ = &runtime.player_;
            const auto track = ui::config_layout::SliderTrack(row);
            runtime.mouse_x_ = static_cast<int>(track.x); runtime.mouse_y_ = static_cast<int>(track.y);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.mouse_x_ = static_cast<int>(std::ceil(track.x + track.width));
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.player_.Settings().hp_style.height == 80 && !runtime.slider_input_.Focused(), "slider dragging still uses its final release position")) return false;

            float value = 0.4f;
            ui::slider_value_input input;
            input.Open(module_option::Slider("Value", "", &value, 0.1f, 1, 0.05f));
            input.Insert("0.83");
            if (!check(input.Value() && std::abs(*input.Value() - 0.85f) < 0.00001f && value == 0.4f,
                "fractional entry snaps to the same step grid as dragging without applying while typing")) return false;
            input.Open(module_option::Slider("Offset", "", &value, -200, 200, 1));
            input.Insert("-45");
            if (!check(input.Value() && *input.Value() == -45, "negative offsets support exact entry")) return false;
            input.SelectAll(); input.Insert("1..2");
            if (!check(!input.Value() && input.Invalid(), "malformed numbers cannot reach module settings")) return false;
            input.SelectAll(); input.Insert("1e999");
            if (!check(!input.Value(), "overflowing numeric input is rejected")) return false;
            if (!check(ui::slider_value_input::Format(0.85f, 0.05f) == "0.85" && ui::slider_value_input::Format(31, 1) == "31",
                "value fields retain the precision needed for each slider step")) return false;
            for (const float width : {269.0f, 351.0f, 439.0f})
            {
                const ui::rect bounds{0, 0, width, 56};
                const auto slider = ui::config_layout::SliderTrack(bounds), number = ui::config_layout::SliderValue(bounds);
                if (!check(slider.width > 0 && slider.x + slider.width < number.x && number.x + number.width <= bounds.width &&
                    number.y + number.height < ui::config_layout::Description(bounds).y,
                    "slider fields and tracks fit narrow columns without covering descriptions")) return false;
            }
            return true;
        }

        static auto ColorPicker(void) -> bool
        {
            using picker = ui::color_picker;
            for (uint32_t i = 0; i < 4096; ++i)
            {
                const D3DCOLOR color = i * 2654435761u;
                if (!check(picker::ToColor(picker::FromColor(color)) == color, "HSV round-trips preserve all RGB and alpha bytes")) return false;
            }
            const ui::rect panel{30, 40, 300, 404};
            const ui::color_picker_layout layout{panel};
            const auto wheel = layout.Wheel(), value = layout.Value(), alpha = layout.Alpha();
            const float cx = wheel.x + wheel.width * 0.5f, cy = wheel.y + wheel.height * 0.5f;
            picker model;
            model.Open(0x40000000);
            model.BeginDrag(picker::part::wheel); model.Update(panel, cx, wheel.y);
            const auto black = model.Value();
            model.BeginDrag(picker::part::value); model.Update(panel, value.x + 4, value.y - 20);
            if (!check(black.value == 0 && black.saturation == 1 && model.Value().hue == black.hue &&
                (model.Color() & 0xFFFFFF) != 0 && (model.Color() >> 24) == 0x40, "hue and saturation chosen at black survive brightness changes without losing alpha")) return false;
            model.BeginDrag(picker::part::wheel); model.Update(panel, cx, cy);
            if (!check(model.Value().saturation == 0 && (model.Color() & 0xFFFFFF) == 0xFFFFFF,
                "the center of the wheel selects white at full brightness")) return false;
            model.BeginDrag(picker::part::alpha); model.Update(panel, alpha.x - 30, alpha.y + 4);
            if (!check(model.Color() == 0x00FFFFFF && model.Hex() == "#FFFFFF00", "opacity clamps at zero while preserving RGB and uses RGBA hex order")) return false;
            model.Update(panel, alpha.x + alpha.width + 30, alpha.y + 4);
            if (!check(model.Color() == 0xFFFFFFFF && model.Hex() == "#FFFFFF", "opaque colors copy ordinary RGB hex")) return false;
            model.Restore(); model.EndDrag();
            if (!check(model.Color() == 0x40000000 && model.Dragging() == picker::part::none &&
                picker::HitTest(panel, wheel.x, wheel.y) != picker::part::wheel && picker::HitTest(panel, cx, cy) == picker::part::wheel,
                "restore retains the opening color and wheel corners do not claim drags")) return false;
            for (const auto viewport : {D3DVIEWPORT8{0, 0, 1280, 720, 0, 1}, D3DVIEWPORT8{0, 0, 640, 480, 0, 1}, D3DVIEWPORT8{0, 0, 320, 240, 0, 1}})
            {
                const auto bounds = ui::config_layout{{40, 40, 1100, 600}}.ColorEditor(viewport);
                const ui::color_picker_layout controls{bounds};
                if (!check(bounds.x >= 0 && bounds.y >= 0 && bounds.x + bounds.width <= viewport.Width && bounds.y + bounds.height <= viewport.Height &&
                    controls.Hex().y + controls.Hex().height <= bounds.y + bounds.height,
                    "the complete picker and controls remain inside small viewports")) return false;
            }

            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-wheel-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            hud_runtime runtime;
            runtime.settings_path_ = path.string(); runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.active_module_ = &runtime.player_;
            runtime.player_.Settings().style_edit_mode = 1;
            runtime.player_.Settings().hp_color_top = 0x40FF0000;
            runtime.active_color_ = &runtime.player_.Settings().hp_color_top;
            runtime.color_picker_.Open(*runtime.active_color_);
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto editor = runtime.ConfigLayout().ColorEditor(viewport);
            const ui::color_picker_layout editorLayout{editor};
            const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
                runtime.mouse_x_ = static_cast<int>(x); runtime.mouse_y_ = static_cast<int>(y);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            const auto disc = editorLayout.Wheel();
            const bool enabled = runtime.player_.State().enabled;
            send(WM_LBUTTONDOWN, MK_LBUTTON, disc.x + disc.width * 0.5f, disc.y + disc.height - 1);
            send(WM_MOUSEMOVE, MK_LBUTTON, disc.x + disc.width + 40, disc.y + disc.height * 0.5f);
            send(WM_LBUTTONUP, 0, disc.x + disc.width + 40, disc.y + disc.height * 0.5f);
            if (!check(runtime.interaction_ == hud_runtime::interaction::none && runtime.color_picker_.Dragging() == picker::part::none &&
                !runtime.mouse_down_ && runtime.player_.State().enabled == enabled && (*runtime.active_color_ >> 24) == 0x40,
                "wheel drags retain alpha, cannot click through to config, and release outside the disc")) return false;
            const auto opacity = editorLayout.Alpha();
            send(WM_LBUTTONDOWN, MK_LBUTTON, opacity.x + opacity.width * 0.5f, opacity.y + 4);
            send(WM_LBUTTONUP, 0, opacity.x + opacity.width * 0.5f, opacity.y + 4);
            const auto picked = *runtime.active_color_;
            runtime.FlushSettings();
            player_module restored;
            restored.Load(settings_store{path.string()});
            if (!check(restored.Settings().hp_color_top == picked && (picked >> 24) >= 126 && (picked >> 24) <= 128,
                "picker opacity survives the normal asynchronous save and module reload")) return false;
            const auto original = editorLayout.Original();
            send(WM_LBUTTONDOWN, MK_LBUTTON, original.x + 5, original.y + 5);
            send(WM_LBUTTONUP, 0, original.x + 5, original.y + 5);
            if (!check(*runtime.active_color_ == 0x40FF0000, "the original swatch restores the opening color through normal config input")) return false;
            const auto close = editorLayout.Close();
            send(WM_LBUTTONDOWN, MK_LBUTTON, close.x + 4, close.y + 4);
            send(WM_LBUTTONUP, 0, close.x + 4, close.y + 4);
            if (!check(runtime.active_color_ == nullptr && runtime.interaction_ == hud_runtime::interaction::none, "the close button dismisses the picker without leaving capture active")) return false;
            const auto legacy = settings_store::FromIniText("[meter.player.hp]\nfill_top=123456\n[meter.target.hp]\nfill_top=40556677\n[meter.experience]\nfill_top=8099AABB\n");
            restored.Load(legacy);
            target_module target; target.Load(legacy);
            experience_module experience; experience.Load(legacy);
            return check(restored.Settings().hp_color_top == 0xFF123456 && target.Settings().hp_color_top == 0x40556677 && experience.Settings().color_top == 0x8099AABB,
                "legacy RGB colors remain opaque while target and EXP retain alpha colors");
        }

        static auto StatusSnapshots(void) -> bool
        {
            std::array<int16_t, 32> localIcons{};
            std::array<uint8_t, 32> loadingIcons{};
            std::array<uint16_t, 32> loadingOutput{};
            if (!check(!LocalStatusSnapshotReady(localIcons) && !PartyStatusSnapshotReady(loadingIcons, 0) &&
                DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 0,
                "zero-filled zoning snapshots cannot produce rows of KO icons")) return false;
            localIcons.fill(-1); localIcons[0] = 0;
            loadingIcons.fill(255); loadingIcons[0] = 0;
            if (!check(LocalStatusSnapshotReady(localIcons) && PartyStatusSnapshotReady(loadingIcons, 0) &&
                DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 1 && loadingOutput[0] == 0,
                "a genuine KO followed by empty status slots remains visible")) return false;
            localIcons.fill(255); loadingIcons.fill(255);
            if (!check(LocalStatusSnapshotReady(localIcons) && PartyStatusSnapshotReady(loadingIcons, 0) &&
                DecodePartyBuffs(loadingIcons, 0, loadingOutput) == 0,
                "initialized empty snapshots remain authoritative")) return false;
            localIcons[0] = 32767;
            if (!check(!LocalStatusSnapshotReady(localIcons), "invalid local slot data stays unavailable")) return false;
            std::array<uint8_t, 32> icons{}; icons.fill(255);
            icons[0] = 33; icons[1] = 1; icons[2] = 255; icons[3] = 0; icons[4] = 255;
            std::array<uint16_t, 32> decoded{};
            const uint64_t mask = (1ULL << 2) | (1ULL << 4) | (3ULL << 6);
            if (!check(DecodePartyBuffs(icons, mask, decoded) == 4 && decoded[0] == 33 && decoded[1] == 257 && decoded[2] == 511 && decoded[3] == 768,
                "party status high bits preserve IDs above 255, including a low byte of 255")) return false;
            if (!check(DecodePartyBuffs({}, 0, decoded) == 0 && DecodePartyBuffs(icons, mask, std::span{decoded}.first(1)) == 1,
                "party status decoding respects both input and output bounds")) return false;
            return true;
        }

        static auto Party(void) -> bool
        {
            if (!StatusSnapshots()) return false;
            if (!check(PartyResourceMaximum(0, 0) == 0 && PartyResourceMaximum(803, 80) == 1004 &&
                FormatPartyResource(0, 0, 0, false, 4) == "0/?" && FormatPartyResource(803, 80, 1004, false, 4) == "803/~1004" &&
                FormatPartyResource(62, 61, 101, true, 4) == "62/101" && FormatPartyResource(62, 61, 101, true, 5).empty(),
                "party displays distinguish missing, exact and rounded maximum resources")) return false;
            if (!check(PartyJobHasMP(3) && PartyJobHasMP(22) && !PartyJobHasMP(1) && !PartyJobHasMP(0) &&
                std::strcmp(PartyJobName(22), "RUN") == 0 && std::strcmp(PartyJobName(255), "") == 0,
                "zero MP does not hide caster bars or invent unknown job icons")) return false;
            party_cast cast{"Cure", 1, 1000, 0, 2};
            if (!check(cast.Active(4499) && !cast.Active(4500), "party casts expire even when a finish packet is absent")) return false;
            cast.interrupted = 1500;
            if (!check(cast.Active(2299) && !cast.Active(2300), "interrupted party casts clear their display promptly")) return false;

            party_source source;
            source.player_id_ = 100; source.zone_ = 100;
            auto& player = source.members_[0];
            player.active = player.in_zone = true; player.id = 100; player.slot = 0; player.name = "Player";
            player.hp = 80; player.hp_percent = 80; player.main_job = 1;
            party_module party, alliance{1}, third{2};
            for (auto* module : {&party, &alliance, &third}) module->SetSource(&source);
            hud_snapshot snapshot{};
            if (!check(party.ShouldRender(snapshot) && !alliance.ShouldRender(snapshot) && !third.ShouldRender(snapshot),
                "empty alliance lists remain hidden while the solo party row is visible")) return false;
            party.solo_ = false;
            if (!check(!party.ShouldRender(snapshot), "solo visibility follows its own preference")) return false;
            source.members_[7] = player; source.members_[7].id = 200; source.members_[7].slot = 7;
            if (!check(source.Count(1) == 1 && source.Count(2) == 0 && source.InAlliance() && party.ShouldRender(snapshot) && alliance.ShouldRender(snapshot),
                "alliance membership follows native slots without requiring a full six-member block")) return false;
            player.index = 101; player.entity_valid = true;
            source.members_[7].index = 102; source.members_[7].entity_valid = true;
            source.UpdateSelection(101, 102, true, -1);
            if (!check(player.selected && player.subtarget && source.members_[7].selected && !source.members_[7].subtarget,
                "entity subtarget highlighting distinguishes the cursor from the held target")) return false;
            source.members_[7].in_zone = source.members_[7].entity_valid = false;
            source.UpdateSelection(101, 0, false, 7);
            if (!check(source.members_[7].selected && source.members_[7].subtarget && player.selected && !player.subtarget,
                "party cursor selection can highlight out-of-zone members without replacing the held target")) return false;
            source.cutscene_ = true;
            if (!check(!party.ShouldRender(snapshot) && !alliance.ShouldRender(snapshot), "event hiding applies to each list")) return false;
            source.cutscene_ = false;
            party.preview_ = false; party.bottom_ = true;
            ui::design_system design;
            module_render_context context{design};
            party.Render(snapshot, context);
            const float bottom = party.state_.bounds.y + party.state_.bounds.height;
            source.members_[3] = player; source.members_[3].id = 300; source.members_[3].slot = 3;
            party.Render(snapshot, context);
            if (!check(bottom == party.state_.bounds.y + party.state_.bounds.height, "bottom alignment keeps a fixed anchor as membership grows")) return false;
            party.ResetDefaults(design.GetTheme());
            if (!check(party.source_ == &source && party.text_[0].style.shadow.enabled == design.GetTheme().title_text.shadow.enabled &&
                party.meters_[0].appearance == ui::meter_style::window_theme, "reset preserves the live source and shared theme effects")) return false;
            if (!check(party.job_placement_ == party_module::jobs_name && party.Option(49).choice_count == 2,
                "job/subjob defaults to the name row with a separate-row option")) return false;
            if (!check(!party.labels_ && party.Option(51).choice_count == 2 && party.Option(51).available && party.Option(51).visible,
                "party resource labels default off and TP offers a numbers-only mode")) return false;
            party.tp_display_ = 1; player.tp = 1500;
            party.Render(snapshot, context);
            party.tp_ = false;
            if (!check(!party.Option(51).visible, "hiding TP also hides its display selector")) return false;
            party.tp_ = true; party.tp_display_ = 0;
            const float inlineJobs = party.EntryHeight();
            party.job_placement_ = party_module::jobs_below;
            if (!check(party.EntryHeight() > inlineJobs, "a separate job row reserves its own text height")) return false;
            party.job_placement_ = party_module::jobs_name;
            player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4};
            for (const int placement : {party_module::cast_separate, party_module::cast_name, party_module::cast_mp, party_module::cast_tp})
            {
                party.cast_placement_ = placement;
                const float height = party.MemberHeight(player);
                party.Render(snapshot, context);
                player.cast = {};
                party.Render(snapshot, context);
                if (!check(party.MemberHeight(player) <= height, "finished casts release their visible space without enlarging the entry")) return false;
                player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4};
            }
            player.cast = {"Cure IV", 4, ::GetTickCount64() - 500, 0, 4}; player.main_job = 1; player.sub_job = 13;
            party.tp_ = false; party.cast_placement_ = party_module::cast_mp;
            if (!check(party.ResourceSlots(player).mp && !party.ResourceSlots(player).mp_values,
                "active MP replacement supplies a cast slot even for jobs without MP")) return false;
            party.cast_placement_ = party_module::cast_tp;
            if (!check(party.ResourceSlots(player).tp && !party.ResourceSlots(player).tp_values,
                "active TP replacement remains available when idle TP is hidden")) return false;
            party.meters_[3].height = 30;
            if (!check(party.ResourceBarHeight(true, true, true) >= party.CastHeight(), "active replacement rows fit their cast style")) return false;
            party.cast_placement_ = party_module::cast_name;
            if (!check(party.HeaderHeight(true) >= party.CastHeight(), "active name replacement fits its cast style")) return false;
            party.casts_ = false;
            if (!check(!party.Option(48).visible, "cast placement is hidden when casts are hidden")) return false;
            party.ResetDefaults(design.GetTheme());
            party.meters_[0].height = 1; party.meters_[1].height = 8;
            const ui::rect resourceRow{0, 0, 200, party.ResourceBarHeight()};
            if (!check(ui::LayoutMeterTrack(resourceRow, party.meters_[0]).height == 1 &&
                ui::LayoutMeterTrack(resourceRow, party.meters_[1]).height == 8,
                "party resource bars keep independent pixel heights within a shared row")) return false;
            party.ResetDefaults(design.GetTheme());
            party.layout_ = party_module::bars_mp_below_tp_left;
            const auto stacked = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(party.Option(21).choice_count == 3 && stacked.bars[1].x == stacked.bars[0].x &&
                stacked.bars[1].width == stacked.bars[0].width && stacked.bars[1].y > stacked.bars[0].y + stacked.bars[0].height &&
                stacked.bars[2].x + stacked.bars[2].width < stacked.bars[0].x &&
                stacked.bars[2].y + stacked.bars[2].height * 0.5f == 20 + stacked.height * 0.5f,
                "MP stacks directly beneath HP while TP occupies a centered column to their left")) return false;
            const auto noMP = party.LayoutResources({10, 20, 360, 0}, {false, true});
            const auto noTP = party.LayoutResources({10, 20, 360, 0}, {true, false});
            if (!check(noMP.height == party.ResourceBarHeight() && noMP.bars[1].height == 0 && noTP.bars[0].x == 10 &&
                noTP.bars[0].width == 360 && noTP.bars[2].height == 0,
                "missing MP collapses its row and hidden TP gives the stacked bars their full width")) return false;
            party.tp_display_ = 1;
            const auto numberTP = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(numberTP.bars[2].x == stacked.bars[2].x && numberTP.height == stacked.height,
                "numbers-only TP retains its left column without changing the MP/HP stack")) return false;
            party.cast_placement_ = party_module::cast_mp;
            const auto replacement = party.LayoutResources({10, 20, 360, 0}, party.ResourceSlots(player));
            if (!check(replacement.bars[1].height > 0,
                "cast replacement can reserve the lower MP row for a job without MP")) return false;
            party.Render(snapshot, context);
            party.ResetDefaults(design.GetTheme());
            party.style_mode_ = 1; party.selected_bar_ = 0;
            *party.Option(52).slider = 120; party.OnOptionChanged(52);
            party.selected_bar_ = 1;
            *party.Option(52).slider = 90; party.OnOptionChanged(52);
            const auto custom = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(custom.bars[0].width == 120 && custom.bars[1].width == 90 && custom.bars[2].width == 130 &&
                custom.bars[2].x + custom.bars[2].width == 370 && party.bar_widths_[2] == 0,
                "per-bar pixel widths retain automatic neighbors and fit the complete row")) return false;
            party.bar_widths_ = {800, 800, 0, 140};
            const auto constrained = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(constrained.bars[0].width > 0 && constrained.bars[1].width > 0 && constrained.bars[2].width >= 24 &&
                constrained.bars[2].x + constrained.bars[2].width <= 370.01f && party.BarWidth(3, 90) == 90 && party.BarWidth(3, 200) == 140,
                "oversized resource and cast widths remain within their available row or replacement slot")) return false;
            party.layout_ = party_module::bars_mp_below_tp_left; party.bar_widths_ = {200, 150, 60, 140};
            const auto customStack = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(customStack.bars[0].width == 200 && customStack.bars[1].width == 150 && customStack.bars[2].width == 60 &&
                customStack.bars[0].x == customStack.bars[1].x && customStack.bars[2].x + customStack.bars[2].width < customStack.bars[0].x,
                "stacked HP, MP and left TP retain independent widths and column clearance")) return false;
            party.layout_ = party_module::bars_vertical;
            const auto customVertical = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(customVertical.bars[0].width == 200 && customVertical.bars[1].width == 150 && customVertical.bars[2].width == 60,
                "compact vertical layout honors each resource width")) return false;
            party.state_.maximum_width = 2560;
            if (!check(party.Option(52).maximum == party.BarWidthLimit() && party.Option(52).maximum < party.state_.bounds.width,
                "per-bar width is limited to party content rather than the viewport width")) return false;
            party.layout_ = party_module::bars_mp_below_tp_left; party.selected_bar_ = 1;
            *party.Option(54).slider = -7; party.OnOptionChanged(54);
            *party.Option(55).slider = 20; party.OnOptionChanged(55);
            for (const int position : {party_module::position_left, party_module::position_center, party_module::position_right})
            {
                *party.Option(53).choice = position; party.OnOptionChanged(53);
                const auto positioned = party.LayoutResources({10, 20, 360, 0}, {true, true});
                const float factor = position == party_module::position_right ? 1.0f : position == party_module::position_center ? 0.5f : 0;
                if (!check(positioned.bars[1].x == 10 + (360 - positioned.bars[1].width) * factor - 7 &&
                    positioned.bars[1].y == customStack.bars[1].y + 20 && positioned.bars[0].x == customStack.bars[0].x &&
                    positioned.height >= positioned.bars[1].y + positioned.bars[1].height - 20,
                    "MP anchors and offsets move only its bar while row bounds include the moved content")) return false;
            }
            party.bar_offsets_y_[1] = -4;
            const auto lifted = party.LayoutResources({10, 20, 360, 0}, {true, true});
            if (!check(lifted.height < customStack.height, "moving MP upward retains compact spacing beneath the resource rows")) return false;
            party.bar_positions_[3] = party_module::position_right; party.bar_offsets_x_[3] = -3; party.bar_offsets_y_[3] = 6;
            const auto placedCast = party.CastBounds({10, 20, 200, 16});
            if (!check(placedCast.x == 67 && placedCast.y == 26 && placedCast.width == 140,
                "cast positioning uses its own alignment, width and offsets within the selected placement")) return false;
            party.style_mode_ = 0; party.selected_bar_ = 2;
            *party.Option(52).slider = 72; party.OnOptionChanged(52);
            if (!check(std::all_of(party.bar_widths_.begin(), party.bar_widths_.end(), [](float value) { return value == 72; }),
                "shared bar settings apply a width to HP, MP, TP and Cast together")) return false;
            *party.Option(53).choice = party_module::position_center; party.OnOptionChanged(53);
            *party.Option(54).slider = 4; party.OnOptionChanged(54);
            *party.Option(55).slider = 8; party.OnOptionChanged(55);
            if (!check(std::all_of(party.bar_positions_.begin(), party.bar_positions_.end(), [](int value) { return value == party_module::position_center; }) &&
                std::all_of(party.bar_offsets_x_.begin(), party.bar_offsets_x_.end(), [](float value) { return value == 4; }) &&
                std::all_of(party.bar_offsets_y_.begin(), party.bar_offsets_y_.end(), [](float value) { return value == 8; }),
                "shared bar settings apply position and both offsets consistently")) return false;
            party.ResetDefaults(design.GetTheme());
            party.job_icons_ = false; party.tp_ = false; party.cast_placement_ = party_module::cast_name;
            party.bar_widths_ = {184, 122, 0, 0}; party.layout_ = party_module::bars_vertical;
            party.bar_positions_[0] = party.bar_positions_[1] = party_module::position_right;
            party.bar_offsets_x_[0] = -23; party.bar_offsets_x_[1] = -25; party.bar_offsets_y_[1] = -17;
            const auto beforeFit = party.LayoutContainer();
            if (!check(beforeFit.width < party.state_.bounds.width && beforeFit.inset == 23,
                "fitted containers account for existing negative bar offsets without clipping their left edges")) return false;
            party.UseCompactLayout();
            const auto fitted = party.LayoutContainer();
            if (!check(fitted.width == 184 + 2 * (party.container_.edge_size + party.container_.padding + party.SelectionPadding().x) && fitted.canvas_width == 184 &&
                party.bar_widths_[1] == 122 && party.cast_placement_ == party_module::cast_name && !party.tp_ &&
                party.bar_offsets_x_[1] == 0 && party.bar_offsets_y_[1] == 0,
                "compact layout clears overlap offsets while keeping chosen bar widths, cast placement and visibility")) return false;
            party.bar_widths_[0] = 140;
            const auto smaller = party.LayoutContainer();
            if (!check(smaller.width < fitted.width && party.LayoutRows(2).bounds.width == smaller.width,
                "reducing HP width shrinks the actual native container and its layout bounds")) return false;
            party.fit_width_ = false;
            if (!check(party.LayoutContainer().width == party.state_.bounds.width,
                "fixed-width mode retains the manually configured container width")) return false;
            party.ResetDefaults(design.GetTheme());
            if (!check(party.LayoutContainer().width == party.state_.bounds.width,
                "automatic HP widths retain the existing container size without feedback shrinkage")) return false;
            player.cast = {};
            party.cast_placement_ = party_module::cast_name; party.job_icons_ = false; party.spacing_ = 0;
            const float compact = party.EntryHeight();
            const auto compactLayout = party.LayoutRows(2);
            if (!check(compact < party.EntryHeight(false) && compactLayout.heights[0] == compact && compactLayout.heights[1] == compact &&
                compactLayout.bounds.height == 2 * (party.container_.edge_size + party.container_.padding) +
                    party_module::LineHeight(party.text_[party_module::heading_text]) + 6 + 2 * compact + 4 * party.SelectionPadding().y,
                "zero entry spacing retains only visible rows and their selection padding")) return false;
            player.selected = true;
            const auto selectedLayout = party.LayoutRows(2);
            player.selected = false;
            if (!check(selectedLayout.bounds.height == compactLayout.bounds.height && selectedLayout.bounds.width == compactLayout.bounds.width,
                "moving selection cannot change the space reserved for its frame")) return false;
            source.members_[3].in_zone = false;
            const auto remoteLayout = party.LayoutRows(2);
            if (!check(remoteLayout.heights[0] == compact && remoteLayout.heights[1] == party.EntryHeight(false),
                "an off-zone panel only expands its own member's entry")) return false;
            source.members_[3].in_zone = true;
            player.buff_count = 20;
            const auto statusLayout = party.LayoutRows(2);
            if (!check(statusLayout.heights[0] == party.RowHeight(true, 20) && statusLayout.heights[0] > compact &&
                statusLayout.heights[1] == compact, "wrapped visible statuses only expand their own member's row")) return false;
            party.bottom_ = true;
            const auto before = party.LayoutRows(2);
            player.buff_count = 0;
            const auto after = party.LayoutRows(2);
            if (!check(before.bounds.y + before.bounds.height == after.bounds.y + after.bounds.height,
                "bottom alignment remains fixed as a visible status container grows or shrinks")) return false;
            party.expand_ = true;
            const auto expanded = party.LayoutRows(2);
            if (!check(expanded.rows == 6 && expanded.leading_space == 4 * (compact + 2 * party.SelectionPadding().y),
                "expanded bottom-aligned parties reserve compact empty slots before their members")) return false;
            party.preview_ = true; party.bottom_ = false; party.expand_ = false;
            const auto previewLayout = party.LayoutRows(6, true);
            const module_render_context previewContext{design, nullptr, true};
            party.Render(snapshot, previewContext);
            if (!check(party.state_.bounds.height == previewLayout.bounds.height &&
                party.VisualBounds(true).height == previewLayout.bounds.height + 16,
                "preview drawing, edit bounds and variable row geometry agree")) return false;
            party.ResetDefaults(design.GetTheme());
            const float oneStatusRow = party.RowHeight(true, 32);
            party.buff_limit_ = 32;
            if (!check(party.RowHeight(true, 32) > oneStatusRow && party.BuffColumns(0) == 1,
                "all 32 statuses can wrap within the party window without overlapping another member")) return false;
            party.buff_limit_ = 12;
            const ui::rect frame{500, 50, 420, 600}, row{513, 85, 394, party.RowHeight(true, 20)};
            const auto left = party.StatusLayout(row, frame, 20);
            const auto first = party.StatusIconBounds(left, 0), wrapped = party.StatusIconBounds(left, 8);
            if (!check(left.framed && left.reverse && left.count == 12 && left.overflow == 8 && left.columns == 8 &&
                left.bounds.x + left.bounds.width + party.status_gap_ == frame.x && left.bounds.y == row.y &&
                first.x == wrapped.x && wrapped.y == first.y + party.buff_size_ + party.status_spacing_ &&
                left.bounds.height <= row.height,
                "separate status windows default left, grow away from the member and wrap inside their reserved row")) return false;
            party.status_placement_ = party_module::status_right;
            const auto right = party.StatusLayout(row, frame, 20);
            if (!check(right.framed && !right.reverse && right.bounds.x == frame.x + frame.width + party.status_gap_ &&
                party.StatusIconBounds(right, 1).x > party.StatusIconBounds(right, 0).x && right.bounds.width == left.bounds.width,
                "right-side statuses retain the same dimensions and flow away from the member")) return false;
            party.status_frame_ = false;
            const auto bare = party.StatusLayout(row, frame, 1);
            if (!check(!bare.framed && bare.bounds.width == party.buff_size_ && bare.content.x == bare.bounds.x &&
                !party.Option(46).visible, "frameless effects retain side placement and hide window-only controls")) return false;
            party.status_placement_ = party_module::status_inline;
            const auto inside = party.StatusLayout(row, frame, 20);
            if (!check(!inside.framed && inside.bounds.x == row.x && party.RowHeight(true, 20) > party.EntryHeight() &&
                !party.Option(42).visible && !party.Option(45).visible && party.StatusLayout(row, frame, 0).bounds.width == 0,
                "inline mode reserves its own space while empty effects never draw a container")) return false;
            party.status_placement_ = party_module::status_left;
            party.status_frame_ = true;
            party.buff_limit_ = 32; party.status_columns_ = 1; party.buff_size_ = 32;
            const auto tall = party.StatusLayout(row, frame, 32);
            if (!check(party.RowHeight(true, 32) >= tall.bounds.height, "large status columns cannot overlap the next party entry")) return false;
            party.ResetDefaults(design.GetTheme());
            party.state_.bounds = {0, 0, 360, 100};
            const auto reserve = party.VisualBounds(false);
            const auto maximum = party.StatusLayout({}, party.PartyBounds(2), 32);
            if (!check(reserve.x <= maximum.bounds.x && reserve.width > party.state_.bounds.width,
                "viewport bounds reserve external statuses before effects arrive")) return false;

            const auto path = std::filesystem::temp_directory_path() / ("xiui-party-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            settings_store store{path.string()};
            party.state_.bounds = {20, 30, 380, 110}; party.hp_display_ = 4; party.buffs_ = false;
            party.status_placement_ = party_module::status_right; party.status_frame_ = false;
            party.status_columns_ = 6; party.status_gap_ = 14; party.status_container_.opacity = 0.6f;
            party.meters_[1].appearance = ui::meter_style::modern; party.text_[3].offset_y = 3;
            party.text_[0].offset_y = 12; party.text_[party_module::heading_text].offset_y = -2;
            party.selected_text_ = static_cast<int>(party_module::heading_text);
            party.cast_placement_ = party_module::cast_name; party.job_placement_ = party_module::jobs_below;
            party.tp_display_ = 1; party.labels_ = true;
            party.layout_ = party_module::bars_mp_below_tp_left;
            party.bar_widths_ = {200, 150, 60, 140};
            party.bar_positions_[1] = party_module::position_center; party.bar_positions_[3] = party_module::position_right;
            party.bar_offsets_x_ = {0, 7, 0, -3}; party.bar_offsets_y_ = {0, 12, 0, 6};
            alliance.state_.bounds = {450, 30, 320, 100}; alliance.tp_ = false; third.state_.enabled = false;
            alliance.cast_placement_ = party_module::cast_tp;
            alliance.fit_width_ = false;
            party.selection_style_ = {0xFF55AAFF, 0xFFFF9955, 0.23f, 8, 5, false};
            party.Save(store); alliance.Save(store); third.Save(store); store.Flush();
            store.WriteFloat("party", "frame_size", 12); store.WriteBool("party", "scan_lines", false);
            store.WriteFloat("party", "status_frame_size", 1); store.WriteBool("party", "status_scan_lines", false);
            party_module loaded, second{1}, last{2}; loaded.Load(store); second.Load(store); last.Load(store);
            const ui::container_style nativeStyle;
            if (!check(loaded.selection_style_.target_color == 0xFF55AAFF && loaded.selection_style_.subtarget_color == 0xFFFF9955 &&
                loaded.selection_style_.background_opacity == 0.23f && loaded.SelectionPadding().x == 8 && loaded.SelectionPadding().y == 5 &&
                !loaded.selection_style_.arrow && second.selection_style_.arrow && second.selection_style_.background_opacity == 0.12f,
                "selection tint, padding, colors and arrow persist independently for each party list")) return false;
            if (!check(loaded.text_[0].offset_y == 12 && loaded.text_[party_module::heading_text].offset_y == -2 &&
                loaded.selected_text_ == party_module::heading_text && second.text_[party_module::heading_text].offset_y == 0,
                "member names and party/alliance headings retain independent text settings")) return false;
            if (!check(loaded.container_.edge_size == nativeStyle.edge_size && loaded.container_.scan_lines == nativeStyle.scan_lines &&
                loaded.status_container_.edge_size == nativeStyle.edge_size && loaded.status_container_.scan_lines == nativeStyle.scan_lines,
                "party containers use shared native defaults instead of retired frame and pattern overrides")) return false;
            if (!check(loaded.state_.bounds.x == 20 && loaded.hp_display_ == 4 && !loaded.buffs_ && loaded.meters_[1].appearance == ui::meter_style::modern &&
                loaded.text_[3].offset_y == 3 && loaded.status_placement_ == party_module::status_right && !loaded.status_frame_ &&
                loaded.status_columns_ == 6 && loaded.status_gap_ == 14 && loaded.status_container_.opacity == 0.6f &&
                loaded.cast_placement_ == party_module::cast_name && loaded.job_placement_ == party_module::jobs_below &&
                loaded.tp_display_ == 1 && loaded.labels_ && !second.labels_ && second.tp_display_ == 0 &&
                loaded.layout_ == party_module::bars_mp_below_tp_left && second.layout_ == party_module::bars_horizontal &&
                loaded.fit_width_ && !second.fit_width_ && loaded.tp_ && !second.tp_ && loaded.fills_[2] == party.fills_[2],
                "fitted width and TP visibility persist independently from TP color and other party lists")) return false;
            if (!check(
                loaded.bar_widths_ == party.bar_widths_ && second.bar_widths_ == std::array<float, 4>{} &&
                loaded.bar_positions_ == party.bar_positions_ && loaded.bar_offsets_x_ == party.bar_offsets_x_ && loaded.bar_offsets_y_ == party.bar_offsets_y_ &&
                second.bar_positions_ == std::array<int, 4>{} && second.bar_offsets_x_ == std::array<float, 4>{} && second.bar_offsets_y_ == std::array<float, 4>{} &&
                second.cast_placement_ == party_module::cast_tp && second.job_placement_ == party_module::jobs_name &&
                second.state_.bounds.x == 450 && !second.tp_ && !last.state_.enabled,
                "party and both alliance windows persist independent layout, data and shared component settings")) return false;
            store.WriteRect("layout.config", {100, 200, 820, 430}); store.Flush();
            hud_runtime runtime;
            runtime.settings_path_ = path.string(); runtime.LoadSettings();
            if (!check(runtime.config_bounds_.height == ui::config_layout::default_height && runtime.config_bounds_.width == 1100 &&
                runtime.config_bounds_.x == 100 && runtime.config_bounds_.y == 200,
                "existing default config grows to 1100 by 600 without resetting its position")) return false;
            runtime.config_bounds_.height = 520; runtime.config_bounds_.width = 1000; runtime.FlushSettings(); runtime.LoadSettings();
            if (!check(runtime.config_bounds_.height == 520 && runtime.config_bounds_.width == 1000,
                "later custom config dimensions survive the one-time migrations")) return false;

            std::array<uint8_t, 48> bitmap{};
            const auto write = [&](size_t offset, auto value) { std::memcpy(bitmap.data() + offset, &value, sizeof(value)); };
            write(0, uint32_t{40}); write(4, int32_t{1}); write(8, int32_t{2}); write(12, uint16_t{1}); write(14, uint16_t{32});
            write(40, uint32_t{0xFF123456}); write(44, uint32_t{0x80556677});
            const auto image = resources::DecodeItemIcon(bitmap);
            if (!check(image && image->pixels[0] == 0x80556677 && image->pixels[1] == 0xFF123456 &&
                !resources::DecodeItemIcon(std::span{bitmap}.first(47)), "native BGRA status icons retain alpha, orientation and bounds checks")) return false;
            source.Clear();
            return check(!source.Available() && source.Count(0) == 0 && source.Count(1) == 0 && !party.ShouldRender(snapshot),
                "zoning or logout clears membership and stale resources together");
        }

        static auto WindowFrame(const char* dat) -> bool
        {
            std::ifstream file{dat, std::ios::binary};
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
            auto window = resources::DecodeMenuWindow(bytes);
            if (!check(window && window->patches[5].additive && !window->patches[3].additive,
                "Type 7 retains its distinct native left/right blend modes")) return false;
            const auto original = window->patches[5];
            auto unchanged = *window;
            ui::container_renderer::CorrectWindowFrame(unchanged, 5);
            if (!check(unchanged.patches[5].additive && unchanged.patches[5].colors == original.colors,
                "Type 7 correction does not change other themes")) return false;
            unchanged.patches[5].colors[0] = 0x50505050;
            ui::container_renderer::CorrectWindowFrame(unchanged, 6);
            if (!check(unchanged.patches[5].additive && unchanged.patches[5].colors[0] == 0x50505050,
                "custom DAT edge colors are preserved")) return false;
            ui::container_renderer::CorrectWindowFrame(*window, 6);
            return check(window->patches[3].colors == std::array<uint32_t, 4>{0x91616161, 0x91616161, 0x91616161, 0x91616161} &&
                window->patches[5].colors == window->patches[3].colors && !window->patches[5].additive &&
                window->patches[5].u == original.u && window->patches[5].v == original.v && window->patches[5].width == original.width &&
                window->patches[5].height == original.height && window->patches[5].texture == original.texture,
                "Type 7 right edge matches the left intensity/blend while retaining its artwork and native color conversion");
        }

        static auto NativeWindowBorders(const char* type6Dat, const char* type7Dat, const char* liveLeft) -> bool
        {
            const auto load = [](const char* path) {
                std::ifstream file{path, std::ios::binary};
                return std::vector<uint8_t>{std::istreambuf_iterator<char>{file}, {}};
            };
            const auto window = resources::DecodeMenuWindow(load(type6Dat));
            if (!check(window.has_value(), "decode installed Type 6")) return false;
            const auto& top = window->patches[1]; const auto& left = window->patches[3];
            if (!check(top.additive && left.additive && top.colors == std::array<uint32_t, 4>{0x61808080, 0x61808080, 0x61808080, 0x61808080} &&
                left.colors == std::array<uint32_t, 4>{0x00808080, 0x0D808080, 0x00808080, 0x0D808080},
                "Type 6 uses native load-time color/alpha conversion, including faint gold side edges")) return false;
            if (!check(resources::MenuVertexColorFromRgba(0x00000000) == 0x00010101 &&
                resources::MenuVertexColorFromRgba(0xFFFFFFFF) == 0xFFFFFFFF &&
                resources::MenuVertexColorFromRgba(0xFE123456) == 0xFF573513 &&
                resources::MenuVertexColorFromRgba(0x017F7F7F) == 0x03808080,
                "native conversion retains transparency, clamps channels and truncates alpha like the client")) return false;
            if (liveLeft != nullptr)
            {
                const auto sprite = load(liveLeft);
                if (!check(sprite.size() >= 0x60 + 3 * 28 + 4, "live native sprite snapshot contains four vertices")) return false;
                for (size_t i = 0; i < 4; ++i)
                {
                    uint32_t diffuse{}; std::memcpy(&diffuse, sprite.data() + 0x60 + i * 28, 4);
                    if (!check(diffuse == left.colors[i], "decoded Type 6 edge matches live native frame diffuse colors exactly")) return false;
                }
            }
            return WindowFrame(type7Dat);
        }

        static auto DefaultProfile(const char* source) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-defaults-" + std::to_string(::GetCurrentProcessId()));
            std::filesystem::create_directory(root);
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanupFiles{root};
            hud_runtime runtime;
            runtime.Initialize(nullptr, root.string().c_str());
            runtime.FlushSettings();
            std::ifstream file{source, std::ios::binary};
            const std::string expected{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
            if (!check(!expected.empty() && runtime.default_settings_text_ == expected, "runtime embeds the shipped defaults profile")) return false;
            const auto preferences = [](hud_runtime& hud) { return hud.CaptureSettings().Preferences("").ToIniText(); };
            const auto baseline = preferences(runtime);
            size_t styles{}; bool fonts = runtime.general_.GlobalFont() == 0;
            for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { ++styles; fonts &= style.font == 0; });
            if (!check(fonts && styles > 70, "fresh defaults use XIUI Bahnschrift for all HUD text")) return false;
            for (auto* module : runtime.modules_.All())
            {
                module->State().bounds.x += 117; module->State().bounds.y += 83;
                module->VisitTextSettings([](ui::text_style& style) { style.font = 3; style.letter_spacing = 2; });
            }
            runtime.party_.spacing_ += 3;
            runtime.player_.Settings().name_text.offset_x = 13;
            runtime.player_.Settings().name_text.style.top_color = 0xAABBCCDD;
            auto changed = runtime.CaptureSettings();
            changed.WriteFloat("status.buffs", "columns", 2);
            changed.WriteFloat("status.debuffs", "offset_y", 36);
            changed.WriteFloat("status.party.buffs", "row_spacing", 13);
            runtime.LoadSettings(changed);
            runtime.editing_ = true; runtime.active_module_ = &runtime.party_;
            runtime.active_config_section_ = party_module::status_section;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const auto customized = preferences(runtime);
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& at) {
                runtime.mouse_x_ = static_cast<int>(at.x + 4); runtime.mouse_y_ = static_cast<int>(at.y + 4);
                const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                const bool up = runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
                return down && up;
            };
            if (!check(!runtime.ProfileActionAvailable(ui::profile_delete_action) && runtime.ProfileActionAvailable(ui::profile_reset_action),
                "Default disables Delete while keeping global Reset to defaults available")) return false;
            click(runtime.ConfigLayout().ProfileActionsButton());
            click(runtime.ConfigLayout().ProfileActionRow(ui::profile_reset_action));
            if (!check(runtime.profile_dialog_ == hud_runtime::profile_dialog::reset_defaults && runtime.profile_name_.Focused() &&
                runtime.profile_keyboard_selection_ == 0 && preferences(runtime) == customized,
                "three-dot Reset opens its Cancel-first warning from Party without changing settings")) return false;
            runtime.HandleProfileKey(VK_RETURN, 0);
            runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0x80000000u), false);
            if (!check(!runtime.profile_name_.Focused() && preferences(runtime) == customized, "default Enter cancels the entire reset")) return false;
            click(runtime.ConfigLayout().ProfileActionsButton());
            click(runtime.ConfigLayout().ProfileActionRow(ui::profile_reset_action));
            const ui::profile_dialog_layout dialog{runtime.config_bounds_, viewport};
            if (!check(click(dialog.Create()) && !runtime.ApplyConfirmedAction() && preferences(runtime) == baseline,
                "explicit pointer Reset restores all modules, text, positions and settings exactly once")) return false;
            runtime.FlushSettings(); runtime.ReloadSettings();
            if (!check(preferences(runtime) == baseline, "complete defaults persist after save and reload")) return false;
            runtime.party_.spacing_ += 6; runtime.buffs_.State().bounds.x += 200;
            runtime.active_module_ = &runtime.enemy_list_; runtime.active_config_section_ = 0;
            runtime.controller_config_.region = config_controller_region::profiles;
            runtime.controller_config_.id = "actions"; runtime.RefreshControllerConfigFocus();
            runtime.HandleControllerConfig(controller_action::confirm, false);
            for (size_t i = 0; i < 3; ++i) runtime.HandleControllerConfig(controller_action::down, false);
            if (!check(runtime.profile_keyboard_selection_ == ui::profile_reset_action, "controller navigation reaches Reset after disabled Delete")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(runtime.profile_name_.Focused() && preferences(runtime) != baseline, "held Confirm cannot accept global reset")) return false;
            runtime.HandleControllerConfig(controller_action::right, false);
            runtime.HandleControllerConfig(controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(!runtime.profile_name_.Focused() && preferences(runtime) == baseline,
                "controller Reset restores everything from another category and ignores held reactivation")) return false;
            runtime.FlushSettings();
            return check(runtime.party_.source_ == &runtime.party_source_, "global reset retains the native party source connection");
        }

        static auto PartyPresets(const char* classicPath = nullptr, const char* xivPath = nullptr) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true; runtime.active_module_ = &runtime.party_;
            runtime.active_config_section_ = party_module::general_section;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            auto& party = runtime.party_;
            party.state_.bounds.x = 550; party.state_.bounds.y = 400;
            party.split_statuses_[0].enabled = false;
            party.text_[0].style.font = 2; party.text_[0].offset_x = 9;
            const auto alliance = runtime.alliance_first_.State().bounds, alliance2 = runtime.alliance_second_.State().bounds;
            const auto slots = runtime.GroupOptionSlots();
            const auto first = std::find(slots.begin(), slots.end(), party_module::preset_options_start);
            if (!check(first != slots.end() && std::count_if(slots.begin(), slots.end(), [&](size_t index) {
                return index < party.OptionCount() && party.Option(index).card;
            }) == 2, "Party General exposes exactly two preset cards below its controls")) return false;
            const auto cardSlot = static_cast<int>(first - slots.begin());
            for (size_t i = 0; i < 2; ++i)
                if (!check(party.Option(party_module::preset_options_start + i).kind == module_option_kind::action &&
                    party.Option(party_module::preset_options_start + i).confirmation != nullptr &&
                    party.Option(party_module::preset_options_start + i).card,
                    "Classic and XIV presets use shared action buttons with a reset warning")) return false;
            for (const auto& section : party.ConfigSections())
                if (!check(std::strcmp(section.name, "Presets") != 0, "Party has no duplicate Presets page")) return false;
            const auto firstCard = runtime.ConfigLayout().OptionRow(cardSlot), secondCard = runtime.ConfigLayout().OptionRow(cardSlot + 1);
            if (!check(firstCard.width > 200 && firstCard.height > 90 && secondCard.y == firstCard.y && secondCard.x >= firstCard.x + firstCard.width,
                "Classic and XIV boxes share one row without overlapping")) return false;
            const auto button = runtime.ConfigLayout().OptionRow(cardSlot);
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            runtime.mouse_x_ = static_cast<int>(button.x + 4); runtime.mouse_y_ = static_cast<int>(button.y + 4);
            const auto click = [&] {
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            click();
            if (!check(runtime.profile_name_.Focused() && runtime.profile_dialog_ == hud_runtime::profile_dialog::confirm_action &&
                runtime.profile_keyboard_selection_ == 0 && party.text_[0].style.font == 2 && !party.split_statuses_[0].enabled,
                "opening a preset warning selects Cancel and leaves custom settings untouched")) return false;
            runtime.HandleProfileKey(VK_RETURN, 0);
            if (!check(!runtime.profile_name_.Focused() && party.text_[0].style.font == 2,
                "keyboard confirmation defaults to Cancel")) return false;
            click(); runtime.HandleProfileKey(VK_ESCAPE, 0);
            if (!check(!runtime.profile_name_.Focused() && party.text_[0].offset_x == 9, "Escape cancels without resetting the preset")) return false;
            click();
            if (!check(runtime.ApplyConfirmedAction() && !runtime.ApplyConfirmedAction(), "an explicitly accepted preset executes only once")) return false;
            const auto classic = party.LayoutResources({0, 0, party.LayoutContainer().canvas_width, 0}, {true, true, true, true, false});
            if (!check(!party.show_sync_ && party.job_icons_ && party.icon_follow_name_ && party.tp_ && party.tp_display_ == 1 && party.meters_[0].appearance == ui::meter_style::window_theme &&
                classic.bars[1].width < classic.bars[0].width &&
                classic.bars[1].x + classic.bars[1].width == classic.bars[0].x + classic.bars[0].width &&
                classic.bars[2].x < classic.bars[0].x && classic.bars[2].width == 28 && party.LayoutContainer().width < 240 &&
                party.text_[0].style.font == 0 && party.text_[0].offset_x == 44 && party.split_statuses_[0].enabled &&
                party.meters_[0].height == 10 && party.text_[3].offset_y == -6 && party.text_[5].offset_y == -6 &&
                party.icon_offset_x_ == 9 && party.icon_offset_y_ == 1 && party.text_[party_module::tp_text].placement == 1 &&
                party.state_.bounds.x == 550 && party.state_.bounds.y == 400,
                "Classic restores Cody's captured bars, text, job-icon adjustments and TP placement while retaining position")) return false;
            if (classicPath != nullptr)
            {
                std::ifstream input{classicPath, std::ios::binary};
                const std::string captured{std::istreambuf_iterator<char>{input}, {}};
                if (!check(!captured.empty(), "the saved Classic preset is available")) return false;
                party_module expected; expected.Load(settings_store::FromIniText(captured));
                expected.state_.bounds.x = 550; expected.state_.bounds.y = 400;
                auto expectedSettings = settings_store::FromIniText(""); expected.Save(expectedSettings);
                auto actual = settings_store::FromIniText(""); party.Save(actual);
                if (!check(actual.ToIniText() == expectedSettings.ToIniText(),
                    "embedded Classic restores every captured Party preference through normal persistence")) return false;
                party_module allianceClassic{1}; allianceClassic.state_.bounds.x = 210; allianceClassic.state_.bounds.y = 310;
                allianceClassic.ApplyPreset(0);
                if (!check(allianceClassic.group_ == 1 && allianceClassic.state_.bounds.x == 210 && allianceClassic.state_.bounds.y == 310,
                    "Classic retains the selected alliance identity and position")) return false;
                allianceClassic.group_ = 0;
                allianceClassic.state_.bounds.x = 550; allianceClassic.state_.bounds.y = 400;
                auto allianceSettings = settings_store::FromIniText(""); allianceClassic.Save(allianceSettings);
                if (!check(allianceSettings.ToIniText() == expectedSettings.ToIniText(),
                    "the same complete Classic preferences apply to an alliance list")) return false;
            }
            const auto originalIconSize = party.icon_size_;
            runtime.controller_config_.region = config_controller_region::options;
            const auto items = runtime.ControllerConfigItems(config_controller_region::options);
            const auto xiv = std::find_if(items.begin(), items.end(), [](const auto& item) { return item.index == party_module::preset_options_start + 1; });
            if (!check(xiv != items.end(), "controller focus exposes the XIV card below the ordinary controls")) return false;
            runtime.config_bounds_.height = 330;
            runtime.controller_config_.id = xiv->id;
            runtime.RefreshControllerConfigFocus();
            const auto revealed = runtime.ControllerConfigItems(config_controller_region::options);
            const auto shown = std::find_if(revealed.begin(), revealed.end(), [](const auto& item) { return item.index == party_module::preset_options_start + 1; });
            if (!check(shown != revealed.end() && shown->bounds.y + shown->bounds.height <= runtime.config_bounds_.y + runtime.config_bounds_.height - 24,
                "controller focus scrolls the full preset card into a short config window")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(runtime.profile_name_.Focused() && party.icon_size_ == originalIconSize,
                "held controller Confirm cannot accept the warning")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            if (!check(!runtime.profile_name_.Focused() && party.icon_size_ == originalIconSize,
                "controller confirmation defaults to Cancel")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::right, false);
            runtime.HandleControllerConfig(controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(!runtime.profile_name_.Focused() && party.state_.bounds.x == 550 && party.state_.bounds.y == 400 &&
                runtime.alliance_first_.icon_size_ == party.icon_size_ && runtime.alliance_second_.icon_size_ == party.icon_size_ &&
                runtime.alliance_first_.state_.bounds.x == alliance.x && runtime.alliance_first_.state_.bounds.y == alliance.y &&
                runtime.alliance_second_.state_.bounds.x == alliance2.x && runtime.alliance_second_.state_.bounds.y == alliance2.y,
                "explicit controller Apply restores captured XIV preferences while preserving every list position")) return false;
            if (xivPath != nullptr)
            {
                std::ifstream input{xivPath,std::ios::binary};
                const std::string captured{std::istreambuf_iterator<char>{input},{}};
                if (!check(!captured.empty(),"the captured XIV preset is available")) return false;
                party_module expected; expected.Load(settings_store::FromIniText(captured));
                expected.state_.bounds.x=550; expected.state_.bounds.y=400;
                auto expectedSettings=settings_store::FromIniText(""); expected.Save(expectedSettings);
                for (const auto* list : {&party,&runtime.alliance_first_,&runtime.alliance_second_})
                {
                    auto normalized=*list;
                    normalized.group_=0; normalized.state_.bounds.x=550; normalized.state_.bounds.y=400;
                    auto actual=settings_store::FromIniText(""); normalized.Save(actual);
                    if (!check(actual.ToIniText()==expectedSettings.ToIniText(),
                        "XIV restores every captured preference on Party and both alliances")) return false;
                }
            }
            runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=party\nsection=12\n"));
            if (!check(runtime.active_module_ == &party && runtime.active_config_section_ == party_module::general_section,
                "old Party Presets navigation migrates to Party General")) return false;
            runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=general\nsection=6\nparty_presets_general=1\n"));
            if (!check(runtime.active_module_ == &party && runtime.active_config_section_ == party_module::general_section,
                "the previous General preset page migrates to Party General")) return false;
            runtime.LoadConfigSelection(settings_store::FromIniText("[session.config]\nmodule=party\nsection=13\n"));
            if (!check(runtime.active_module_ == &party && runtime.active_config_section_ == party_module::window_section,
                "the old Window page keeps its navigation destination")) return false;
            party_module restored; restored.Load(runtime.CaptureSettings());
            return check(restored.layout_ == party.layout_ && restored.meters_[0].appearance == party.meters_[0].appearance &&
                restored.state_.bounds.x == 550 && restored.tp_ == party.tp_, "preset results persist through ordinary module settings");
        }

        static auto ThemedButtons(void) -> bool
        {
            ui::design_system design;
            design.SetControlPalette(0xFFC0C0FE, 0xFFFEC080);
            const auto native = design.GetTheme().colors.control;
            design.SetWindowPalette(0xFF203050);
            const auto blue = design.GetTheme().colors.control;
            design.SetWindowPalette(0xFF605030);
            if (!check(native != blue && blue != design.GetTheme().colors.control && design.GetTheme().colors.control_selected == 0xFFFEC080,
                "shared button faces follow the window palette and retain the native orange hover")) return false;
            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-button-palette-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            { std::ofstream theme{path}; theme << "[colors]\ncontrol=FF123456\ncontrol_selected=FFABCDEF\n"; }
            design.SetThemePath(path.string().c_str());
            design.SetWindowPalette(0xFF304030);
            return check(design.GetTheme().colors.control == 0xFF123456 && design.GetTheme().colors.control_selected == 0xFFABCDEF,
                "explicit button color overrides survive Window Type changes");
        }

        static auto GlobalText(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true; runtime.active_module_ = &runtime.general_;
            runtime.active_config_section_ = general_module::appearance_section;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto tab = ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, std::size(general_module::appearance_tabs));
            runtime.mouse_x_ = static_cast<int>(tab.x + 4); runtime.mouse_y_ = static_cast<int>(tab.y + 4);
            if (!check(ControlCount(runtime) == 2 && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                runtime.ConfigSoundTarget(viewport) != 0 && runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) &&
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && runtime.general_.AppearanceTab() == 1 &&
                runtime.GroupOptionIndex(0) == runtime.general_.GlobalTextStart() && ControlCount(runtime) == ui::global_text_settings::option_count &&
                runtime.ConfigLayout().ContentSubtabs().y + 48 <= runtime.ConfigLayout().OptionRow(0).y,
                "Global Text exposes all controls with shared input geometry and a separate preview area")) return false;
            auto& name = runtime.player_.Settings().name_text;
            name.style.top_color = 0xAABBCCDD; name.style.bottom_color = 0x88997766;
            name.style.shadow.enabled = name.style.stroke.enabled = name.style.glow.enabled = true;
            name.offset_x = 13; name.offset_y = -7;
            const auto sizeIndex = runtime.general_.GlobalTextStart();
            *runtime.general_.Option(sizeIndex).slider = 1.4f;
            runtime.general_.OnOptionChanged(sizeIndex);
            size_t styles{};
            bool sizes = true;
            for (auto* module : runtime.modules_.All())
                module->VisitTextSettings([&](ui::text_style& style) { ++styles; sizes &= style.scale == 1.4f; });
            if (!check(sizes && styles > 70, "global size reaches hidden modules, both chat windows, alliances, timers and recipient text")) return false;
            name.style.scale = 0.8f;
            const auto family = runtime.general_.Option(sizeIndex + 1);
            if (!check(family.kind == module_option_kind::dropdown && family.choice_count == 4 && std::strcmp(family.choices[3], "Grammara") == 0,
                "global fonts include the original, two Windows alternatives and Grammara")) return false;
            *family.choice = 3; runtime.general_.OnOptionChanged(sizeIndex + 1);
            bool fonts = true;
            for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { fonts &= style.font == 3; });
            if (!check(fonts && name.style.scale == 0.8f && name.style.top_color == 0xAABBCCDD && name.style.bottom_color == 0x88997766 &&
                name.style.shadow.enabled && name.style.stroke.enabled && name.style.glow.enabled && name.offset_x == 13 && name.offset_y == -7,
                "font changes preserve module size overrides, colors, opacity, text effects and offsets")) return false;
            name.style.font = 2;
            const auto global = [&](const text_property property) { return runtime.general_.Option(sizeIndex + static_cast<size_t>(property)); };
            *global(text_property::letter_spacing).slider = 1.25f;
            *global(text_property::shadow).toggle = false;
            *global(text_property::shadow_color).color = 0x80403020;
            *global(text_property::shadow_x).slider = -3; *global(text_property::shadow_y).slider = 4;
            *global(text_property::soft_shadow).toggle = true;
            *global(text_property::outline).toggle = false; *global(text_property::outline_color).color = 0xA0102030;
            *global(text_property::glow).toggle = true; *global(text_property::glow_color).color = 0x40506070;
            *global(text_property::sheen).toggle = true; *global(text_property::sheen_color).color = 0x708090A0;
            *global(text_property::sheen_width).slider = 28; *global(text_property::sheen_speed).slider = 0.5f;
            for (size_t i = 2; i < ui::global_text_settings::option_count; ++i) runtime.general_.OnOptionChanged(sizeIndex + i);
            bool effects = true;
            for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) {
                effects &= style.letter_spacing == 1.25f && !style.shadow.enabled && style.shadow.color == 0x80403020 &&
                    style.shadow.offset_x == -3 && style.shadow.offset_y == 4 && style.shadow.soft && !style.stroke.enabled &&
                    style.stroke.color == 0xA0102030 && style.glow.enabled && style.glow.color == 0x40506070 &&
                    style.sheen.enabled && style.sheen.color == 0x708090A0 && style.sheen.width == 28 && style.sheen.speed == 0.5f;
            });
            if (!check(effects && name.style.scale == 0.8f && name.style.font == 2 && name.style.top_color == 0xAABBCCDD &&
                name.style.bottom_color == 0x88997766 && name.offset_x == 13 && name.offset_y == -7 &&
                runtime.general_.ConfigText().letter_spacing == 0 && !runtime.general_.ConfigText().sheen.enabled,
                "global spacing and every effect reach all HUD text without changing config text, module colors, size, font or positions")) return false;
            name.style.letter_spacing = 0.25f; name.style.shadow.offset_x = 6; name.style.sheen.enabled = false;
            *global(text_property::glow_color).color = 0x8090A0B0;
            runtime.general_.OnOptionChanged(sizeIndex + static_cast<size_t>(text_property::glow_color));
            if (!check(name.style.letter_spacing == 0.25f && name.style.shadow.offset_x == 6 && !name.style.sheen.enabled,
                "a later global property leaves unrelated per-module effect overrides intact")) return false;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored; restored.LoadSettings(saved);
            std::vector<std::pair<float, int>> before, after;
            std::vector<std::string> owners;
            for (auto* module : runtime.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { before.emplace_back(style.scale, style.font); owners.emplace_back(module->Id()); });
            for (auto* module : restored.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) { after.emplace_back(style.scale, style.font); });
            for (size_t index = 0; index < std::min(before.size(), after.size()); ++index)
                if (before[index] != after[index]) std::fprintf(stderr, "%s text %zu: expected %.2f/font %d, got %.2f/font %d\n",
                    owners[index].c_str(), index, before[index].first, before[index].second, after[index].first, after[index].second);
            if (!check(before == after && restored.general_.GlobalTextSize() == 1.4f && restored.general_.GlobalFont() == 3,
                "global defaults and later per-module overrides persist through complete profile reloads")) return false;
            const auto textStyles = [](hud_runtime& hud) {
                auto store = settings_store::FromIniText(""); size_t index{};
                for (auto* module : hud.modules_.All()) module->VisitTextSettings([&](ui::text_style& style) {
                    SaveTextStyle(store, std::to_string(index++).c_str(), style);
                });
                return store.ToIniText();
            };
            if (!check(textStyles(runtime) == textStyles(restored),
                "global effects and individual overrides round trip for every module, including notifications")) return false;
            const auto fresh = LoadTextProfile(saved, "text.new.module", {});
            if (!check(fresh.style.scale == 1.4f && fresh.style.font == 3 && fresh.style.letter_spacing == 1.25f &&
                fresh.style.shadow.offset_x == -3 && fresh.style.shadow.offset_y == 4 && fresh.style.shadow.soft && !fresh.style.stroke.enabled &&
                fresh.style.glow.color == 0x8090A0B0 && fresh.style.sheen.enabled && fresh.style.sheen.width == 28 && fresh.style.sheen.speed == 0.5f,
                "new text profiles inherit each applied global property while saved overrides stay authoritative")) return false;
            const auto legacy = LoadTextProfile(settings_store::FromIniText("[global.text]\nfont=2\nscale=1.8\n"), "text.old.module", name);
            if (!check(legacy.style.font == 2 && legacy.style.scale == name.style.scale && legacy.style.shadow.offset_x == 6 &&
                !legacy.style.sheen.enabled, "older global settings preserve existing effects until their controls are used")) return false;
            ui::embedded_font embedded;
            if (!check(embedded.Load(), "Grammara loads directly from the embedded font resource")) return false;
            HDC dc = ::CreateCompatibleDC(nullptr);
            HFONT font = ::CreateFontA(-15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Grammara");
            if (!check(dc != nullptr && font != nullptr, "the supplied font creates a native font face"))
            { if (dc != nullptr) ::DeleteDC(dc); if (font != nullptr) ::DeleteObject(font); return false; }
            const auto previous = ::SelectObject(dc, font);
            char actual[LF_FACESIZE]{}; ::GetTextFaceA(dc, LF_FACESIZE, actual);
            ::SelectObject(dc, previous); ::DeleteObject(font); ::DeleteDC(dc);
            return check(::_stricmp(actual, "Grammara") == 0, "Grammara resolves to its own face rather than a Windows fallback");
        }

        static auto Attribution(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.option_scroll_ = 6;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto icon = runtime.ConfigLayout().HeaderAction(), close = runtime.ConfigLayout().CloseButton();
            const auto drag = runtime.ConfigLayout().HeaderDragArea();
            if (!check(icon.x + icon.width < close.x && drag.x + drag.width <= icon.x,
                "attribution sits beside close and outside the config drag area")) return false;
            runtime.mouse_x_ = static_cast<int>(icon.x + icon.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(icon.y + icon.height * 0.5f);
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand && runtime.ConfigSoundTarget(viewport) == 3,
                "attribution shares its draw, cursor and hover-sound target")) return false;
            if (!check(runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) && runtime.active_module_ == &runtime.general_ &&
                runtime.active_config_section_ == general_module::attribution_section && runtime.option_scroll_ == 0 &&
                runtime.interaction_ == hud_runtime::interaction::none && runtime.IsEditing(), "header icon opens attribution without closing or dragging config")) return false;
            if (!check(runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport) && runtime.IsEditing(), "attribution press and release remain captured")) return false;
            bool cadence{}, momentum{};
            if (!check(ControlCount(runtime) == ui::attributions.size(), "attribution contains the complete credits list")) return false;
            for (size_t row = 0; row < runtime.GroupOptionCount(); ++row)
            {
                const auto index = runtime.GroupOptionIndex(row);
                const auto option = runtime.general_.Option(index);
                if (!check(option.kind == module_option_kind::action && std::strcmp(option.action_text, "Copy link") == 0 &&
                    !runtime.general_.ConfigSectionContains(general_module::enabled_modules_section, index),
                    "credit source links are separate from module switches")) return false;
                if (std::strcmp(option.label, "Cadence") == 0) cadence = std::strstr(option.description, "Kipling") != nullptr;
                if (std::strcmp(option.label, "Momentum") == 0) momentum = std::strstr(option.description, "Kipling") != nullptr;
                if (!check(std::strcmp(option.label, "XIVParty") != 0 && std::strcmp(option.label, "XIUI") != 0,
                    "XIVParty and XIUI are absent from project credits")) return false;
            }
            if (!check(cadence && momentum, "Kipling is credited for both Cadence and Momentum")) return false;
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                const bool down = runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                const bool up = runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
                return down && up;
            };
            const auto section = ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 1, std::size(ui::attribution_views));
            runtime.mouse_x_ = static_cast<int>(section.x + 4); runtime.mouse_y_ = static_cast<int>(section.y + 4);
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand && runtime.ConfigSoundTarget(viewport) != 0,
                "contributor section uses shared segmented-control cursor and sound targets")) return false;
            if (!check(click(section) && runtime.general_.AttributionView() == 1 && runtime.option_scroll_ == 0 &&
                ControlCount(runtime) == ui::xiui_contributors.size(), "Original XIUI shows every contributor in one view")) return false;
            static std::string openedProfile;
            runtime.general_.open_link_ = [](HWND, const char* url) -> bool { openedProfile = url; return true; };
            size_t linked{}, unlinked{}, total{};
            size_t counts[3]{};
            const char* names[]{"Tirem", "Shuu", "JQuick", "Nerf"};
            const char* urls[]{"https://github.com/tirem", "https://github.com/Shuu-37", "https://github.com/jquick", "https://github.com/NerfOnline"};
            for (size_t group = 0; group < std::size(ui::contributor_groups); ++group)
            {
                const ui::contributor_list_layout list{runtime.ConfigLayout()};
                const auto header = list.Header(group);
                runtime.mouse_x_ = static_cast<int>(header.x + 4); runtime.mouse_y_ = static_cast<int>(header.y + 4);
                openedProfile.clear();
                if (!check(list.Visible(header) && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow &&
                    runtime.ConfigSoundTarget(viewport) == 0 && click(header) && openedProfile.empty(),
                    "Founders, Core Contributors and Contributors are visible noninteractive list headings")) return false;
            }
            for (size_t row = 0; row < runtime.GroupOptionCount(); ++row)
            {
                const auto index = runtime.GroupOptionIndex(row);
                const auto& contributor = ui::xiui_contributors[index - runtime.general_.ContributorsStart()];
                const auto option = runtime.general_.Option(index);
                if (!check(option.kind == module_option_kind::action && option.action_icon == ui::icon_glyph::github &&
                    std::strcmp(option.label, contributor.name) == 0 && option.available == (contributor.url != nullptr) &&
                    std::strcmp(option.label, "onimitch") != 0 && std::strcmp(option.label, "tukhubvm") != 0 &&
                    !runtime.general_.ConfigSectionContains(general_module::grid_section, index), "curated contributors retain GitHub actions and removed names stay absent")) return false;
                if (row < 4 && !check(std::strcmp(option.label, names[row]) == 0 && std::strcmp(contributor.url, urls[row]) == 0,
                    "founder and core display names retain the correct GitHub profiles")) return false;
                ++total; ++counts[static_cast<size_t>(contributor.group)];
                const ui::contributor_list_layout list{runtime.ConfigLayout()};
                const auto member = list.Member(row), button = ui::config_layout::ChoiceButton(member);
                if (!check(list.Visible(member) && member.y >= list.Header(static_cast<size_t>(contributor.group)).y + list.Header(0).height,
                    "each contributor appears beneath its category header")) return false;
                runtime.mouse_x_ = static_cast<int>(button.x + 4); runtime.mouse_y_ = static_cast<int>(button.y + 4);
                if (!check((runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand) == (contributor.url != nullptr) &&
                    (runtime.ConfigSoundTarget(viewport) != 0) == (contributor.url != nullptr), "profile buttons share drawing, cursor and sound geometry")) return false;
                openedProfile.clear();
                if (!check(click(button), "GitHub action press and release stay in config")) return false;
                if (contributor.url != nullptr)
                {
                    ++linked;
                    if (!check(openedProfile == contributor.url, "GitHub buttons open each contributor's own profile through the link handler")) return false;
                }
                else
                {
                    ++unlinked;
                    if (!check(openedProfile.empty(), "historical names without linked accounts never open guessed profiles")) return false;
                }
            }
            if (!check(total == ui::xiui_contributors.size() && linked == 11 && unlinked == 1 && counts[0] == 2 && counts[1] == 2 && counts[2] == 8,
                "all curated contributors appear once under the correct group header")) return false;
            runtime.config_bounds_.height = 430;
            const ui::contributor_list_layout compact{runtime.ConfigLayout()};
            if (!check(!compact.Visible(compact.Member(ui::xiui_contributors.size() - 1)), "compact windows keep overflow rows outside the visible list")) return false;
            runtime.option_scroll_ = 1000; runtime.ClampOptionScroll();
            const ui::contributor_list_layout scrolled{runtime.ConfigLayout(), runtime.option_scroll_};
            if (!check(runtime.option_scroll_ == compact.MaximumScroll() && scrolled.Visible(scrolled.Member(ui::xiui_contributors.size() - 1)),
                "compact contributor lists scroll all the way to the final profile")) return false;
            openedProfile.clear();
            if (!check(click(ui::config_layout::ChoiceButton(scrolled.Member(ui::xiui_contributors.size() - 1))) &&
                openedProfile == ui::xiui_contributors.back().url, "scrolled profile buttons retain matching click targets")) return false;
            if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 0, std::size(ui::attribution_views))) &&
                ControlCount(runtime) == ui::attributions.size(), "project credits remain reachable after browsing contributors")) return false;
            if (!check(click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), 2, std::size(ui::attribution_views))) &&
                runtime.general_.AttributionView() == 2 && ControlCount(runtime) == ui::ashita_credits.size(), "Ashita has a separate attribution tab")) return false;
            const auto atomCredit = runtime.general_.Option(runtime.general_.AshitaStart() + 1);
            if (!check(std::strcmp(atomCredit.label, "atom0s") == 0 && std::strstr(atomCredit.description, "Thank you") != nullptr,
                "the Ashita tab includes a short thank-you to atom0s")) return false;
            runtime.controller_config_.active = true;
            runtime.controller_config_.region = config_controller_region::options;
            const auto ashitaItems = runtime.ControllerConfigItems(config_controller_region::options);
            if (!check(ashitaItems.size() == ui::ashita_credits.size(), "Ashita credit links are reachable through shared controller controls")) return false;
            openedProfile.clear();
            runtime.general_.OnOptionChanged(runtime.general_.AshitaStart() + 1);
            if (!check(openedProfile == "https://github.com/atom0s", "the atom0s credit opens the correct profile")) return false;
            return true;
        }

        static auto TargetOfTargetSettings(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.target_;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + 3); runtime.mouse_y_ = static_cast<int>(bounds.y + 3);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto topics = runtime.target_.ConfigSections();
            const auto page = runtime.ActiveConfigPage();
            if (!check(topics.size() == BarConfigSections().size() + 2 && page.SectionCount() == 3 && runtime.HasConfigSections() &&
                std::strcmp(page.SectionName(0), "Target") == 0 && std::strcmp(page.SectionName(1), "Target of Target") == 0 &&
                std::strcmp(page.SectionName(2), "Subtarget") == 0,
                "Target, Target of Target and Subtarget are sibling top categories")) return false;
            const auto second = runtime.ConfigLayout().Section(1, 3), menu = runtime.ConfigLayout().Submenu(0, topics.size());
            if (!check(second.y + second.height < menu.y && runtime.ConfigLayout().Section(0, 3).x == menu.x,
                "the category row sits above both the secondary menu and content")) return false;
            click(second);
            if (!check(runtime.target_.ConfigView() == 1 && runtime.target_.ConfigSections().size() == BarConfigSections().size() &&
                page.SectionContains(1, &runtime.target_) && !page.SectionContains(0, &runtime.target_),
                "the recipient category selects its own settings topics and highlight")) return false;
            auto& settings = runtime.target_.Settings().recipient;
            const ui::rect main{20, 100, 440, 82};
            const auto classic = settings.Area(main, 90, 20), arrow = settings.Arrow(main, 20);
            if (!check(settings.placement == target_of_target_settings::inline_right && settings.arrow && !settings.distance &&
                !settings.cast && !settings.statuses && classic.x > main.x + main.width &&
                classic.width - 14 == (main.width - 14) / 3 && classic.y + 31 + settings.BarHeight(20) / 2 == main.y + 41 &&
                arrow.y + arrow.height / 2 == main.y + 41 && classic.x + 7 - (arrow.x + arrow.width) == settings.arrow_gap,
                "classic defaults use a one-third-width inline bar and an arrow centered on the main HP bar")) return false;
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(!settings.enabled && runtime.target_.State().enabled, "recipient visibility is independent of the main target")) return false;
            const auto recipientTopics = settings.ConfigSections();
            click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::animation), recipientTopics.size()));
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(!settings.follow_meter && settings.Option(target_of_target_settings::base_options + 13).visible &&
                settings.Option(target_of_target_settings::base_options + 16).visible,
                "Animation exposes the follow-style switch and unlocks independent sheen and smoothing")) return false;
            click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::effects), recipientTopics.size()));
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(settings.follow_meter && !settings.Option(target_of_target_settings::base_options + 6).visible &&
                !settings.Option(target_of_target_settings::base_options + 11).visible,
                "Effects exposes the same follow-style switch and restores inherited shadows and glow")) return false;
            for (size_t tab = 0; tab < recipientTopics.size(); ++tab)
            {
                click(runtime.ConfigLayout().Submenu(tab, recipientTopics.size()));
                if (!check(runtime.active_config_section_ == tab && runtime.GroupOptionCount() > 0 &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                    (runtime.ConfigSoundTarget(viewport) & 0xFFFFF) == 0x300 + tab,
                    "recipient topics share matching drawing, click, hover and sound geometry")) return false;
            }
            for (size_t index = 0; index < target_of_target_settings::option_count; ++index)
            {
                size_t owners{};
                for (size_t tab = 0; tab < recipientTopics.size(); ++tab)
                    owners += settings.Contains(index, tab);
                if (!check(owners == (index == 21 ? size_t{3} : size_t{1}),
                    "recipient controls have one topic except the shared style switch in Bar Style, Animation and Effects")) return false;
            }
            click(runtime.ConfigLayout().Section(0, 3));
            if (!check(runtime.target_.ConfigView() == 0 && runtime.active_config_section_ == static_cast<size_t>(bar_config_section::text) &&
                runtime.ElementSelector().count == 6, "switching to Target preserves the Text topic and restores its own profiles")) return false;
            click(runtime.ConfigLayout().Section(1, 3));
            if (!check(runtime.target_.ConfigView() == 1 && runtime.active_config_section_ == static_cast<size_t>(bar_config_section::text) && runtime.ElementSelector().count == 4,
                "switching back restores recipient Text without leaking the main target options")) return false;
            const std::array<float, 4> textSizes{1.25f, 0.75f, 1.0f, 0.5f};
            for (size_t i = 0; i < textSizes.size(); ++i)
            {
                settings.selected_text = static_cast<int>(i);
                auto size = settings.Option(target_of_target_settings::base_options + meter_style_option_count + 1);
                if (!check(size.available && size.visible && size.slider == &settings.text[i].style.scale && size.display_scale == ui::text_base_pixels,
                    "each recipient text selection edits its own native pixel size")) return false;
                *size.slider = textSizes[i];
            }
            for (size_t i = 0; i < textSizes.size(); ++i)
                if (!check(settings.text[i].style.scale == textSizes[i], "editing another recipient text size leaves the others unchanged")) return false;
            size_t targetSwitches{};
            for (size_t i = general_module::enabled_modules_option; i < runtime.general_.AttributionStart(); ++i)
                targetSwitches += runtime.general_.Option(i).toggle == &runtime.target_.State().enabled;
            if (!check(targetSwitches == 1, "main Target retains one entry in Enabled Modules")) return false;
            settings.placement = target_of_target_settings::inline_left;
            const auto left = settings.Area(main, 90, 20), leftArrow = settings.Arrow(main, 20);
            if (!check(settings.ShowArrow() && left.x < main.x &&
                leftArrow.x - (left.x + left.width - 7) == settings.arrow_gap &&
                main.x + 7 - (leftArrow.x + leftArrow.width) == settings.gap &&
                leftArrow.y + leftArrow.height / 2 == main.y + 41,
                "inline left keeps the arrow between the bars and centered on main HP")) return false;
            for (const int placement : {target_of_target_settings::above, target_of_target_settings::below})
            {
                settings.placement = placement;
                settings.arrow = true;
                const auto withArrow = settings.Area(main, 90, 20), hiddenArrow = settings.Arrow(main, 20);
                if (!check(!settings.ShowArrow() && hiddenArrow.width == 0 && hiddenArrow.height == 0 &&
                    !settings.Option(14).visible && !settings.Option(15).visible && !settings.Option(27).visible,
                    "above and below hide the arrow and inline-only arrow controls")) return false;
                settings.arrow = false;
                const auto withoutArrow = settings.Area(main, 90, 20);
                if (!check(withArrow.x == withoutArrow.x && withArrow.y == withoutArrow.y,
                    "stacked layouts reserve no arrow space regardless of the saved toggle")) return false;
            }
            settings.placement = target_of_target_settings::below;
            settings.arrow = false; settings.health = settings.percent = settings.cast = settings.statuses = false;
            settings.width = 238; settings.bar_height = 12; settings.gap = 4; settings.offset_x = 17; settings.offset_y = -70;
            settings.follow_meter = settings.follow_fill = settings.follow_text = false;
            settings.fill_top = 0xFFAABBCC; settings.arrow_color = 0x80887766;
            settings.text[0].style.glow.enabled = true; settings.text[0].style.bottom_color = 0xFF335577;
            const auto area = settings.Area(main, 50);
            if (!check(area.x == 37 && area.y == 84 && area.width == 238, "stacked sizing and signed offsets apply after spacing")) return false;
            auto saved = settings_store::FromIniText(""); runtime.target_.Save(saved);
            target_module restored; restored.Load(saved);
            const auto& loaded = restored.Settings().recipient;
            if (!check(!loaded.enabled && !loaded.health && !loaded.percent && !loaded.cast && !loaded.statuses &&
                loaded.width == 238 && loaded.bar_height == 12 && loaded.gap == 4 && loaded.offset_x == 17 && loaded.offset_y == -70 &&
                loaded.placement == target_of_target_settings::below && !loaded.arrow && !loaded.follow_meter && !loaded.follow_fill &&
                loaded.fill_top == 0xFFAABBCC && loaded.arrow_color == 0x80887766 && loaded.text[0].style.glow.enabled &&
                loaded.text[0].style.bottom_color == 0xFF335577,
                "recipient layout, arrow, independent bar colors and styled text persist through profiles")) return false;
            for (size_t i = 0; i < textSizes.size(); ++i)
                if (!check(loaded.text[i].style.scale == textSizes[i], "all four independent recipient text sizes persist")) return false;
            auto scaled = settings_store::FromIniText("[target.target_of_target]\ntext_scale=1.5\nfollow_text=1\n"
                "[text.target.name]\nscale=1.25\n[text.target.distance]\nscale=0.75\n[text.target.labels]\nscale=1\n[text.target.values]\nscale=0.5\n"
                "[text.target.recipient.name]\nscale=0.5\n");
            restored.Load(scaled);
            if (!check(!loaded.follow_text, "a legacy shared multiplier becomes independent text sizes")) return false;
            for (size_t i = 0; i < textSizes.size(); ++i)
                if (!check(loaded.text[i].style.scale == textSizes[i] * 1.5f, "legacy scaling preserves the previously displayed inherited sizes")) return false;
            restored.Save(scaled); restored.Load(scaled);
            for (size_t i = 0; i < textSizes.size(); ++i)
                if (!check(loaded.text[i].style.scale == textSizes[i] * 1.5f, "saved recipient sizes are not scaled a second time")) return false;
            auto legacy = settings_store::FromIniText("[target.target_of_target]\nwidth=0\nbar_height=0\ngap=6\npercent=1\nlabel=1\ndistance=1\ncast=1\nstatuses=1\nattack=1\n");
            restored.Load(legacy);
            if (!check(restored.Settings().recipient.placement == target_of_target_settings::inline_right && !restored.Settings().recipient.distance &&
                restored.Settings().recipient.gap == 8, "untouched earlier defaults migrate to the classic inline presentation")) return false;
            legacy.WriteFloat("target.target_of_target", "offset_x", 27);
            restored.Load(legacy);
            if (!check(restored.Settings().recipient.placement == target_of_target_settings::below && restored.Settings().recipient.offset_x == 27,
                "customized earlier placement survives the new default")) return false;
            return true;
        }

        static auto TargetChildren(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.LoadSettings(settings_store::FromIniText("[layout.grid]\nsnap_grid=0\nsnap_elements=0\n"));
            runtime.editing_ = true;
            if (!check(!runtime.subtarget_.HasEditableBounds() && !runtime.target_recipient_.HasEditableBounds(),
                "missing child settings retain attached layouts without separate movement hit areas")) return false;
            auto& recipient = runtime.target_.Settings().recipient;
            recipient.placement = target_of_target_settings::detached;
            recipient.offset_x = 17; recipient.gap = 11;
            *runtime.subtarget_.Option(13).choice = 1;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto originalMain = runtime.target_.State().bounds;
            for (auto* child : {&runtime.target_recipient_, &runtime.subtarget_})
            {
                child->State().bounds = {420, 240, 220, 80};
                const auto movable = runtime.ControllerMovableModules();
                if (!check(child->PreviewVisible() && std::find(movable.begin(), movable.end(), child) != movable.end(),
                    "detached children join the shared controller movement list")) return false;
                runtime.active_module_ = child;
                const auto page = runtime.ActiveConfigPage();
                const size_t view = child == &runtime.subtarget_ ? 2 : 1;
                if (!check(page.Contains(child) && page.SectionContains(view, child),
                    "dragged child retains its Target category and matching configuration tab")) return false;
                const auto original = child->State().bounds;
                auto& focus = runtime.controller_config_;
                focus.moving = true; focus.moving_id = child->Id(); focus.move_original = original;
                focus.move_desired = original; focus.move_collapsed = false; focus.viewport = viewport;
                runtime.HandleControllerMove(controller_action::right, false);
                const auto pending = runtime.CaptureSettings();
                const auto section = std::string{"layout."} + child->Id();
                if (!check(child->State().bounds.x > original.x && pending.ReadRect(section.c_str(), {}).x == original.x &&
                    runtime.target_.State().bounds.x == originalMain.x && runtime.target_.State().bounds.y == originalMain.y,
                    "child movement leaves Target fixed and automatic saving retains the committed child position")) return false;
                runtime.HandleControllerMove(controller_action::cancel, false);
                if (!check(!focus.moving && child->State().bounds.x == original.x && child->State().bounds.y == original.y,
                    "controller Cancel restores each detached child's position")) return false;
                runtime.CenterModuleLayout(*child, 0, viewport);
                if (!check(runtime.target_.State().bounds.x == originalMain.x, "centering a child leaves Target fixed")) return false;
                child->SetDisplayAllowed(false);
                if (!check(!child->PreviewVisible() && !child->ShouldRender({}), "context-hidden children expose no preview or live surface")) return false;
                child->SetDisplayAllowed(true);
            }
            auto saved = runtime.CaptureSettings();
            hud_runtime restored; restored.LoadSettings(saved);
            if (!check(restored.target_.Settings().recipient.placement == target_of_target_settings::detached &&
                *restored.subtarget_.Option(13).choice == 1 &&
                restored.target_recipient_.State().bounds.x == runtime.target_recipient_.State().bounds.x &&
                restored.subtarget_.State().bounds.x == runtime.subtarget_.State().bounds.x,
                "both child modes and independent positions persist through ordinary profiles")) return false;
            recipient.placement = target_of_target_settings::inline_right;
            *runtime.subtarget_.Option(13).choice = 0;
            if (!check(recipient.offset_x == 17 && recipient.gap == 11 && !runtime.target_recipient_.HasEditableBounds() &&
                !runtime.subtarget_.HasEditableBounds(), "reattaching restores linked preferences and removes detached hit areas")) return false;
            runtime.editing_ = false;
            std::puts("Target child configuration, independent movement, committed saves, cancellation, visibility and profile persistence passed.");
            return true;
        }
        static auto PartyModuleVisibility(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            size_t masters{}, children{};
            for (size_t i = general_module::enabled_modules_option; i < runtime.general_.AttributionStart(); ++i)
            {
                const auto option = runtime.general_.Option(i);
                masters += option.toggle == runtime.general_.PartyEnabledFlag();
                children += option.toggle == &runtime.alliance_first_.State().enabled || option.toggle == &runtime.alliance_second_.State().enabled;
            }
            if (!check(masters == 1 && children == 0, "Enabled Modules has one Party master and no separate alliance switches")) return false;
            runtime.party_source_.player_id_ = 1;
            for (size_t i : {size_t{0}, size_t{6}, size_t{12}}) runtime.party_source_.members_[i].active = true;
            for (unsigned mask = 0; mask < 4; ++mask)
            {
                runtime.alliance_first_.State().enabled = (mask & 1) != 0;
                runtime.alliance_second_.State().enabled = (mask & 2) != 0;
                runtime.active_module_ = &runtime.party_;
                const auto page = runtime.ActiveConfigPage();
                if (!check(page.SectionCount() == 3 && runtime.HasConfigSections(), "all Party settings selectors remain available for restoring disabled lists")) return false;
                for (size_t index = 1; index < 3; ++index)
                {
                    auto* module = page.SectionModule(index);
                    click(runtime.ConfigLayout().Section(index, 3));
                    if (!check(runtime.active_module_ == module && module->PreviewVisible() == module->State().enabled &&
                        module->ShouldRender({}) == module->State().enabled, "alliance visibility controls both live rendering and the edit preview")) return false;
                }
            }
            auto disabled = settings_store::FromIniText("[modules]\nparty=0\n");
            runtime.general_.Load(disabled);
            const auto page = runtime.ActiveConfigPage();
            for (auto* module : {&runtime.party_, &runtime.alliance_first_, &runtime.alliance_second_})
                if (!check(!module->PreviewVisible() && !module->ShouldRender({}) && module->State().enabled,
                    "Party master hides all lists without changing their individual enable choices")) return false;
            runtime.alliance_first_.ResetDefaults(ui::theme::CreateDefault());
            runtime.alliance_second_.CopyPreferences(runtime.party_);
            if (!check(!runtime.alliance_first_.PreviewVisible() && !runtime.alliance_second_.PreviewVisible() && !runtime.ConfigPageVisible(page),
                "list resets and preference copies retain the disabled master gate")) return false;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored;
            for (auto* module : restored.modules_.All()) module->Load(saved);
            if (!check(!restored.general_.PartyEnabled() && !restored.party_.PreviewVisible(), "Party master persists through profiles")) return false;
            runtime.general_.Load(settings_store::FromIniText(""));
            runtime.alliance_first_.State().enabled = false;
            if (!check(runtime.party_.PreviewVisible() && !runtime.alliance_first_.PreviewVisible() && runtime.alliance_second_.PreviewVisible() && runtime.ConfigPageVisible(page),
                "reenabling Party restores only the enabled lists")) return false;
            return true;
        }
        static auto EnabledModules(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-enabled-modules-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            std::filesystem::create_directory(root);
            struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } cleanupFiles{root};
            hud_runtime runtime;
            runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.editing_ = true;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            for (auto* module : runtime.modules_.All())
            {
                if (module == &runtime.general_) continue;
                const auto enabled = module->Option(0);
                if (!check(enabled.kind == module_option_kind::toggle && enabled.toggle != nullptr,
                    "every configurable module exposes its existing enable switch")) return false;
                *enabled.toggle = true;
                const bool inventoryChild = std::find(runtime.inventory_pages_.begin() + 1, runtime.inventory_pages_.end(), module) != runtime.inventory_pages_.end();
                const bool partyChild = module == &runtime.alliance_first_ || module == &runtime.alliance_second_;
                const bool* flag = module == &runtime.inventory_[0] ? runtime.general_.InventoryEnabledFlag() : module == &runtime.party_ ? runtime.general_.PartyEnabledFlag() : enabled.toggle;
                size_t matches{};
                for (size_t i = general_module::enabled_modules_option; i < runtime.general_.OptionCount(); ++i)
                    matches += runtime.general_.Option(i).toggle == flag;
                if (!check(matches == (inventoryChild || partyChild ? 0u : 1u), "General groups Inventory and Party while retaining their child switches in module settings")) return false;
            }
            const auto casting = runtime.ConfigPages()[1];
            if (!check(std::strcmp(casting.id, "casting") == 0 && std::strcmp(casting.name, "Casting") == 0 &&
                casting.Count() == 3 && casting.primary == &runtime.cast_bar_ && casting.secondary == &runtime.cast_cost_ && casting.tertiary == &runtime.recast_ &&
                ui::config_controls::IconId(casting.id) == ui::icon_glyph::castbar,
                "Casting groups both original modules under one alphabetized sidebar entry")) return false;
            click(runtime.ConfigLayout().ModuleTab(1));
            if (!check(runtime.active_module_ == &runtime.cast_bar_ && runtime.HasConfigSections(), "Casting opens the Cast Bar settings")) return false;
            click(runtime.ConfigLayout().Section(1, 2));
            if (!check(runtime.active_module_ == &runtime.cast_cost_ && runtime.HasConfigSections() && runtime.HasConfigSubmenu(),
                "the Cast Cost selector opens its existing settings in Casting")) return false;
            click(runtime.ConfigLayout().Section(0, 2));
            if (!check(runtime.active_module_ == &runtime.cast_bar_, "the Cast Bar selector returns to its independent settings")) return false;
            const auto groupedPages = runtime.ConfigPages();
            const auto inventoryPage = std::find_if(groupedPages.begin(), groupedPages.end(), [](const auto& page) { return std::strcmp(page.id, "inventory") == 0; });
            if (!check(inventoryPage != groupedPages.end() && inventoryPage->Count() == 8 && inventoryPage->SectionCount() == 3 &&
                inventoryPage->SectionModule(1) == &runtime.equipment_ && inventoryPage->SectionModule(2) == &runtime.gil_,
                "Inventory groups storage, Equipment and Gil under three settings selectors")) return false;
            for (const auto& page : groupedPages)
                if (!check(std::strcmp(page.id, "equipment") != 0 && std::strcmp(page.id, "gil") != 0,
                    "Equipment and Gil have no duplicate sidebar entries")) return false;
            click(runtime.ConfigLayout().ModuleTab(static_cast<size_t>(inventoryPage - groupedPages.begin())));
            for (size_t index = 0; index < runtime.inventory_.size(); ++index)
            {
                click(ui::config_controls::Segment(runtime.ConfigLayout().ContentTabs(), index, runtime.inventory_.size()));
                if (!check(runtime.active_module_ == &runtime.inventory_[index] && runtime.IsInventorySettings() &&
                    inventoryPage->SectionContains(0, runtime.active_module_) && runtime.ConfigSoundTarget(viewport) != 0 &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                    "each storage selector retains its settings, Inventory section, hover and sound target")) return false;
            }
            click(runtime.ConfigLayout().Section(1, 3));
            if (!check(runtime.active_module_ == &runtime.equipment_ && !runtime.IsInventorySettings() && runtime.GroupOptionCount() > 0,
                "Equipment opens its original settings inside Inventory")) return false;
            click(runtime.ConfigLayout().Section(2, 3));
            if (!check(runtime.active_module_ == &runtime.gil_ && !runtime.IsInventorySettings() && runtime.GroupOptionCount() > 0,
                "Gil opens its original settings inside Inventory")) return false;
            click(runtime.ConfigLayout().ModuleTab(0));
            click(runtime.ConfigLayout().Submenu(general_module::enabled_modules_section, 2));
            if (!check(runtime.active_module_ == &runtime.general_ && ControlCount(runtime) == runtime.modules_.Count() - runtime.inventory_.size() - 2,
                "Enabled Modules exposes one Inventory group and the other module switches")) return false;
            const auto toggle = [&](hud_module& module) {
                size_t row{};
                const bool* flag = &module == &runtime.inventory_[0] ? runtime.general_.InventoryEnabledFlag() : &module == &runtime.party_ ? runtime.general_.PartyEnabledFlag() : module.Option(0).toggle;
                while (row < runtime.GroupOptionCount() && runtime.general_.Option(runtime.GroupOptionIndex(row)).toggle != flag) ++row;
                if (row == runtime.GroupOptionCount()) return false;
                const int maxScroll = std::max(0, (static_cast<int>((runtime.GroupOptionCount() + 1) / 2) - runtime.ConfigLayout().VisibleRows()) * 2);
                runtime.option_scroll_ = std::min(static_cast<int>(row / 2 * 2), maxScroll);
                click(runtime.ConfigLayout().OptionRow(static_cast<int>(row) - runtime.option_scroll_));
                return true;
            };
            const auto castBounds = runtime.cast_bar_.State().bounds;
            if (!toggle(runtime.cast_bar_) || !check(!runtime.cast_bar_.State().enabled && runtime.VisibleConfigPages().size() == runtime.ConfigPages().size(),
                "Casting remains available while Cast Cost is enabled")) return false;
            if (!toggle(runtime.cast_cost_) || !check(runtime.VisibleConfigPages().size() == runtime.ConfigPages().size() - 1,
                "disabling both casting modules removes their shared sidebar page")) return false;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored;
            for (auto* module : restored.modules_.All()) module->Load(saved);
            if (!check(!restored.cast_bar_.State().enabled && !restored.cast_cost_.State().enabled && restored.VisibleConfigPages().size() == runtime.VisibleConfigPages().size(),
                "existing saved enable keys restore sidebar visibility")) return false;
            if (!toggle(runtime.cast_cost_) || !toggle(runtime.cast_bar_) || !check(runtime.cast_bar_.State().enabled && runtime.cast_bar_.State().bounds.x == castBounds.x &&
                runtime.cast_bar_.State().bounds.y == castBounds.y, "enabling a module restores the sidebar without moving it")) return false;
            std::array<bool, 6> inventoryVisibility{};
            for (size_t i = 0; i < runtime.inventory_.size(); ++i)
                runtime.inventory_[i].State().enabled = inventoryVisibility[i] = i % 2 == 1;
            if (!toggle(runtime.inventory_[0])) return false;
            if (!check(runtime.ConfigPageVisible(*inventoryPage), "Equipment and Gil keep Inventory reachable when storage is disabled")) return false;
            if (!toggle(runtime.equipment_) || !toggle(runtime.gil_) || !check(!runtime.ConfigPageVisible(*inventoryPage),
                "the category hides when storage, Equipment and Gil are all disabled")) return false;
            if (!toggle(runtime.equipment_) || !check(runtime.ConfigPageVisible(*inventoryPage),
                "Equipment alone keeps the shared category visible")) return false;
            if (!toggle(runtime.gil_)) return false;
            for (size_t i = 0; i < runtime.inventory_.size(); ++i)
                if (!check(!runtime.inventory_[i].PreviewVisible() && !runtime.inventory_[i].ShouldRender({}) &&
                    runtime.inventory_[i].State().enabled == inventoryVisibility[i], "Inventory master hides HUD and previews without rewriting child visibility")) return false;
            const auto grouped = runtime.CaptureSettings();
            for (auto* module : restored.modules_.All()) module->Load(grouped);
            restored.inventory_[1].ResetDefaults(ui::theme::CreateDefault());
            if (!check(!restored.general_.InventoryEnabled() && !restored.inventory_[1].PreviewVisible() &&
                !grouped.ReadBool("modules", "inventory", true), "master switch persists through profiles and child resets retain the group gate")) return false;
            if (!toggle(runtime.inventory_[0])) return false;
            for (size_t i = 0; i < runtime.inventory_.size(); ++i)
                if (!check(runtime.inventory_[i].PreviewVisible() == inventoryVisibility[i] && runtime.inventory_[i].State().enabled == inventoryVisibility[i],
                    "reenabling Inventory restores previews only for enabled trackers")) return false;
            size_t inventoryOption = general_module::enabled_modules_option;
            while (runtime.general_.Option(inventoryOption).toggle != runtime.general_.InventoryEnabledFlag()) ++inventoryOption;
            const auto groupOption = runtime.general_.Option(inventoryOption);
            if (!check(groupOption.submodules != nullptr && std::strstr(groupOption.submodules, "Satchel") &&
                std::strstr(groupOption.submodules, "Wardrobes"), "Inventory shows a nested submodule summary with one master control")) return false;
            const auto visible = runtime.VisibleConfigPages();
            for (size_t index = 0; index < visible.size(); ++index)
            {
                click(runtime.ConfigLayout().ModuleTab(index));
                if (!check(runtime.active_module_ == visible[index].primary && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                    runtime.ConfigSoundTarget(viewport) == 0x100 + index, "sidebar drawing, clicks, hover and sound use the same compacted indices")) return false;
            }
            for (const auto& page : runtime.ConfigPages())
            {
                if (page.primary == &runtime.general_) continue;
                for (size_t member = 0; member < page.Count(); ++member) *page.Module(member)->Option(0).toggle = false;
                if (page.primary == &runtime.party_)
                {
                    if (!check(runtime.ConfigPageVisible(page), "Party settings stay reachable with all lists disabled")) return false;
                    continue;
                }
                if (page.primary == &runtime.inventory_[0])
                {
                    if (!check(runtime.ConfigPageVisible(page), "Inventory settings remain accessible when all individual trackers are hidden")) return false;
                    continue;
                }
                if (!check(!runtime.ConfigPageVisible(page), "pages disappear only when all of their members are disabled")) return false;
                if (page.Count() > 1)
                {
                    *page.Module(page.Count() - 1)->Option(0).toggle = true;
                    if (!check(runtime.ConfigPageVisible(page), "any enabled Chat, Party, Inventory or Extras member keeps its page reachable")) return false;
                    *page.Module(page.Count() - 1)->Option(0).toggle = false;
                }
            }
            runtime.active_module_ = &runtime.general_;
            runtime.active_config_section_ = general_module::enabled_modules_section;
            if (!toggle(runtime.inventory_[0])) return false;
            if (!toggle(runtime.party_)) return false;
            runtime.active_module_ = &runtime.target_;
            runtime.NormalizeConfigSelection();
            if (!check(runtime.active_module_ == &runtime.general_ && runtime.active_config_section_ == general_module::enabled_modules_section &&
                runtime.VisibleConfigPages().size() == 1, "a hidden active page falls back to General and every module can still be restored")) return false;
            click(runtime.ConfigLayout().ModuleTab(1));
            if (!check(runtime.active_module_ == &runtime.general_ && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow &&
                runtime.ConfigSoundTarget(viewport) == 0, "removed sidebar rows leave no click, cursor or sound targets")) return false;
            if (!toggle(runtime.target_) || !toggle(runtime.game_hud_)) return false;
            const auto recovered = runtime.VisibleConfigPages();
            if (!check(recovered.size() == 3 && std::strcmp(recovered[1].id, "target") == 0 && std::strcmp(recovered.back().id, "extras") == 0,
                "reenabling modules preserves General first, alphabetical pages and Extras last")) return false;
            click(runtime.ConfigLayout().Submenu(general_module::appearance_section, 2));
            return check(ControlCount(runtime) == 2, "global appearance retains its separate topic");
        }

        static auto NativeWindowsTab(void) -> bool
        {
            const auto root = std::filesystem::temp_directory_path() / ("xiui-native-windows-" + std::to_string(::GetCurrentProcessId()) + "-" + std::to_string(::GetTickCount64()));
            std::filesystem::create_directory(root);
            struct cleanup { std::filesystem::path root; ~cleanup() { std::error_code ignored; std::filesystem::remove_all(root, ignored); } } cleanupFiles{root};
            hud_runtime runtime;
            runtime.settings_path_ = (root / "xiuinative.ini").string();
            runtime.profiles_.Initialize(runtime.settings_path_);
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.game_hud_.SetAvailable(0xFFFFFFFF);
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto pages = runtime.ConfigPages();
            if (!check(pages.front().primary == &runtime.general_ && pages.back().Count() == 4 &&
                pages.back().Module(3) == &runtime.game_hud_, "General retains appearance and Extras contains Game HUD")) return false;
            click(runtime.ConfigLayout().ModuleTab(pages.size() - 1));
            click(runtime.ConfigLayout().Section(3, 4));
            if (!check(runtime.active_module_ == &runtime.game_hud_ && runtime.HasConfigSections() && runtime.HasConfigSubmenu() &&
                ControlCount(runtime) == native_hud_options.size() + 3, "Game HUD tab exposes native window controls and bulk actions")) return false;
            for (size_t i = 0; i < runtime.game_hud_.OptionCount(); ++i)
            {
                const auto expected = i == 0 ? 0 : i == 1 ? game_hud_module::hide_all_option : i == 2 ? game_hud_module::show_all_option : i - 2;
                if (!check(runtime.GroupOptionIndex(i) == expected, "native visibility options keep stable indices after the bulk actions")) return false;
            }
            click(runtime.ConfigLayout().OptionRow(3));
            *runtime.general_.Option(general_module::dark_backing_opacity_option).slider = 0.45f;
            const auto saved = runtime.CaptureSettings();
            game_hud_module restoredHud;
            general_module restoredGeneral;
            restoredHud.Load(saved); restoredGeneral.Load(saved);
            if (!check(!*restoredHud.Option(1).toggle && std::abs(restoredGeneral.DarkBackingOpacity() - 0.45f) < 0.01f &&
                !saved.ReadBool("game_hud", ("show_" + std::string{native_hud_options[0].key}).c_str(), true), "native visibility and appearance retain their saved keys")) return false;
            runtime.HandleCommand("general");
            if (!check(runtime.active_module_ == &runtime.general_ && ControlCount(runtime) == 2 && !runtime.HasConfigSections(),
                "General command opens only global appearance settings")) return false;
            runtime.HandleCommand("gamehud");
            if (!check(runtime.active_module_ == &runtime.game_hud_ && std::strcmp(runtime.ActiveConfigPage().id, "extras") == 0,
                "gamehud command opens the Extras tab")) return false;
            for (const float height : {350.0f, 600.0f})
            {
                const ui::config_layout layout{{40, 40, 1100, height}, pages.size()};
                const auto selector = layout.ProfileSelector(), add = layout.ProfileAdd(), actions = layout.ProfileActionsButton();
                const auto reset = layout.ProfileSelector(), last = layout.ModuleTab(pages.size() - 1);
                if (!check(selector.y == add.y && add.y == actions.y && selector.height == actions.height &&
                    selector.x + selector.width < add.x && add.x + add.width < actions.x &&
                    actions.x + actions.width <= 40 + ui::config_layout::sidebar_width - 8 &&
                    last.y + last.height <= reset.y && reset.y + reset.height < selector.y,
                    "profile selector, plus and ellipsis share one row without overlapping navigation")) return false;
            }
            click(runtime.ConfigLayout().ProfileActionsButton());
            return check(runtime.profile_actions_ && !runtime.profile_dropdown_, "same-row ellipsis opens the profile actions menu");
        }

        static auto DarkBacking(void) -> bool
        {
            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-dark-backing-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            hud_runtime runtime;
            runtime.settings_path_ = path.string();
            runtime.config_bounds_ = {40, 40, 820, 430};
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.general_;
            runtime.active_option_group_ = module_option_group::appearance;
            const auto option = runtime.general_.Option(general_module::dark_backing_opacity_option);
            if (!check(option.available && option.kind == module_option_kind::slider && option.minimum == 0 && option.maximum == 1 &&
                *option.slider == 1, "General exposes an independent dark backing slider with the existing appearance as its default")) return false;
            const auto track = ui::config_layout::SliderTrack(runtime.ConfigLayout().OptionRow(1));
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto originalTheme = runtime.design_system_.GetTheme();
            for (const float value : {0.0f, 0.5f, 1.0f})
            {
                runtime.mouse_x_ = static_cast<int>(track.x + track.width * value);
                runtime.mouse_y_ = static_cast<int>(track.y + track.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
                runtime.FlushSettings();
                general_module restored;
                restored.Load(settings_store{path.string()});
                runtime.design_system_.SetDarkBackingOpacity(restored.DarkBackingOpacity());
                if (!check(std::abs(runtime.design_system_.DarkBackingOpacity() - value) < 0.01f && !runtime.general_.TakeWindowTypeRequest() &&
                    runtime.design_system_.GetTheme().colors.border == originalTheme.colors.border &&
                    runtime.design_system_.GetTheme().meter.track_opacity == originalTheme.meter.track_opacity,
                    "dragging and reloading the backing slider preserves native theme and artwork opacity")) return false;
            }
            runtime.design_system_.SetDarkBackingOpacity(-1);
            if (!check(runtime.design_system_.DarkBackingOpacity() == 0, "negative backing opacity clamps to zero")) return false;
            runtime.design_system_.SetDarkBackingOpacity(2);
            runtime.general_.ResetDefaults(originalTheme);
            return check(runtime.design_system_.DarkBackingOpacity() == 1 && runtime.general_.DarkBackingOpacity() == 1,
                "renderer bounds and resetting General restore full backing opacity");
        }

        static inline uint32_t* window_record{};
        static inline unsigned window_writes{};
        static auto __cdecl SetWindowType(const uint32_t id, const int value) -> int
        {
            if (id != 0x12) return -1;
            window_record[2] = value;
            ++window_writes;
            return 1;
        }

        static auto WindowTheme(void) -> bool
        {
            std::array<uint8_t, 8> config{};
            std::array<uint32_t, 19 * 11> table{};
            const auto configPointer = reinterpret_cast<uintptr_t>(config.data()), tablePointer = reinterpret_cast<uintptr_t>(table.data());
            std::memcpy(config.data() + 4, &tablePointer, sizeof(tablePointer));
            window_record = table.data() + 18 * 11;
            window_record[1] = 18; window_record[2] = 1; window_record[7] = 7;
            window_writes = 0;
            native_window_config client;
            client.config_slot_ = reinterpret_cast<uintptr_t>(&configPointer);
            client.setter_ = reinterpret_cast<uintptr_t>(&SetWindowType);
            if (!check(client.Type() == 1 && client.SetType(4) && client.Type() == 4 && window_writes == 1 &&
                client.SetType(4) && !client.SetType(8) && !client.SetType(-1) && window_writes == 1,
                "Window Type uses the native setter once and rejects invalid or unchanged choices")) return false;
            general_module module;
            module.UpdateWindowType(client.Type());
            auto option = module.Option(general_module::window_theme_option);
            if (!check(option.available && option.choice_count == 8 && *option.choice == 4, "theme control reflects the current client setting")) return false;
            *option.choice = 2;
            module.OnOptionChanged(general_module::window_theme_option);
            module.UpdateWindowType(uint8_t{4});
            const auto request = module.TakeWindowTypeRequest();
            if (!check(*option.choice == 2 && request == 2 && !module.TakeWindowTypeRequest() && client.SetType(*request),
                "a UI change survives snapshot refresh and is consumed once on the game thread")) return false;
            module.UpdateWindowType(client.Type());
            module.ResetDefaults(ui::theme::CreateDefault());
            if (!check(*option.choice == 2 && client.Type() == 2 && window_writes == 2,
                "resetting XIUI preferences does not overwrite the client theme")) return false;
            module.UpdateWindowType(uint8_t{6});
            if (!check(*option.choice == 6, "changes made in the native menu sync back into XIUI")) return false;
            module.UpdateWindowType(std::nullopt);
            window_record[1] = 19;
            return check(!module.Option(general_module::window_theme_option).available && !client.Type() && !client.SetType(0),
                "unavailable or mismatched native settings disable the control");
        }

        static auto Presentation(const char* dat) -> bool
        {
            if (!WindowTheme()) return false;
            hud_runtime runtime;
            std::array<bool, static_cast<size_t>(ui::icon_glyph::unknown)> icons{};
            const auto pages = runtime.ConfigPages();
            for (const auto& page : pages)
            {
                const auto id = static_cast<size_t>(ui::config_controls::IconId(page.id));
                if (!check(id < icons.size() && !icons[id], "every config sidebar module has a distinct icon")) return false;
                icons[id] = true;
            }
            auto text = ui::theme::CreateDefault().body_text;
            text.glow.enabled = text.sheen.enabled = true;
            const auto label = ui::config_controls::TextStyle(text), button = ui::config_controls::TextStyle(text, true);
            if (!check(label.shadow.enabled && !label.stroke.enabled && !label.glow.enabled && !label.sheen.enabled &&
                button.shadow.enabled && button.stroke.enabled, "config labels have shadows and button text also has an outline")) return false;
            const auto theme = ui::theme::CreateDefault();
            const auto regular = ui::button_renderer::TextStyle(theme, ui::config_controls::TextStyle(theme.caption_text, true), false);
            const auto active = ui::button_renderer::TextStyle(theme, regular, true);
            const auto hudButton = ui::button_renderer::TextStyle(theme, theme.meter_text, false);
            if (!check(regular.top_color == theme.colors.text && regular.bottom_color == theme.colors.text &&
                active.top_color == theme.colors.control_selected && regular.shadow.enabled && regular.stroke.enabled &&
                hudButton.top_color != hudButton.bottom_color && hudButton.stroke.enabled,
                "shared buttons brighten regular labels, retain active colors, and preserve HUD gradients and effects")) return false;
            ui::design_system paletteDesign;
            paletteDesign.SetControlPalette(0xFFC0C0FE, 0xFFFEC080);
            paletteDesign.SetWindowPalette(0xFF303840);
            if (!check(paletteDesign.GetTheme().colors.control != 0xFFC0C0FE && paletteDesign.GetTheme().colors.control_selected == 0xFFFEC080 &&
                paletteDesign.GetTheme().colors.accent != 0xFFFEC080, "native control highlights stay warm across window changes without recoloring HUD accents")) return false;
            for (const float height : {350.0f, 430.0f, 720.0f})
            for (const bool sections : {false, true})
            {
                const ui::config_layout layout{{0, 0, 820, height}, pages.size(), sections};
                const auto last = layout.ModuleTab(pages.size() - 1);
                if (!check(last.y + last.height <= layout.ProfileSelector().y, "all sidebar rows clear profile controls even at minimum height")) return false;
                const auto row = layout.OptionRow(layout.VisibleSlots() - 1);
                const auto control = ui::config_layout::ChoiceButton(row), description = ui::config_layout::Description(row);
                if (!check(control.y + control.height < description.y && description.x + description.width <= row.x + row.width &&
                    description.y + description.height <= height - 24, "option controls and descriptions stay separate and clear the footer")) return false;
            }
            const D3DVIEWPORT8 viewport{20, 10, 1280, 720, 0, 1};
            const auto tip = ui::config_layout::Tooltip({1250, 690, 40, 24}, 300, viewport);
            if (!check(tip.x >= 28 && tip.x + tip.width <= 1292 && tip.y + tip.height <= 722,
                "edge tooltips remain inside the viewport")) return false;
            const auto below = ui::config_layout::Tooltip({100, 100, 280, 56}, 300, viewport);
            if (!check(below.y > 156, "option descriptions place their tooltip below the controls")) return false;
            if (dat == nullptr) return true;
            std::ifstream file{dat, std::ios::binary};
            const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{file}, {}};
            bool haveButtons{}, haveHighlight{};
            for (size_t at = 0; at + 32 <= bytes.size();)
            {
                uint32_t meta{}; std::memcpy(&meta, bytes.data() + at + 4, 4);
                const size_t size = ((meta >> 7) & 0xFFFFF) * size_t{16};
                if (size < 16 || size > bytes.size() - at) break;
                if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(bytes.data() + at + 16), 16} == "menu    windowps")
                {
                    auto definition = resources::DecodeMenuSprite(std::span{bytes}.subspan(at + 16, size - 16), 13);
                    if (!check(definition.has_value(), "decode the native button definition")) return false;
                    auto& quads = definition->front();
                    std::erase_if(quads, [](const auto& q) { return q.texture != "buttonto"; });
                    if (!check(quads.size() == 2 && quads[0].u == 16 && quads[0].v == 0 && quads[0].width == 48 &&
                        quads[1].u == 0 && quads[1].v == 16 && quads[1].width == 40 && quads[1].points[6] == 88 &&
                        quads[0].additive && quads[1].additive && quads[0].colors[0] == 0x7060607F,
                        "native two-piece button geometry, blue vertex color, UVs and blend survive extraction")) return false;
                    haveButtons = true;
                }
                else if ((meta & 0x7F) == 0x31 && std::string_view{reinterpret_cast<const char*>(bytes.data() + at + 16), 16} == "menu    ncol    ")
                {
                    const auto palette = resources::DecodeMenuSprite(std::span{bytes}.subspan(at + 16, size - 16), 8);
                    if (!check(palette && palette->front().size() == 1 && palette->front().front().colors[0] == 0x7F7F6040,
                        "native orange palette entry preserves red/blue channel order and alpha")) return false;
                    haveHighlight = true;
                }
                at += size;
            }
            return check(haveButtons && haveHighlight, "native button and highlight DAT entries must be present");
        }

        static auto EmptyCategories(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            size_t removed{};
            for (auto* module : runtime.modules_.All())
            {
                runtime.active_module_ = module;
                const auto topics = module->ConfigSections();
                if (!topics.empty())
                {
                    for (size_t topic = 0; topic < topics.size(); ++topic)
                    {
                        runtime.active_config_section_ = topic;
                        if (!check(runtime.GroupOptionCount() != 0, "every custom module topic has settings")) return false;
                    }
                    continue;
                }
                const auto categories = runtime.ConfigCategories();
                if (!check(categories.count != 0, "each module retains at least one populated category")) return false;
                removed += 5 - categories.count;
                size_t reachable{};
                for (size_t index = 0; index < categories.count; ++index)
                {
                    runtime.active_option_group_ = categories.groups[index];
                    if (!check(runtime.GroupOptionCount() != 0, "every visible standard category has settings")) return false;
                    reachable += runtime.GroupOptionCount();
                }
                if (!check(reachable == module->OptionCount(), "filtering empty categories keeps every setting reachable")) return false;
            }
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            runtime.active_module_ = &runtime.general_;
            runtime.active_option_group_ = module_option_group::text;
            const auto categories = runtime.ConfigCategories();
            if (!check(categories.count == 1 && categories.groups[0] == module_option_group::appearance,
                "General only shows its populated Appearance category")) return false;
            runtime.option_scroll_ = 8;
            click(runtime.ConfigLayout().Category(0, categories.count));
            if (!check(runtime.active_option_group_ == module_option_group::appearance && ControlCount(runtime) == 2 && runtime.option_scroll_ == 0,
                "switching from a missing category selects a valid category and resets scrolling")) return false;
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                "visible category buttons retain their hover cursor")) return false;
            click(runtime.ConfigLayout().Category(4, 5));
            if (!check(runtime.active_option_group_ == module_option_group::appearance && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
                "removed tabs leave no clickable or hoverable ghost buttons")) return false;
            std::printf("Audited %zu modules; removed %zu empty standard categories.\n", runtime.modules_.Count(), removed);
            return true;
        }

        static auto SecondaryMenus(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            struct page_case { hud_module* module; size_t page, text_selector; };
            for (const auto test : {page_case{&runtime.player_, 1, 6}, page_case{&runtime.target_, 3, 4}, page_case{&runtime.enemy_list_, 4, 6}})
            {
                click(runtime.ConfigLayout().ModuleTab(test.page));
                const auto topics = test.module->ConfigSections();
                if (!check(runtime.active_module_ == test.module && runtime.HasConfigSubmenu() && topics.size() == 7 && runtime.ConfigCategories().count == 0,
                    "Player, Target and Enemy List use the secondary menu instead of top categories")) return false;
                std::vector<bool> covered(test.module->OptionCount());
                for (size_t topic = 0; topic < topics.size(); ++topic)
                {
                    click(runtime.ConfigLayout().Submenu(topic, topics.size()));
                    if (!check(runtime.active_config_section_ == topic && runtime.GroupOptionCount() != 0 &&
                        runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                        "each secondary section has settings and shares its click and hover bounds")) return false;
                    for (size_t index = 0; index < runtime.GroupOptionCount(); ++index) covered[runtime.GroupOptionIndex(index)] = true;
                }
                if (!check(std::all_of(covered.begin(), covered.end(), [](bool value) { return value; }),
                    "secondary sections keep every existing setting reachable")) return false;
                click(runtime.ConfigLayout().OptionRow(0));
                if (!check(runtime.GroupOptionIndex(0) == test.text_selector && *test.module->Option(test.text_selector).choice == 1,
                    "each Text section exposes and updates its own text selector")) return false;
                click(runtime.ConfigLayout().Category(4, 5));
                if (!check(runtime.active_config_section_ == static_cast<size_t>(bar_config_section::text) &&
                    runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::arrow,
                    "the old top category area contains no ghost buttons")) return false;
            }
            runtime.player_.Settings().style_edit_mode = 1;
            click(runtime.ConfigLayout().ModuleTab(1));
            click(runtime.ConfigLayout().Submenu(static_cast<size_t>(bar_config_section::bar_style), 7));
            const bool hpCaps = runtime.player_.Settings().hp_style.themed_bookends, mpCaps = runtime.player_.Settings().mp_style.themed_bookends;
            click(runtime.ConfigLayout().OptionRow(1));
            click(runtime.ConfigLayout().OptionRow(4));
            return check(runtime.player_.Settings().selected_bar == 1 && runtime.player_.Settings().mp_style.themed_bookends != mpCaps &&
                runtime.player_.Settings().hp_style.themed_bookends == hpCaps,
                "bar selectors inside secondary sections edit only the selected resource in Per bar mode");
        }

        static auto ChatExtrasMenus(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto visit = [&](hud_module& module) {
                const auto topics = module.ConfigSections();
                if (!check(runtime.active_module_ == &module && runtime.HasConfigSubmenu() && runtime.ConfigCategories().count == 0,
                    "Chat and every Extras module use secondary sections instead of top categories")) return false;
                std::vector<size_t> owners(module.OptionCount());
                for (size_t topic = 0; topic < topics.size(); ++topic)
                {
                    click(runtime.ConfigLayout().Submenu(topic, topics.size()));
                    if (!check(runtime.active_config_section_ == topic && runtime.GroupOptionCount() != 0 &&
                        runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand,
                        "each Chat and Extras section has settings with matching click and hover bounds")) return false;
                    for (size_t index = 0; index < runtime.GroupOptionCount(); ++index) ++owners[runtime.GroupOptionIndex(index)];
                }
                return check(std::all_of(owners.begin(), owners.end(), [](size_t count) { return count == 1; }),
                    "every existing Chat and Extras option appears in exactly one section");
            };
            click(runtime.ConfigLayout().ModuleTab(10));
            if (!visit(runtime.chat_)) return false;
            click(runtime.ConfigLayout().Section(1));
            if (!check(runtime.active_config_section_ == 4, "switching chat windows retains the matching Text section") || !visit(runtime.chat_second_)) return false;
            click(runtime.ConfigLayout().Section(0));
            click(runtime.ConfigLayout().Submenu(0, 5));
            const bool secondEnabled = runtime.chat_second_.State().enabled;
            click(runtime.ConfigLayout().OptionRow(2));
            if (!check(runtime.GroupOptionIndex(2) == 11 && runtime.chat_second_.State().enabled != secondEnabled,
                "the second chat window toggle remains functional in General")) return false;
            click(runtime.ConfigLayout().Submenu(1, 5));
            const int primarySource = *runtime.chat_.Option(8).choice, secondarySource = *runtime.chat_second_.Option(7).choice;
            click(runtime.ConfigLayout().OptionRow(1));
            if (!check(*runtime.chat_.Option(8).choice == (primarySource + 1) % 3 && *runtime.chat_second_.Option(7).choice == secondarySource,
                "Messages updates the active window's native message source independently")) return false;

            click(runtime.ConfigLayout().ModuleTab(11));
            if (!visit(runtime.compass_)) return false;
            click(runtime.ConfigLayout().Submenu(2, 4));
            click(runtime.ConfigLayout().Section(1, 3));
            if (!check(runtime.active_module_ == &runtime.world_info_ && runtime.active_config_section_ == 0 && runtime.GroupOptionIndex(0) == 0,
                "a section missing from the next Extras module falls back to General immediately")) return false;
            if (!visit(runtime.world_info_)) return false;
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(runtime.GroupOptionIndex(0) == 5 && *runtime.world_info_.Option(5).choice == 1,
                "World Info keeps its text selector alongside the profile controls")) return false;
            click(runtime.ConfigLayout().Section(0, 3));
            if (!check(runtime.active_config_section_ == 0, "different Extras sections do not inherit unrelated indices")) return false;
            click(runtime.ConfigLayout().Section(1, 3));
            click(runtime.ConfigLayout().Submenu(2, 3));
            click(runtime.ConfigLayout().Section(2, 3));
            return check(runtime.active_config_section_ == 0 && visit(runtime.commands_),
                "Commands has only its populated General section without an invalid selection");
        }

        static auto SettingsSearch(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.party_.meters_[0].height = 11; runtime.party_.meters_[1].height = 9;
            runtime.party_.meters_[2].height = 7; runtime.party_.meters_[3].height = 5;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto layout = runtime.ConfigLayout();
            if (!check(layout.InputMode().width == 24 && layout.InputModeMenu().width >= 180 &&
                layout.SearchInput().x + layout.SearchInput().width < layout.InputMode().x &&
                layout.HeaderTitle().x + layout.HeaderTitle().width < layout.SearchInput().x,
                "compact input icon retains readable options and leaves a separate header search field")) return false;
            const auto before = runtime.CaptureSettings().Preferences("").ToIniText();
            const auto started = std::chrono::steady_clock::now();
            const auto collapsedSearch = runtime.SettingsSearchBounds();
            if (!check(collapsedSearch.width == 24 && runtime.ControllerConfigItems(config_controller_region::utilities).back().bounds.width == 24,
                "search starts as a compact icon for pointer and controller focus")) return false;
            runtime.mouse_x_ = static_cast<int>(collapsedSearch.x + 12); runtime.mouse_y_ = static_cast<int>(collapsedSearch.y + 11);
            if (!check(runtime.ProcessSettingsSearchClick(viewport), "the search icon opens the existing search action")) return false;
            runtime.settings_search_animation_at_ = 1000;
            runtime.AnimateSettingsSearch(1090);
            const auto expandingSearch = runtime.SettingsSearchBounds();
            if (!check(expandingSearch.width > 24 && expandingSearch.width < 248 &&
                expandingSearch.x + expandingSearch.width == collapsedSearch.x + collapsedSearch.width,
                "search expands leftward smoothly without moving the header's other actions")) return false;
            runtime.AnimateSettingsSearch(1180);
            if (!check(runtime.SettingsSearchBounds().width == 248, "search reaches its full input width")) return false;
            if (!check(runtime.settings_search_open_ && runtime.KeyboardFocused() && runtime.settings_search_entries_.size() > 1000 &&
                runtime.CaptureSettings().Preferences("").ToIniText() == before,
                "building the cross-category search catalog preserves every live preference")) return false;
            std::printf("Settings search: %zu entries in %.2f ms\n", runtime.settings_search_entries_.size(),
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("PaRtY HP height"); runtime.UpdateSettingsSearch();
            if (!check(!runtime.settings_search_matches_.empty(), "search matches multiple case-insensitive terms across category and HP element paths")) return false;
            const auto found = std::find_if(runtime.settings_search_matches_.begin(), runtime.settings_search_matches_.end(), [&](const auto index) {
                const auto& entry = runtime.settings_search_entries_[index];
                return entry.module == "party" && entry.label_terms.find("height") != std::string::npos && entry.path_terms.find(" hp") != std::string::npos;
            });
            if (!check(found != runtime.settings_search_matches_.end(), "party HP height is indexed in its actual element context")) return false;
            const auto entry = runtime.settings_search_entries_[*found];
            const auto heights = [&] { return std::array{runtime.party_.meters_[0].height, runtime.party_.meters_[1].height,
                runtime.party_.meters_[2].height, runtime.party_.meters_[3].height}; };
            const auto height = heights();
            if (!check(runtime.SelectSettingsSearch(static_cast<size_t>(found - runtime.settings_search_matches_.begin())) &&
                runtime.active_module_ == &runtime.party_ && !runtime.settings_search_open_ && heights() == height &&
                runtime.settings_search_highlight_option_ == entry.option && runtime.MatchesConfigOption(entry.option),
                "opening a result navigates to the actual setting without editing bar values")) return false;
            runtime.BeginSettingsSearch();
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("clear me"); runtime.UpdateSettingsSearch();
            const auto clearInput = runtime.SettingsSearchBounds();
            runtime.mouse_x_ = static_cast<int>(clearInput.x + clearInput.width - 12); runtime.mouse_y_ = static_cast<int>(clearInput.y + 11);
            if (!check(runtime.ProcessSettingsSearchClick(viewport) && runtime.settings_search_open_ && runtime.settings_search_input_.Editor().Text().empty() &&
                runtime.settings_search_input_.Focused() && runtime.settings_search_matches_.empty(), "the typed-query X clears results while preserving typing focus")) return false;
            runtime.settings_search_input_.Insert("party hp"); runtime.UpdateSettingsSearch();
            runtime.HandleSettingsSearchController(controller_action::actions, true);
            if (!check(!runtime.settings_search_input_.Editor().Text().empty(), "held controller Actions cannot repeat the clear operation")) return false;
            runtime.HandleSettingsSearchController(controller_action::actions, false);
            if (!check(runtime.settings_search_input_.Editor().Text().empty() && runtime.KeyboardFocused(), "controller Actions shares Clear without closing search")) return false;
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("no_such_setting_4829"); runtime.UpdateSettingsSearch();
            if (!check(runtime.settings_search_matches_.empty() && !runtime.SelectSettingsSearch(0), "no-match searches cannot activate a setting")) return false;
            runtime.HandleSettingsSearchKey(VK_ESCAPE, 0, nullptr);
            if (!check(!runtime.settings_search_open_ && !runtime.KeyboardFocused(), "Escape releases search input without leaking into chat")) return false;
            runtime.settings_search_animation_at_ = 2000; runtime.AnimateSettingsSearch(2180);
            runtime.mouse_x_ = static_cast<int>(layout.SearchInput().x + 8);
            if (!check(runtime.SettingsSearchBounds().width == 24 && runtime.SettingsSearchTarget(viewport) == 0,
                "closing search returns to the icon and releases the expanded input's old hit area")) return false;
            runtime.BeginSettingsSearch();
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("party hp"); runtime.UpdateSettingsSearch();
            const ui::settings_search_layout results{runtime.ConfigLayout(), runtime.config_bounds_};
            runtime.MoveSettingsSearch(100);
            if (!check(runtime.settings_search_selection_ >= runtime.settings_search_scroll_ &&
                runtime.settings_search_selection_ < runtime.settings_search_scroll_ + results.rows,
                "keyboard and controller result navigation keeps the selection visible")) return false;
            runtime.HandleControllerConfig(controller_action::cancel, false);
            if (!check(!runtime.settings_search_open_, "controller Cancel shares search dismissal")) return false;
            runtime.general_.party_enabled_ = false;
            runtime.BeginSettingsSearch();
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("party hp height"); runtime.UpdateSettingsSearch();
            const auto row = ui::settings_search_layout{runtime.ConfigLayout(), runtime.config_bounds_}.Row(0);
            runtime.mouse_x_ = static_cast<int>(row.x + 10); runtime.mouse_y_ = static_cast<int>(row.y + 10);
            if (!check(runtime.ProcessSettingsSearchClick(viewport) && !runtime.settings_search_open_ && !runtime.general_.PartyEnabled() &&
                runtime.ConfigPageVisible(runtime.ActiveConfigPage()),
                "search can reveal a disabled category's controls without enabling it")) return false;
            runtime.BeginSettingsSearch();
            runtime.settings_search_input_.SelectAll(); runtime.settings_search_input_.Insert("party hp font"); runtime.UpdateSettingsSearch();
            const auto font = std::find_if(runtime.settings_search_matches_.begin(), runtime.settings_search_matches_.end(), [&](const auto index) {
                const auto& item = runtime.settings_search_entries_[index];
                return item.module == "party" && item.label == "Font" && item.path.find("HP values") != std::string::npos;
            });
            return check(font != runtime.settings_search_matches_.end() &&
                runtime.SelectSettingsSearch(static_cast<size_t>(font - runtime.settings_search_matches_.begin())) && runtime.party_.selected_text_ == 3 && heights() == height,
                "search opens the HP text element without changing its bar settings");
        }

        static auto ConfigSidebar(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true; runtime.config_bounds_ = {40, 40, 920, 600};
            runtime.active_module_ = &runtime.party_; runtime.active_config_section_ = 0;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto initial = runtime.ConfigLayout();
            const auto toggle = initial.SidebarToggle();
            runtime.mouse_x_ = static_cast<int>(toggle.x + 12); runtime.mouse_y_ = static_cast<int>(toggle.y + 11);
            if (!check(runtime.ConfigSoundTarget(viewport) == 6 && runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::hand &&
                !hud_runtime::Contains(initial.HeaderDragArea(), runtime.mouse_x_, runtime.mouse_y_) && runtime.ProcessConfigClick(viewport),
                "sidebar button owns its click and is separate from window dragging")) return false;
            const auto compact = runtime.ConfigLayout();
            if (!check(runtime.config_sidebar_collapsed_ && compact.SidebarWidth() == 52 &&
                compact.ContentArea().x == initial.ContentArea().x - 144 && compact.ContentArea().width > initial.ContentArea().width &&
                initial.Columns() == 1 && compact.Columns() == 2 && runtime.active_module_ == &runtime.party_,
                "icon-only sidebar returns width to responsive content without changing the selected page")) return false;
            const auto last = compact.ModuleTab(runtime.VisibleConfigPages().size() - 1);
            for (const auto& button : {compact.ProfileSelector(), compact.ProfileAdd(), compact.ProfileActionsButton()})
                if (!check(button.x > runtime.config_bounds_.x && button.x + button.width < runtime.config_bounds_.x + compact.SidebarWidth() &&
                    button.y > last.y + last.height && button.y + button.height < runtime.config_bounds_.y + runtime.config_bounds_.height,
                    "profile selector, creation and actions remain accessible within the compact sidebar")) return false;
            const auto pages = runtime.VisibleConfigPages(); const auto category = compact.ModuleTab(2);
            runtime.mouse_x_ = static_cast<int>(category.x + category.width * 0.5f); runtime.mouse_y_ = static_cast<int>(category.y + category.height * 0.5f);
            runtime.DrawConfig(viewport);
            if (!check(runtime.config_tooltip_ == pages[2].name && runtime.config_tooltip_anchor_.x == category.x &&
                runtime.ProcessConfigClick(viewport) && pages[2].Contains(runtime.active_module_),
                "compact category icons retain name tooltips and their normal click action")) return false;
            const auto saved = runtime.CaptureSettings(); hud_runtime restored; restored.LoadSettings(saved);
            if (!check(restored.config_sidebar_collapsed_ && restored.config_bounds_.width == 920,
                "sidebar preference reloads without changing the user's window width")) return false;
            runtime.controller_config_.active = true; runtime.controller_config_.region = config_controller_region::utilities;
            runtime.controller_config_.viewport = viewport; runtime.controller_config_.id = "sidebar";
            runtime.controller_surface_ = hud_runtime::controller_surface::config;
            if (!check(runtime.HandleControllerConfig(controller_action::confirm, false) && !runtime.config_sidebar_collapsed_ &&
                runtime.controller_config_.id == "sidebar", "controller activation shares the sidebar toggle and keeps stable focus")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(!runtime.config_sidebar_collapsed_, "held Confirm does not repeatedly toggle the sidebar")) return false;
            runtime.controller_config_.region = config_controller_region::options;
            const auto options = runtime.ControllerConfigItems(config_controller_region::options);
            if (!check(!options.empty(), "settings remain available after expanding the sidebar")) return false;
            runtime.controller_config_.id = options.back().id; runtime.RefreshControllerConfigFocus();
            const auto focused = runtime.controller_config_.id;
            runtime.ToggleConfigSidebar(viewport);
            return check(runtime.controller_config_.id == focused && runtime.config_sidebar_collapsed_,
                "content reflow preserves the focused setting");
        }

        static auto ConfigResize(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            runtime.controller_config_.viewport = viewport;
            const auto initial = runtime.config_bounds_;
            const auto grip = runtime.ConfigLayout().ResizeGrip();
            runtime.mouse_x_ = static_cast<int>(grip.x + grip.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(grip.y + grip.height * 0.5f);
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal &&
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport) && runtime.interaction_ == hud_runtime::interaction::resize_config,
                "the config grip owns a resize gesture")) return false;
            runtime.mouse_x_ += 500; runtime.mouse_y_ += 500;
            if (!check(runtime.ProcessPointerMessage(WM_MOUSEMOVE, MK_LBUTTON, viewport) &&
                runtime.config_bounds_.x == initial.x && runtime.config_bounds_.y == initial.y &&
                runtime.config_bounds_.width == viewport.Width - initial.x && runtime.config_bounds_.height == viewport.Height - initial.y,
                "config resizing remains captured beyond the window and stops at the viewport")) return false;
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.interaction_ == hud_runtime::interaction::none,
                "releasing a resize ends its gesture")) return false;
            runtime.ApplyConfigResize(initial, 2 | 8, -2000, -2000, viewport);
            if (!check(runtime.config_bounds_.width == runtime.ConfigLayout().MinimumWidth() &&
                runtime.config_bounds_.height == runtime.ConfigLayout().MinimumHeight(), "config resize enforces usable minimum dimensions")) return false;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored;
            restored.LoadSettings(saved);
            if (!check(restored.config_bounds_.width == runtime.config_bounds_.width && restored.config_bounds_.height == runtime.config_bounds_.height,
                "a custom config size survives settings reload")) return false;

            runtime.config_bounds_ = initial;
            runtime.BeginConfigResize();
            runtime.HandleConfigResize(controller_action::left, false);
            if (!check(runtime.config_bounds_.width == initial.width - 10 && runtime.CaptureSettings().ReadRect("layout.config", {}).width == initial.width,
                "controller resize previews the same geometry without saving an unfinished edit")) return false;
            runtime.HandleConfigResize(controller_action::confirm, true);
            if (!check(runtime.controller_config_.resizing, "held Confirm cannot commit a resize")) return false;
            runtime.HandleConfigResize(controller_action::cancel, false);
            if (!check(runtime.config_bounds_.width == initial.width && !runtime.controller_config_.resizing,
                "Cancel restores configuration dimensions")) return false;
            runtime.BeginConfigResize();
            runtime.HandleConfigResize(controller_action::actions, false);
            runtime.HandleConfigResize(controller_action::down, false);
            runtime.HandleConfigResize(controller_action::confirm, false);
            if (!check(runtime.config_bounds_.height == initial.height + 1 && !runtime.controller_config_.resizing,
                "controller resize supports fine adjustments and explicit commit")) return false;
            runtime.config_bounds_ = initial;
            runtime.BeginConfigResize(true);
            runtime.HandleConfigResize(controller_action::left, false);
            controller_status unavailable{};
            unavailable.enumeration_disabled = true; unavailable.suspended = true;
            unavailable.connection_known = true; unavailable.configuration_known = true;
            runtime.UpdateControllerStatus(unavailable);
            if (!check(runtime.controller_config_.resizing && runtime.controller_config_.resize_keyboard &&
                runtime.config_bounds_.width == initial.width - 10,
                "unavailable controller polls do not cancel keyboard-owned resizing")) return false;
            runtime.SetDevice(nullptr);
            if (!check(!runtime.controller_config_.resizing && runtime.config_bounds_.width == initial.width,
                "actual runtime device suspension cancels keyboard resizing and restores dimensions")) return false;
            runtime.config_collapsed_ = true;
            return check(runtime.ConfigResizeEdges(initial.x + 2, initial.y + 2) == 0,
                "collapsed configuration has no hidden resize edges");
        }

        static auto NotificationGroupsInput(void) -> bool
        {
            hud_runtime runtime;
            runtime.notifications_.Load(settings_store::FromIniText("[notifications]\ngroup_count=6\nenabled=0\n"));
            if (!check(!runtime.notification_groups_[4].PreviewVisible(), "disabled notifications hide every group preview")) return false;
            runtime.notifications_.State().enabled = true;
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.notifications_;
            runtime.active_config_section_ = 2;
            runtime.notifications_.SelectGroup(3);
            if (!check(runtime.MovementSelectionModule() == &runtime.notification_groups_[2] && runtime.notification_groups_[2].PreviewVisible(),
                "each group uses the shared movement adapter")) return false;
            runtime.FocusModuleConfig(runtime.notification_groups_[4]);
            if (!check(runtime.active_module_ == &runtime.notifications_ && runtime.notifications_.SelectedGroup() == 5,
                "clicking an independent group opens its owner settings")) return false;
            runtime.notifications_.Group(5).text.style.glow.enabled = true;
            runtime.notifications_.Group(5).state.bounds.x = 321;
            auto saved = runtime.CaptureSettings();
            notification_module restored; restored.Load(saved);
            if (!check(restored.Group(5).text.style.glow.enabled && restored.Group(5).state.bounds.x == 321,
                "independent group text effects and positions participate in profiles")) return false;
            *runtime.notifications_.Option(27).choice = 0; runtime.notifications_.OnOptionChanged(27);
            if (!check(!runtime.notification_groups_[4].PreviewVisible() && runtime.notifications_.SelectedGroup() < 2,
                "removed groups leave edit mode and selection falls back")) return false;
            runtime.editing_ = false;
            const auto now = ::GetTickCount64();
            runtime.notifications_.Center().Post({notification_kind::party_invite, 11, 0, "Party Invitation: Alice"}, now - 4000);
            runtime.notifications_.Center().Post({notification_kind::trade_invite, 22, 0, "Trade Request: Bob"}, now - 4000);
            const module_render_context context{runtime.design_system_};
            runtime.notifications_.Group(0).screen_anchor = false;
            runtime.notifications_.RenderGroup(0, context);
            const auto id = runtime.notifications_.FirstDetail();
            const auto hit = runtime.notifications_.Detail(id);
            const auto hovered = hit ? runtime.notifications_.DetailAt(hit->bounds.x + 1, hit->bounds.y + 1) : std::nullopt;
            if (!check(hit && hovered && hovered->id == id,
                "minified notices expose the same sender details to pointer inspection")) return false;
            runtime.notifications_.Group(0).style.pin_invites = false;
            runtime.notifications_.RenderGroup(0, context);
            if (!check(runtime.notifications_.Detail(id)->bounds.height > hit->bounds.height, "minification reduces the invitation footprint while retaining identity")) return false;
            runtime.notifications_.Group(0).style.pin_invites = true;
            runtime.notifications_.RenderGroup(0, context);
            if (!check(runtime.BeginControllerFocus(hud_runtime::controller_surface::notifications), "notification details have an explicit controller entry")) return false;
            runtime.HandleController({controller_action::down, controller_backend::none, 0, true, false, false, now});
            if (!check(runtime.controller_notification_id_ != id && runtime.notifications_.Center().Entries().size() == 2,
                "controller browsing follows stable notice identity without responding to invites")) return false;
            runtime.HandleController({controller_action::confirm, controller_backend::none, 0, true, false, false, now});
            if (!check(runtime.notifications_.Center().Entries().size() == 2, "reading a notice cannot accept or decline it")) return false;
            runtime.HandleController({controller_action::cancel, controller_backend::none, 0, true, false, false, now});
            if (!check(!runtime.ControllerCaptured(), "Cancel releases notification focus")) return false;
            const auto names = runtime.general_.NameplatesOption();
            if (!check(runtime.general_.ConfigSectionContains(general_module::visibility_section, names) &&
                !runtime.general_.ConfigSectionContains(general_module::appearance_section, names), "nameplate control belongs to Visibility")) return false;
            *runtime.general_.Option(names).toggle = true; runtime.general_.OnOptionChanged(names);
            if (!check(runtime.general_.TakeNameplatesRequest() == true && !runtime.general_.TakeNameplatesRequest(), "hiding names emits one native request")) return false;
            saved = runtime.CaptureSettings(); general_module general; general.Load(saved);
            if (!check(*general.Option(general.NameplatesOption()).toggle && general.TakeNameplatesRequest() == true, "nameplate preference survives save and load")) return false;
            *general.Option(general.NameplatesOption()).toggle = false; general.OnOptionChanged(general.NameplatesOption());
            return check(general.TakeNameplatesRequest() == false, "turning off nameplate hiding requests native restoration");
        }

        static auto AllUiVisibility(void) -> bool
        {
            hud_runtime runtime;
            runtime.game_hud_.Load(settings_store::FromIniText("[game_hud]\nenabled=1\nshow_player=0\nshow_party=1\n"));
            runtime.world_info_.State().enabled = false;
            const auto nativeMask = runtime.NativeHudHiddenMask();
            const auto playerBounds = runtime.player_.State().bounds;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.active_config_section_ = general_module::visibility_section;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            if (!check(runtime.GroupOptionIndex(0) == runtime.general_.HideAllUiOption(), "global hiding is reachable in General Visibility")) return false;
            const auto button = ui::config_layout::ChoiceButton(runtime.ConfigLayout().OptionRow(0));
            runtime.mouse_x_ = static_cast<int>(button.x + button.width * 0.5f);
            runtime.mouse_y_ = static_cast<int>(button.y + button.height * 0.5f);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            if (!check(runtime.IsVisible() && runtime.ConfirmingAction() && runtime.profile_keyboard_selection_ == 0 &&
                runtime.confirmation_message_.find("press /, type xiui show") != std::string::npos &&
                runtime.confirmation_message_.find("Typing will be invisible") != std::string::npos,
                "hide-all opens a Cancel-first warning with complete recovery instructions")) return false;
            runtime.HandleProfileKey(VK_RETURN, 0);
            runtime.HandleKey(VK_RETURN, static_cast<LPARAM>(0x80000000u), false);
            if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused() && runtime.NativeHudHiddenMask() == nativeMask,
                "default Enter cancels hiding and leaves visibility unchanged")) return false;
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            runtime.HandleProfileKey(VK_ESCAPE, 0);
            runtime.HandleKey(VK_ESCAPE, static_cast<LPARAM>(0x80000000u), false);
            if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused(), "Escape dismisses the hide-all warning")) return false;
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            const ui::profile_dialog_layout dialog{runtime.config_bounds_, viewport};
            runtime.mouse_x_ = static_cast<int>(dialog.Create().x + 5);
            runtime.mouse_y_ = static_cast<int>(dialog.Create().y + 5);
            runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
            const uint32_t allNative = (uint32_t{1} << native_hud_options.size()) - 1;
            if (!check(!runtime.IsVisible() && !runtime.IsEditing() && runtime.NativeHudHiddenMask() == allNative &&
                !runtime.ContextVisible(runtime.player_) && runtime.ControllerCapture() == controller_capture_mode::none && !runtime.ApplyConfirmedAction(),
                "explicit Hide all hides both HUDs exactly once and releases XIUI focus")) return false;
            if (!check(runtime.HandleMouseMessage(WM_LBUTTONUP, 0, 0, false) && !runtime.visibility_pointer_release_,
                "hiding from a pointer press consumes its release while the UI is hidden")) return false;
            runtime.SetVisible(true);
            if (!check(runtime.NativeHudHiddenMask() == nativeMask && !runtime.world_info_.State().enabled &&
                runtime.player_.State().bounds.x == playerBounds.x && runtime.player_.State().bounds.y == playerBounds.y,
                "restoring preserves native choices, disabled modules and positions")) return false;

            *runtime.game_hud_.Option(0).toggle = false;
            runtime.editing_ = true;
            runtime.controller_config_.region = config_controller_region::options;
            const auto items = runtime.ControllerConfigItems(config_controller_region::options);
            const auto item = std::find_if(items.begin(), items.end(), [&](const auto& value) { return value.index == runtime.general_.HideAllUiOption(); });
            if (!check(item != items.end(), "controller can reach the shared hide-all action")) return false;
            runtime.ActivateControllerConfigItem(*item, controller_action::confirm, true);
            if (!check(runtime.IsVisible(), "held Confirm cannot hide the UI repeatedly")) return false;
            runtime.ActivateControllerConfigItem(*item, controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::confirm, true);
            if (!check(runtime.IsVisible() && runtime.ConfirmingAction(), "controller activation opens the warning without hiding on held Confirm")) return false;
            runtime.HandleControllerConfig(controller_action::confirm, false);
            if (!check(runtime.IsVisible() && !runtime.profile_name_.Focused(), "controller confirmation defaults to Cancel")) return false;
            runtime.ActivateControllerConfigItem(*item, controller_action::confirm, false);
            runtime.HandleControllerConfig(controller_action::right, false);
            runtime.HandleControllerConfig(controller_action::confirm, false);
            if (!check(!runtime.IsVisible() && runtime.NativeHudHiddenMask() == allNative, "controller hides both HUDs even with native controls paused")) return false;
            runtime.HandleCommand("toggleui");
            return check(runtime.IsVisible() && runtime.NativeHudHiddenMask() == 0 && !*runtime.game_hud_.Option(0).toggle &&
                !*runtime.game_hud_.Option(1).toggle && *runtime.game_hud_.Option(2).toggle,
                "toggle command restores the native master switch and individual preferences");
        }

        static auto PetTabs(void) -> bool
        {
            hud_runtime runtime;
            runtime.pet_.Load(settings_store::FromIniText("[pet]\nenabled=1\n[pet.target]\nenabled=1\n"));
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, 1100, 600};
            runtime.active_module_ = &runtime.pet_;
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto selectedOnly = [&](const size_t selected) {
                const auto page = runtime.ActiveConfigPage();
                if (!check(runtime.active_module_ == &runtime.pet_ && runtime.pet_.ConfigView() == selected,
                    "Pet tab activation opens the matching settings view")) return false;
                for (size_t index = 0; index < page.SectionCount(); ++index)
                    if (!check(page.SectionContains(index, runtime.active_module_) == (index == selected),
                        "exactly one Pet tab is selected when pet types share a module")) return false;
                return true;
            };
            const auto count = runtime.ActiveConfigPage().SectionCount();
            for (size_t selected = 0; selected < count; ++selected)
            {
                const auto bounds = runtime.ConfigLayout().Section(selected, count);
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
                if (!selectedOnly(selected)) return false;
            }
            runtime.controller_config_.region = config_controller_region::sections;
            for (size_t selected = count; selected-- > 0;)
            {
                const auto items = runtime.ControllerConfigItems(config_controller_region::sections);
                if (!check(items.size() == count, "controller exposes all Pet settings tabs")) return false;
                runtime.ActivateControllerConfigItem(items[selected], controller_action::confirm, false);
                if (!selectedOnly(selected)) return false;
            }
            const auto page = runtime.ActiveConfigPage();
            for (size_t index = 0; index < count; ++index)
                if (!check(page.SectionContains(index, &runtime.pet_target_) == (index == count - 1),
                    "the independent Pet Target module retains its own tab selection")) return false;
            return true;
        }

        static auto Navigation(void) -> bool
        {
            hud_runtime runtime;
            runtime.editing_ = true;
            runtime.config_bounds_ = {40, 40, ui::config_layout::default_width, ui::config_layout::default_height};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            runtime.mouse_x_ = static_cast<int>(runtime.config_bounds_.x + runtime.config_bounds_.width - 5);
            runtime.mouse_y_ = static_cast<int>(runtime.config_bounds_.y + runtime.config_bounds_.height - 5);
            if (!check(runtime.ResolveCursorShape(viewport) == hud_runtime::cursor_shape::resize_diagonal,
                "the config corner advertises its resize grip")) return false;
            const auto click = [&](const ui::rect& bounds) {
                runtime.mouse_x_ = static_cast<int>(bounds.x + bounds.width * 0.5f);
                runtime.mouse_y_ = static_cast<int>(bounds.y + bounds.height * 0.5f);
                runtime.ProcessPointerMessage(WM_LBUTTONDOWN, MK_LBUTTON, viewport);
                runtime.ProcessPointerMessage(WM_LBUTTONUP, 0, viewport);
            };
            const auto pages = runtime.ConfigPages();
            if (!check(pages.size() == 12 && std::strcmp(pages[0].name, "General") == 0 &&
                std::strcmp(pages[7].name, "Equipment") == 0 && std::strcmp(pages[8].name, "Gil") == 0 &&
                std::strcmp(pages[11].name, "Extras") == 0 && pages[11].Count() == 3,
                "sidebar consolidates general settings and uses the shorter module names")) return false;
            for (const auto& page : pages)
                if (!check(std::strcmp(page.id, "chat2") != 0 && std::strcmp(page.id, "container") != 0 && std::strcmp(page.id, "gamehud") != 0,
                    "secondary chat, preview and native HUD have no duplicate sidebar entries")) return false;
            click(runtime.ConfigLayout().ModuleTab(0));
            if (!check(runtime.active_module_ == &runtime.game_hud_, "General opens the native HUD controls")) return false;
            if (!check(!runtime.HasConfigSections() && runtime.ActiveConfigPage().secondary == nullptr && runtime.modules_.Count() == 22,
                "General has no leftover container demo or preview section")) return false;
            click(runtime.ConfigLayout().ModuleTab(10));
            if (!check(runtime.active_module_ == &runtime.chat_, "Chat opens the first window editor")) return false;
            click(runtime.ConfigLayout().Section(1));
            if (!check(runtime.active_module_ == &runtime.chat_second_ && std::strcmp(runtime.ActiveConfigPage().id, "chat") == 0,
                "the second window editor belongs to the same Chat page")) return false;
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(runtime.chat_second_.State().enabled && runtime.chat_.State().enabled,
                "secondary-window controls update their original module")) return false;
            click(runtime.ConfigLayout().Section(0));
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(!runtime.chat_.State().enabled && runtime.chat_second_.State().enabled,
                "first-window edits preserve the independently saved second-window preferences")) return false;
            click(runtime.ConfigLayout().ModuleTab(2));
            if (!check(runtime.active_module_ == &runtime.party_ && runtime.ActiveConfigPage().Count() == 3,
                "the Party page owns all three independently movable party lists")) return false;
            click(runtime.ConfigLayout().Section(2, 3));
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(!runtime.alliance_second_.State().enabled && runtime.party_.State().enabled && runtime.alliance_first_.State().enabled,
                "the third Party section updates only Alliance 2")) return false;
            click(runtime.ConfigLayout().Section(1, 3));
            if (!check(runtime.active_module_ == &runtime.alliance_first_ && runtime.ActiveConfigPage().Contains(&runtime.alliance_first_),
                "alliance sections retain their original module identity")) return false;
            click(runtime.ConfigLayout().Section(0, 3));
            const auto topics = runtime.party_.ConfigSections();
            std::vector<bool> covered(runtime.party_.OptionCount());
            if (!check(topics.size() == 11 && runtime.HasConfigSubmenu(), "Party has a topic menu shared by its three lists")) return false;
            for (size_t topic = 0; topic < topics.size(); ++topic)
            {
                click(runtime.ConfigLayout().Submenu(topic, topics.size()));
                if (!check(runtime.active_config_section_ == topic && runtime.GroupOptionCount() != 0 &&
                    runtime.GroupOptionCount() <= static_cast<size_t>(runtime.ConfigLayout().VisibleSlots()),
                    "every Party topic fits the default window without scrolling")) return false;
                for (size_t index = 0; index < runtime.GroupOptionCount(); ++index)
                {
                    const auto option = runtime.GroupOptionIndex(index);
                    if (!check(option < covered.size() && runtime.party_.ConfigSectionContains(topic, option),
                        "topic filtering and click routing use the same option indices")) return false;
                    covered[option] = true;
                }
            }
            for (size_t index = 0; index < covered.size(); ++index)
                if (!check(covered[index] || !runtime.party_.Option(index).visible || !runtime.party_.Option(index).available,
                    "topic navigation keeps every applicable Party setting accessible")) return false;
            runtime.option_scroll_ = 4; runtime.active_color_ = &runtime.party_.low_hp_;
            click(runtime.ConfigLayout().Submenu(party_module::status_section, topics.size()));
            if (!check(runtime.GroupOptionIndex(0) == 15 && runtime.option_scroll_ == 0 && runtime.active_color_ == nullptr,
                "switching topics clears stale scrolling, color editors and reset confirmation")) return false;
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(!runtime.party_.buffs_ && runtime.alliance_first_.buffs_, "Status Effects routes its first toggle to the correct list")) return false;
            click(runtime.ConfigLayout().Section(1, 3));
            if (!check(runtime.active_config_section_ == party_module::status_section && runtime.GroupOptionIndex(0) == 15,
                "switching party lists retains the selected topic")) return false;
            click(runtime.ConfigLayout().Submenu(1, topics.size()));
            click(runtime.ConfigLayout().OptionRow(0));
            if (!check(runtime.alliance_first_.bottom_ && !runtime.party_.bottom_, "Layout settings preserve independent alliance preferences")) return false;
            for (float height : {350.0f, 600.0f})
            {
                const ui::config_layout layout{{0, 0, 1100, height}, 11, true, true};
                const auto last = layout.Submenu(topics.size() - 1, topics.size()), row = layout.OptionRow(0);
                if (!check(last.y + last.height < height - 20 && last.x + last.width < row.x &&
                    layout.Section(0, 3).x < row.x && layout.ContentArea().y <= row.y &&
                    layout.Footer().x == row.x, "secondary navigation, headings, controls and footer occupy separate bounds")) return false;
            }
            click(runtime.ConfigLayout().ModuleTab(1));
            return check(runtime.HasConfigSubmenu() && runtime.active_config_section_ == 0 && runtime.GroupOptionIndex(0) == 0,
                "switching to Player resets the secondary section to General");
        }

        static auto ExperiencePositionPersistence(void) -> bool
        {
            hud_runtime runtime;
            auto& exp = runtime.experience_;
            exp.State().bounds = {850, 1403, 860, 145};
            auto& settings = exp.Settings();
            settings.style.height = 6;
            settings.show_label = settings.show_current = settings.show_percent = settings.show_remaining = false;
            settings.show_points = settings.show_master_level = false;
            settings.readouts.fill(false);
            settings.readouts[static_cast<size_t>(experience_readout::job)] = true;
            settings.readout_anchor = 1; settings.readout_text.offset_y = -60;
            const auto saved = runtime.CaptureSettings();
            hud_runtime restored; restored.LoadSettings(saved);
            const D3DVIEWPORT8 viewport{0, 0, 2560, 1440, 0, 1};
            auto& loaded = restored.experience_;
            for (int pass = 0; pass < 3; ++pass) restored.EnsureLayout(viewport);
            if (!check(!loaded.PositionBoundsReady() && loaded.State().bounds.x == 850 && loaded.State().bounds.y == 1403,
                "loading a compact EXP bar near the screen bottom does not clamp against its old saved height")) return false;
            hud_snapshot snapshot{}; snapshot.exp_current = 200; snapshot.exp_needed = 1000;
            const module_render_context context{restored.design_system_, nullptr, true};
            loaded.Render(snapshot, context);
            restored.ClampLayout(viewport);
            if (!check(loaded.PositionBoundsReady() && loaded.State().bounds.height == 145 && loaded.State().bounds.x == 850 &&
                loaded.State().bounds.y == 1403 && loaded.State().content_bounds.y < 0,
                "the measured bar and above-bar readouts preserve their saved position despite an oversized layout rectangle")) return false;
            const auto roundtrip = restored.CaptureSettings();
            loaded.State().content_bounds = {0, 0, 860, 300};
            restored.LoadSettings(roundtrip); restored.EnsureLayout(viewport);
            if (!check(!loaded.PositionBoundsReady() && loaded.State().bounds.y == 1403 &&
                roundtrip.ReadRect("layout.exp", {}).y == 1403,
                "profile round trips clear stale EXP measurements before in-place reloads")) return false;
            loaded.Render(snapshot, context);
            const D3DVIEWPORT8 smaller{0, 0, 2560, 1080, 0, 1};
            restored.ClampLayout(smaller);
            const auto visual = restored.MovementBounds(loaded);
            return check(visual.y >= 0 && visual.y + visual.height <= smaller.Height && loaded.State().bounds.y < 1403,
                "measured EXP content still clamps into view after a real viewport reduction");
        }

        static auto DragPersistence(void) -> bool
        {
            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-drag-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            const settings_store store{path.string()};
            store.WriteBool("player", "show_identity", false);
            store.Flush();
            hud_runtime runtime{};
            runtime.settings_path_ = path.string();
            runtime.editing_ = true;
            runtime.player_.Settings().show_identity = false;
            *runtime.general_.Option(runtime.general_.GridStart() + 1).toggle = false;
            *runtime.general_.Option(runtime.general_.GridStart() + 2).toggle = false;
            runtime.config_bounds_ = {40, 40, 680, 430};
            const D3DVIEWPORT8 viewport{0, 0, 1920, 1080, 0, 1};
            const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
                runtime.mouse_x_ = static_cast<int>(x);
                runtime.mouse_y_ = static_cast<int>(y);
                return runtime.ProcessPointerMessage(message, buttons, viewport);
            };
            const auto idle = [&] {
                return runtime.interaction_ == hud_runtime::interaction::none && runtime.interaction_module_ == nullptr &&
                    runtime.active_slider_ == nullptr && !runtime.mouse_down_ && runtime.mouse_buttons_ == 0;
            };
            const auto moduleOrder = runtime.modules_.All();
            for (auto* module : moduleOrder)
            {
                if (!module->HasEditableBounds() || !module->PreviewVisible()) continue;
                const auto original = module->State().bounds;
                module->State().bounds.x = 900;
                module->State().bounds.y = 650;
                auto& layers = runtime.modules_.All();
                const auto layer = std::find(layers.begin(), layers.end(), module);
                std::rotate(layer, layer + 1, layers.end());
                const auto area = module->State().EditBounds();
                const auto x = area.x + std::min(20.0f, area.width / 2), y = area.y + std::min(20.0f, area.height / 2);
                send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
                if (!check(runtime.interaction_module_ == module, (std::string{"module presses begin their own drag: "} + module->Id() + "; actual=" + (runtime.interaction_module_ ? runtime.interaction_module_->Id() : "none")).c_str())) return false;
                send(WM_LBUTTONUP, 0, x + 50, y + 30);
                if (!check(runtime.settings_writer_.Flush(), "module layout writes finish successfully")) return false;
                const std::string section = module == &runtime.experience_ ? "layout.exp" : std::string{"layout."} + module->Id();
                const auto saved = store.ReadRect(section.c_str(), {});
                if (!check(idle() && saved.x == module->State().bounds.x && saved.y == module->State().bounds.y &&
                    (saved.x != 900 || saved.y != 650) && saved.width == module->State().bounds.width && saved.height == module->State().bounds.height,
                    (std::string{"dropping each module saves its final layout: "} + module->Id()).c_str())) return false;
                module->State().bounds = original;
            }
            runtime.modules_.All() = moduleOrder;
            send(WM_LBUTTONDOWN, MK_LBUTTON, 100, 50);
            send(WM_LBUTTONUP, 0, 120, 65);
            if (!check(runtime.settings_writer_.Flush(), "config layout writes finish successfully")) return false;
            const auto saved = store.ReadRect("layout.config", {});
            if (!check(idle() && saved.x == 60 && saved.y == 55, "config dragging saves only its final layout")) return false;
            runtime.editing_ = false;
            return check(!store.ReadBool("player", "show_identity", true), "layout gestures preserve the current module settings");
        }

        static auto Run(void) -> bool
        {
            const auto path = std::filesystem::temp_directory_path() / ("xiuinative-input-save-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
            struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
            hud_runtime runtime{};
            runtime.settings_path_ = path.string();
            runtime.editing_ = true;
            runtime.active_module_ = &runtime.player_;
            runtime.config_bounds_ = {40, 40, 680, 430};
            const D3DVIEWPORT8 viewport{0, 0, 1280, 720, 0, 1};
            const auto row = runtime.ConfigLayout().OptionRow(0);
            double maximumMessageMs = 0;
            const auto send = [&](uint32_t message, uint32_t buttons, float x, float y) {
                runtime.mouse_x_ = static_cast<int>(x);
                runtime.mouse_y_ = static_cast<int>(y);
                const auto start = std::chrono::steady_clock::now();
                const bool captured = runtime.ProcessPointerMessage(message, buttons, viewport);
                maximumMessageMs = std::max(maximumMessageMs,
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
                return captured;
            };
            const float x = row.x + row.width - 20, y = row.y + 20;
            auto& enabled = runtime.player_.State().enabled;
            const bool values = runtime.player_.Settings().show_values;
            if (!check(send(WM_LBUTTONDOWN, MK_LBUTTON, x, y) && !enabled, "a press applies immediately at its message position")) return false;
            send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
            runtime.UpdateMouseCapture(viewport, MK_LBUTTON);
            if (!check(!enabled, "duplicate messages and held-frame updates cannot apply the same press twice")) return false;
            const auto other = runtime.ConfigLayout().OptionRow(3);
            send(WM_MOUSEMOVE, MK_LBUTTON, other.x + 20, other.y + 20);
            send(WM_LBUTTONUP, 0, other.x + 20, other.y + 20);
            if (!check(!enabled && runtime.player_.Settings().show_values == values, "moving before the next frame cannot retarget a click")) return false;
            send(WM_LBUTTONDBLCLK, MK_LBUTTON, x, y);
            send(WM_LBUTTONUP, 0, x, y);
            if (!check(enabled, "the second press of a double-click changes the value once")) return false;
            for (int i = 0; i < 2; ++i)
            {
                send(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
                send(WM_LBUTTONUP, 0, x, y);
            }
            if (!check(enabled, "two complete clicks between frames are both retained")) return false;

            const bool labels = runtime.player_.Settings().show_labels;
            send(WM_LBUTTONDOWN, MK_LBUTTON, x, row.y + row.height);
            send(WM_LBUTTONUP, 0, x, row.y + row.height);
            if (!check(enabled && runtime.player_.Settings().show_labels != labels, "shared row edges belong only to the lower row")) return false;
            if (!check(!send(WM_LBUTTONDOWN, MK_LBUTTON, 1200, 1), "a game-owned press passes through")) return false;
            send(WM_MOUSEMOVE, MK_LBUTTON, x, y);
            if (!check(!send(WM_LBUTTONUP, 0, x, y) && enabled, "dragging into the config cannot activate a toggle")) return false;

            runtime.active_module_ = &runtime.game_hud_;
            const auto disabledRow = runtime.ConfigLayout().OptionRow(3);
            bool* shown = runtime.game_hud_.Option(1).toggle;
            send(WM_LBUTTONDOWN, MK_LBUTTON, disabledRow.x + 20, disabledRow.y + 20);
            send(WM_LBUTTONUP, 0, disabledRow.x + 20, disabledRow.y + 20);
            if (!check(*shown, "unavailable Game HUD toggles cannot change")) return false;
            runtime.game_hud_.SetAvailable(0xFFFFFFFF);
            send(WM_LBUTTONDOWN, MK_LBUTTON, disabledRow.x + 20, disabledRow.y + 20);
            send(WM_LBUTTONUP, 0, disabledRow.x + 20, disabledRow.y + 20);
            if (!check(!*shown, "available Game HUD toggles use the same click path")) return false;

            runtime.active_module_ = &runtime.player_;
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::bar_style);
            const auto barStyle = runtime.ConfigLayout().OptionRow(2);
            send(WM_LBUTTONDOWN, MK_LBUTTON, barStyle.x + 20, barStyle.y + 20);
            send(WM_LBUTTONUP, 0, barStyle.x + 20, barStyle.y + 20);
            if (!check(runtime.player_.Settings().hp_style.appearance == ui::meter_style::modern &&
                runtime.player_.Settings().mp_style.appearance == ui::meter_style::modern,
                "the Bar style choice updates shared meters through the normal config input path")) return false;
            const auto track = ui::config_layout::SliderTrack(runtime.ConfigLayout().OptionRow(3));
            send(WM_LBUTTONDOWN, MK_LBUTTON, track.x, track.y);
            send(WM_LBUTTONUP, 0, track.x + track.width + 1, track.y);
            if (!check(runtime.player_.Settings().hp_style.height == 80 && runtime.interaction_ == hud_runtime::interaction::none,
                "sliders apply the release position and end their drag without frame polling")) return false;
            const auto choice = runtime.ConfigLayout().OptionRow(0);
            send(WM_LBUTTONDOWN, MK_LBUTTON, choice.x + 20, choice.y + 20);
            send(WM_LBUTTONUP, 0, choice.x + 20, choice.y + 20);
            if (!check(runtime.player_.Settings().style_edit_mode == 1, "choice controls still apply immediately")) return false;
            runtime.active_config_section_ = static_cast<size_t>(bar_config_section::colors);
            const auto color = runtime.ConfigLayout().OptionRow(2);
            send(WM_LBUTTONDOWN, MK_LBUTTON, color.x + 20, color.y + 20);
            send(WM_LBUTTONUP, 0, color.x + 20, color.y + 20);
            const ui::color_picker_layout picker{runtime.ConfigLayout().ColorEditor(viewport)};
            const auto valueTrack = picker.Value();
            send(WM_LBUTTONDOWN, MK_LBUTTON, valueTrack.x + 2, valueTrack.y + 2);
            send(WM_LBUTTONUP, 0, valueTrack.x + 2, valueTrack.y - 1);
            const auto fill = runtime.player_.Settings().hp_color_top;
            runtime.FlushSettings();
            const settings_store saved{path.string()};
            if (!check(saved.ReadBool("player", "enabled", false) == enabled &&
                saved.ReadFloat("meter.player.hp", "height", 0) == 80 && saved.ReadInt("meter.player.hp", "appearance", -1) == ui::meter_style::modern &&
                saved.ReadInt("player", "style_edit_mode", -1) == 1 &&
                ((fill >> 16) & 0xFF) == 255 && saved.ReadArgb("meter.player.hp", "fill_top", 0) == fill,
                "queued toggle, choice, slider, and color edits persist together before state capture")) return false;
            runtime.OpenProfileAction(ui::profile_reset_action);
            runtime.ApplyConfirmedAction();
            runtime.HandleCommand("gamehud show all");
            runtime.ReloadSettings();
            const auto defaults = settings_store::FromIniText(runtime.default_settings_text_);
            if (!check(runtime.player_.State().enabled && runtime.player_.Settings().hp_style.height == defaults.ReadFloat("meter.player.hp", "height", runtime.design_system_.GetTheme().meter.height) &&
                *runtime.game_hud_.Option(1).toggle,
                "reload drains pending reset and command saves before loading settings")) return false;
            runtime.player_.Settings().show_values = false;
            runtime.ExitEdit();
            if (!check(runtime.settings_writer_.Flush() && !saved.ReadBool("player", "show_values", true),
                "leaving edit mode queues its final changes")) return false;
            std::printf("Maximum input callback time with persistence enabled: %.2f ms.\n", maximumMessageMs);
            return true;
        }
    };
}

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--startup-wiring-only") == 0)
    {
        if (!xiui::config_input_test::StartupWiring()) return 1;
        std::puts("Runtime startup dependencies and shared UTF-8 text fitting passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--cursor-ownership-only") == 0)
    {
        if (!cursor_ownership_cases()) return 1;
        std::puts("Cursor capture, render-thread release, visibility balance and native cursor ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--notification-feedback-only") == 0)
    {
        if (!xiui::RunNotificationFeedbackChecks() || !xiui::config_input_test::NotificationGroupsInput()) return 1;
        std::puts("Notification groups, bounded invitations, controller details, nameplate preferences and HP feedback checks passed.");
        return 0;
    }

    if (argc == 2 && std::strcmp(argv[1], "--all-ui-visibility-only") == 0)
    {
        if (!xiui::config_input_test::AllUiVisibility()) return 1;
        std::puts("Combined UI hiding, pointer release, controller activation and restored preferences passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--settings-search-only") == 0)
    {
        if (!xiui::config_input_test::SettingsSearch()) return 1;
        std::puts("Settings search catalog, matching, navigation, input ownership and compact input control passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-sidebar-only") == 0)
    {
        if (!xiui::config_input_test::ConfigSidebar()) return 1;
        std::puts("Collapsible config sidebar, hover names, responsive geometry, profile controls, persistence and controller focus passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-resize-only") == 0)
    {
        if (!xiui::config_input_test::ConfigResize() || !RunConfigResponsiveChecks()) return 1;
        std::puts("Config resizing, bounded geometry, responsive content, focus and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--target-ring-state-only") == 0) return RunTargetRingStateChecks() ? 0 : 1;
    if (argc >= 2 && std::strcmp(argv[1], "--target-rings-only") == 0) return RunTargetRingChecks(argc > 2 ? argv[2] : nullptr) ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--heal-prediction-only") == 0) return RunHealPredictionChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--profile-delete-only") == 0)
    {
        if (!xiui::config_input_test::ProfileDeletion()) return 1;
        std::puts("Profile delete confirmation, red button, cancellation, failures, Default fallback and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--exp-position-only") == 0)
    {
        if (!xiui::config_input_test::ExperiencePositionPersistence()) return 1;
        std::puts("EXP saved positions, measured bounds, profile reloads and viewport clamping passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--config-text-only") == 0)
    {
        if (!xiui::config_input_test::ConfigText()) return 1;
        std::puts("Config fonts, pixel letter spacing, shared text effects, HUD independence, key wrapping, persistence and reset passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--input-mode-only") == 0)
    {
        if (!xiui::config_input_test::HeaderInputMode()) return 1;
        std::puts("Header input mode pointer selection, controller availability ownership and status icon routing passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--scrollbar-only") == 0)
    {
        if (!xiui::config_input_test::ConfigScrollbar()) return 1;
        std::puts("Config scrollbar visibility, gutter, thumb drag, track paging and bounded row alignment passed.");
        return 0;
    }
    if ((argc >= 2 && argc <= 4) && std::strcmp(argv[1], "--party-presets-only") == 0)
    {
        if (!xiui::config_input_test::PartyPresets(argc >= 3 ? argv[2] : nullptr, argc == 4 ? argv[3] : nullptr)) return 1;
        std::puts("Party General preset cards, pointer/controller actions and persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--all-bars-only") == 0)
    {
        if (!xiui::config_input_test::AllBarsScope()) return 1;
        std::puts("All bars pointer/controller selection, per-setting edits and individual geometry preservation passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--element-tabs-only") == 0)
    {
        if (!xiui::config_input_test::ElementTabs()) return 1;
        std::puts("Direct element tabs, combined bar scope, full-width headers, wrapping, input ownership and saved choices passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-entry-only") == 0)
    {
        if (!xiui::config_input_test::PartyEntries()) return 1;
        std::puts("Party visible entry bounds, following job icons, SYNC visibility, cast sizing and independent persistence passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--choice-arrows-only") == 0)
    {
        if (!xiui::config_input_test::ChoiceArrows()) return 1;
        std::puts("Previous/next choice arrows, wraparound, callbacks, hover feedback, capture and dropdown behavior passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--text-rasterization-only") == 0)
    {
        if (!text_rasterization_cases()) return 1;
        std::puts("Native text sizes, antialiasing for all four fonts, effects, Unicode, stable digits and pixel controls passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--attribution-only") == 0)
    {
        if (!xiui::config_input_test::Attribution() || !xiui::config_input_test::CloseInput()) return 1;
        std::puts("Attribution header routing, credits, capture and existing close gesture checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--global-text-only") == 0)
    {
        if (!xiui::config_input_test::GlobalText()) return 1;
        std::puts("Global text controls, all-module updates, independent overrides, profile persistence and embedded Grammara font passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--target-recipient-only") == 0)
    {
        if (!xiui::config_input_test::TargetOfTargetSettings()) return 1;
        std::puts("Classic inline target-of-target geometry, arrow alignment, topic input, styled persistence and default migration checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-module-only") == 0)
    {
        if (!xiui::config_input_test::PartyModuleVisibility()) return 1;
        std::puts("Party master, per-list live/edit visibility, settings access and profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-target-settings-only") == 0)
    {
        if (!xiui::config_input_test::PartyModuleVisibility() || !xiui::config_input_test::TargetOfTargetSettings()) return 1;
        std::puts("Party master and preview visibility, accessible alliance settings, target-of-target controls and profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--enabled-modules-only") == 0)
    {
        if (!xiui::config_input_test::EnabledModules()) return 1;
        std::puts("Enabled Modules controls, filtered sidebar routing, grouped pages, fallback and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--native-windows-tab-only") == 0)
    {
        if (!xiui::config_input_test::NativeWindowsTab() || !xiui::config_input_test::WindowTheme()) return 1;
        std::puts("Extras Game HUD routing, saved settings, General appearance and same-row profile controls checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-preview-only") == 0)
    {
        if (!xiui::config_input_test::ChatSoundPreview() || !xiui::config_input_test::ChatCustomization()) return 1;
        std::puts("Dropdown play icons, Space previews, duplicate/release handling and normal sound selection checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--profile-sharing-only") == 0)
    {
        if (!xiui::config_input_test::ProfileSharing() || !xiui::config_input_test::Profiles()) return 1;
        std::puts("Profile duplication, portable key sharing/import, invalid-key handling and existing profile persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-customization-only") == 0)
    {
        if (!xiui::config_input_test::ChatCustomization() || !xiui::config_input_test::ChatAlertInput()) return 1;
        std::puts("Chat sound dropdown selection, popup input ownership, optional color picker and keyword field checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--chat-alerts-only") == 0)
    {
        if (!xiui::config_input_test::ChatAlertInput()) return 1;
        std::puts("Chat keyword field editing, commits, cancellation, length limits and keyboard ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--collapse-only") == 0)
    {
        if (!xiui::config_input_test::ConfigCollapse() || !xiui::config_input_test::CloseInput()) return 1;
        std::puts("Config collapse/restore, title-bar dragging, hidden input removal, HUD preview access and close capture checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--close-input-only") == 0)
    {
        if (!xiui::config_input_test::CloseInput()) return 1;
        std::puts("Config close press/release capture, cancellation and mouse block restoration checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--character-profiles-only") == 0)
    {
        if (!xiui::config_input_test::CharacterProfiles() || !xiui::config_input_test::Profiles()) return 1;
        std::puts("Per-character profile restoration, pending saves, identity loss, failure/fallback, shared selection and portability checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--profiles-only") == 0)
    {
        if (!xiui::config_input_test::Profiles()) return 1;
        std::puts("Profile dropdown, modal creation, validation, autosave, switching, session isolation and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--preview-focus-only") == 0)
    {
        if (!xiui::config_input_test::PreviewFocus()) return 1;
        std::puts("Preview module focus, shared-page routing, drag preservation and hidden-preview ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--exp-display-only") == 0)
    {
        if (!xiui::config_input_test::ExperienceDisplay()) return 1;
        std::puts("EXP display views, direct readout selection, ordering, filters, persistence and text separation checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--sounds-only") == 0)
    {
        if (!xiui::config_input_test::Sounds()) return 1;
        std::puts("Native UI sound routing, hover transitions, click/drag ownership, silent restore and HUD controls checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--slider-input-only") == 0)
    {
        if (!xiui::config_input_test::SliderValues()) return 1;
        std::puts("Typed slider values, validation, callbacks, persistence and input ownership checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--pet-hud-only") == 0)
    {
        if (!xiui::RunPetHudChecks() || !xiui::config_input_test::PetTabs()) return 1;
        std::puts("Pet HUD, portrait customization, persistence and tab selection checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--pet-tabs-only") == 0)
    {
        if (!xiui::config_input_test::PetTabs()) return 1;
        std::puts("Pet settings tabs select one view at a time through pointer and controller actions.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--navigation-only") == 0)
    {
        if (!xiui::config_input_test::EmptyCategories() || !xiui::config_input_test::Navigation() || !xiui::config_input_test::SecondaryMenus() || !xiui::config_input_test::ChatExtrasMenus()) return 1;
        std::puts("Populated config categories, compacted input, selection fallback and module navigation checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--color-picker-only") == 0)
    {
        if (!xiui::config_input_test::ColorPicker()) return 1;
        std::puts("Color wheel conversion, interaction, opacity, restoration and persistence checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--resource-target-features-only") == 0)
    {
        const std::pair<const char*, bool(*)()> checks[]{
            {"Player resources", &RunPlayerResourceChecks}, {"MP cost preview", &RunMpCostPreviewChecks},
            {"Target child views", &xiui::RunTargetChildChecks}, {"Target child movement", &xiui::config_input_test::TargetChildren},
            {"Target configuration", &xiui::config_input_test::TargetOfTargetSettings}};
        for (const auto& [name, run] : checks)
        {
            std::printf("Checking %s...\n", name); std::fflush(stdout);
            if (!run()) { std::fprintf(stderr, "%s check failed.\n", name); return 1; }
        }
        std::puts("P16/P17/P19/P20 resource, MP preview, target child data/layout and shared input checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--minimap-only") == 0) return RunMinimapChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--skillchain-only") == 0) return RunSkillchainChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--phoenix-only") == 0) return RunPhoenixIntegrationChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--treasure-input-only") == 0)
    {
        if (!xiui::config_input_test::TreasureInput()) return 1;
        std::puts("Treasure input availability and owned key release checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--player-resources-only") == 0) return RunPlayerResourceChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--mp-preview-only") == 0) return RunMpCostPreviewChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--new-features-only") == 0)
    {
        if (!xiui::RunTargetDisplayChecks() || !xiui::RunReadyCheckSoundChecks() || !RunBlueMagicChecks() ||
            !RunPhantomRollChecks() || !RunIconThemeChecks() || !xiui::RunMobInfoChecks() || !xiui::RunPetHudChecks()) return 1;
        std::puts("Target, Ready Check sound, Blue Magic, Phantom Roll, icon themes, Mob Info and Pet HUD checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--icon-themes-only") == 0) return RunIconThemeChecks() ? 0 : 1;
    if (argc == 2 && std::strcmp(argv[1], "--mob-info-only") == 0)
    {
        if (!xiui::RunMobInfoChecks() || !xiui::RunTargetDisplayChecks()) return 1;
        std::puts("Integrated Target mob information, lookup, legacy settings and layout checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-name-cast-only") == 0)
    {
        return xiui::config_input_test::PartyNameCast() ? 0 : 1;
    }
    if ((argc == 2 || argc == 3) && std::strcmp(argv[1], "--settings-presentation-only") == 0)
    {
        return xiui::config_input_test::SettingsPresentation(argc == 3 ? argv[2] : nullptr) ? 0 : 1;
    }
    if (argc == 3 && std::strcmp(argv[1], "--defaults-only") == 0)
    {
        if (!xiui::config_input_test::DefaultProfile(argv[2])) return 1;
        std::puts("Embedded defaults, fresh installs, existing settings and reset checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--status-snapshots-only") == 0)
    {
        if (!xiui::config_input_test::StatusSnapshots()) return 1;
        std::puts("Local and party zoning snapshots, genuine KO, initialized empty lists and extended status IDs passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--party-only") == 0)
    {
        if (!xiui::config_input_test::Party() || !xiui::config_input_test::Navigation()) return 1;
        std::puts("Party data, statuses, layout, independent settings and navigation checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--type7-edge-only") == 0)
    {
        if (!xiui::config_input_test::WindowFrame(argv[2])) return 1;
        std::puts("Type 7 right-edge correction, original artwork and unrelated/custom theme preservation passed.");
        return 0;
    }
    if (argc >= 4 && std::strcmp(argv[1], "--window-borders-only") == 0)
    {
        if (!xiui::config_input_test::NativeWindowBorders(argv[2], argv[3], argc > 4 ? argv[4] : nullptr)) return 1;
        std::puts("Type 6/7 window vertex colors, native border alpha and original edge blend/artwork checks passed.");
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--theme-ui") == 0)
    {
        if (!xiui::config_input_test::DarkBacking() || !xiui::config_input_test::ThemedButtons() || !xiui::config_input_test::WindowFrame(argv[2])) return 1;
        std::puts("Backing opacity, themed buttons and native Type 7 border checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--backing-only") == 0)
    {
        if (!xiui::config_input_test::DarkBacking()) return 1;
        std::puts("Dark backing opacity input and persistence checks passed.");
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--bars-only") == 0)
    {
        if (!meter_cases(argc > 2 ? argv[2] : nullptr) || !meter_text_cases()) return 1;
        std::puts("Bar styles, native gauge artwork, settings and text layout checks passed.");
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--config-ui") == 0)
    {
        if (!xiui::config_input_test::Presentation(argc > 2 ? argv[2] : nullptr) || !xiui::config_input_test::Navigation() || !xiui::config_input_test::Run()) return 1;
        std::puts("Config layout, text effects, sidebar icons, native buttons and click checks passed.");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--layout-only") == 0)
    {
        if (!enemy_cases() || !meter_text_cases()) return 1;
        std::printf("Enemy header and EXP meter text layout checks passed.\n");
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--drag-only") == 0)
    {
        if (!xiui::config_input_test::DragPersistence()) return 1;
        std::printf("Module drop and config layout persistence checks passed.\n");
        return 0;
    }
    if (argc != 1)
    {
        std::fprintf(stderr, "Unknown check or arguments: %s\n", argv[1]);
        return 2;
    }
    if (!xiui::config_input_test::DragPersistence()) return 1;
    if (!xiui::config_input_test::Run() || !enemy_cases() || !meter_text_cases()) return 1;
    const auto path = std::filesystem::temp_directory_path() / ("xiuinative-toggle-check-" + std::to_string(::GetCurrentProcessId()) + ".ini");
    struct cleanup { std::filesystem::path path; ~cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanupFile{path};
    const xiui::settings_store store{path.string()};
    if (!text_profile_cases(store)) return 1;
    const auto configure = [](auto&, int) {};
    size_t checked{};
    if (!persistence<xiui::player_module>(store, 12, [](auto& module, int context) {
            module.Settings().style_edit_mode = 1;
            module.Settings().selected_bar = context % 3;
            module.Settings().selected_text = context / 3;
        }, checked) ||
        !persistence<xiui::target_module>(store, 4, [](auto& module, int context) { *module.Option(4).choice = context; }, checked) ||
        !persistence<xiui::experience_module>(store, 4, [](auto& module, int context) {
            *module.Option(4).choice = context % 2;
            module.Settings().info_position = context / 2;
        }, checked) ||
        !persistence<xiui::enemy_list_module>(store, 4, [](auto& module, int context) { *module.Option(6).choice = context; }, checked) ||
        !persistence<xiui::notification_module>(store, 1, configure, checked) ||
        !persistence<xiui::game_hud_module>(store, 1, configure, checked)) return 1;
    std::printf("Config click regressions and %zu toggle persistence checks passed.\n", checked);
    return 0;
}
