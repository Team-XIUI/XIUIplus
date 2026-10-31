#include "config_input_fixture.hpp"

namespace xiui::config_checks
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
            for (float factor : {.125f,.75f,1.5f,2.0f,6.0f})
            {
                ui::text_style style; style.font = family; style.scale = 12 / ui::text_base_pixels;
                const auto transform = ui::hud_transform::At(17.25f, 23.5f, factor, factor);
                const auto scope = design.Transform(transform);
                const int pixels = ui::ScaledTextPixelSize(style.scale, factor);
                const auto metrics = rasterizer.Metrics(family, pixels);
                if (!check(std::abs(design.MeasureText("WWW", style) * factor - rasterizer.Advance(family, pixels, 'W') * 3) < .001f &&
                    std::abs(design.TextHeight(style) * factor - metrics.height) < .001f,
                    "HUD text measures the final native font size for shrinking and enlarging")) return false;
                const auto glyph = rasterizer.Rasterize(family, pixels, 'W');
                if (!check(glyph && glyph->ascent == metrics.ascent && glyph->height > metrics.height &&
                    std::all_of(glyph->masks.begin(), glyph->masks.end(), [&](const auto& mask) { return mask.size() == glyph->width * glyph->height; }),
                    "base, outline and glow share final-size native glyph geometry")) return false;
            }
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
        if (!check(full.frame[0].width == 8 && full.frame[2].x == 202 && full.content.x == 19 && full.content.y == 20 &&
            full.content.width == 182 && full.content.height == 16 && narrow.content.width == 0 && empty.content.width == 0 &&
            full.frame[1].x - (full.frame[0].x + full.frame[0].width) == 1 &&
            full.frame[2].x - (full.frame[1].x + full.frame[1].width) == 1,
            "native caps retain side clearance while color fills the entire bar height")) return false;
        if (!check(uncapped.frame[0].width == 0 && uncapped.frame[2].width == 0 && uncapped.frame[1].x == 10 &&
            uncapped.frame[1].width == 200 && uncapped.content.x == 10 && uncapped.content.width == 200 &&
            uncapped.content.y == 20 && uncapped.content.height == 16 && thin.content.width == 1 && thin.content.height == 1,
            "uncapped themed bars fill their complete bounds without frame strips")) return false;
        for (float height : {1.0f,16.0f,40.0f,80.0f})
        {
            const auto body = ui::LayoutMeterSkin({10,20,200,height},skin,false).content;
            const auto filled = ui::LayoutMeterFill(body,.5f);
            const auto overlay = ui::LayoutMeterRange(body,{.2f,.4f,0xFFFFFFFF,0xFFFFFFFF},.5f);
            if (!check(body.y == 20 && filled.y == body.y && filled.height == height && overlay.y == body.y && overlay.height == height,
                "short and tall themed fills and overlays retain full vertical coverage")) return false;
        }

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
        if (!check(themed.label.x == 25 && themed.value.x + themed.value.width == 193 && themed.label.y == 12 && themed.value.y == 20 &&
            themed.placement.x == 21 && themed.placement.width == 178,
            "themed labels straddle the top edge while labels and values stay inset beyond the caps")) return false;
        const auto uncapped = xiui::ui::LayoutMeterText(themedBar, 16, 16, 32,
            xiui::ui::meter_text_anchor::automatic, xiui::ui::meter_text_anchor::automatic, &skin, false);
        if (!check(uncapped.placement.x == 10 && uncapped.placement.width == 200 && uncapped.label.x == 14 &&
            uncapped.value.x + uncapped.value.width == 204 && uncapped.label.y == themed.label.y,
            "uncapped labels and values use the full bar with normal text padding and top-edge placement")) return false;
        const auto tall = xiui::ui::LayoutMeterText({10, 20, 200, 40}, 16, 16, 32,
            xiui::ui::meter_text_anchor::top, xiui::ui::meter_text_anchor::bottom, &skin);
        if (!check(tall.label.x == 35 && tall.value.x + tall.value.width == 183 && tall.label.y == 12 && tall.value.y == 52,
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
            target.value.x + target.value.width == 204, "target HP and percent share the lower row with a six-pixel right inset")) return false;
        const auto above = xiui::ui::LayoutMeterText(bar, 16, 16, 32, xiui::ui::meter_text_anchor::top,
            xiui::ui::meter_text_anchor::top);
        const auto below = xiui::ui::LayoutMeterText(bar, 16, 16, 32, xiui::ui::meter_text_anchor::top,
            xiui::ui::meter_text_anchor::bottom);
        return check(above.value.x == 14 && above.value.x + above.value.width == 204 &&
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

}
