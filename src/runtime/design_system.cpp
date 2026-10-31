#include "design_system.hpp"
#include "ui/meter_skin.hpp"
#include "ui/text_metrics.hpp"
#include "ui/clipped_quad.hpp"
#include "ui/rounded_rect.hpp"
#include "ui/contour.hpp"
#include "utf8.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr DWORD shape_vertex_format = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;
    constexpr DWORD text_vertex_format = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    struct shape_vertex
    {
        float x;
        float y;
        float z;
        float rhw;
        D3DCOLOR color;
    };

    using text_vertex = xiui::ui::textured_vertex;

    constexpr DWORD font_pixel_shader[] = {
        0xFFFF0101u,
        0x00000042u,
        0xB00F0000u,
        0x00000005u,
        0x800F0000u,
        0xB0E40000u,
        0x90E40000u,
        0x0000FFFFu,
    };

    auto alpha_of(const D3DCOLOR color) -> uint32_t
    {
        return (color >> 24) & 0xFFu;
    }

    class ini_document
    {
    public:
        explicit ini_document(const std::string_view content)
        {
            std::string section;
            size_t offset{};
            while (offset <= content.size())
            {
                const size_t end = content.find('\n', offset);
                std::string line = Trim(content.substr(offset, end == std::string_view::npos ? content.size() - offset : end - offset));
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (!line.empty() && line[0] != ';' && line[0] != '#')
                {
                    if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
                        section = Lower(Trim(std::string_view{line}.substr(1, line.size() - 2)));
                    else if (const size_t separator = line.find('='); separator != std::string::npos)
                    {
                        const std::string key = Lower(Trim(std::string_view{line}.substr(0, separator)));
                        const std::string value = Trim(std::string_view{line}.substr(separator + 1));
                        this->values_[section + '\n' + key] = value;
                    }
                }
                if (end == std::string_view::npos)
                    break;
                offset = end + 1;
            }
        }

        auto Get(const char* section, const char* key) const -> const std::string*
        {
            const auto found = this->values_.find(Lower(section) + '\n' + Lower(key));
            return found != this->values_.end() ? &found->second : nullptr;
        }

    private:
        std::unordered_map<std::string, std::string> values_{};

        static auto Trim(const std::string_view value) -> std::string
        {
            size_t first{};
            while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])) != 0)
                ++first;
            size_t last = value.size();
            while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1])) != 0)
                --last;
            return std::string{value.substr(first, last - first)};
        }

    public:
        static auto Lower(const std::string_view value) -> std::string
        {
            std::string result{value};
            std::transform(result.begin(), result.end(), result.begin(), [](const unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            return result;
        }
    };

    auto read_float(const ini_document& document, const char* section, const char* key, const float fallback) -> float
    {
        const std::string* value = document.Get(section, key);
        if (value == nullptr)
            return fallback;
        char* end{};
        const float parsed = std::strtof(value->c_str(), &end);
        return end != value->c_str() && *end == '\0' && std::isfinite(parsed) ? parsed : fallback;
    }

    auto read_int(const ini_document& document, const char* section, const char* key, const int fallback) -> int
    {
        const std::string* value = document.Get(section, key);
        if (value == nullptr)
            return fallback;
        char* end{};
        const long parsed = std::strtol(value->c_str(), &end, 10);
        return end != value->c_str() && *end == '\0' ? static_cast<int>(parsed) : fallback;
    }

    auto read_bool(const ini_document& document, const char* section, const char* key, const bool fallback) -> bool
    {
        const std::string* value = document.Get(section, key);
        if (value == nullptr)
            return fallback;
        const std::string normalized = ini_document::Lower(*value);
        if (normalized == "true" || normalized == "yes" || normalized == "on")
            return true;
        if (normalized == "false" || normalized == "no" || normalized == "off")
            return false;
        return read_int(document, section, key, fallback ? 1 : 0) != 0;
    }

    auto read_color(const ini_document& document, const char* section, const char* key, const D3DCOLOR fallback) -> D3DCOLOR
    {
        const std::string* value = document.Get(section, key);
        if (value == nullptr)
            return fallback;
        const char* start = !value->empty() && value->front() == '#' ? value->c_str() + 1 : value->c_str();
        char* end{};
        const auto parsed = std::strtoul(start, &end, 16);
        if (end == start || *end != '\0')
            return fallback;
        return std::strlen(start) <= 6
            ? (fallback & 0xFF000000u) | (parsed & 0x00FFFFFFu)
            : static_cast<D3DCOLOR>(parsed);
    }

    auto load_text_style(const ini_document& document, const char* section, xiui::ui::text_style& style) -> void
    {
        style.scale = std::clamp(read_float(document, section, "scale", style.scale), 0.5f, 3.0f);
        style.letter_spacing = std::clamp(read_float(document, section, "letter_spacing", style.letter_spacing), -4.0f, 20.0f);
        style.top_color = read_color(document, section, "top_color", style.top_color);
        style.bottom_color = read_color(document, section, "bottom_color", style.bottom_color);
        style.alignment = static_cast<xiui::ui::text_alignment>(std::clamp(read_int(document, section, "alignment", static_cast<int>(style.alignment)), 0, 2));
        style.shadow.enabled = read_bool(document, section, "shadow_enabled", style.shadow.enabled);
        style.shadow.offset_x = std::clamp(read_float(document, section, "shadow_x", style.shadow.offset_x), -20.0f, 20.0f);
        style.shadow.offset_y = std::clamp(read_float(document, section, "shadow_y", style.shadow.offset_y), -20.0f, 20.0f);
        style.shadow.color = read_color(document, section, "shadow_color", style.shadow.color);
        style.shadow.soft = read_bool(document, section, "shadow_soft", style.shadow.soft);
        style.stroke.enabled = read_bool(document, section, "stroke_enabled", style.stroke.enabled);
        style.stroke.color = read_color(document, section, "stroke_color", style.stroke.color);
        style.glow.enabled = read_bool(document, section, "glow_enabled", style.glow.enabled);
        style.glow.color = read_color(document, section, "glow_color", style.glow.color);
        style.sheen.enabled = read_bool(document, section, "sheen_enabled", style.sheen.enabled);
        style.sheen.color = read_color(document, section, "sheen_color", style.sheen.color);
        style.sheen.width = std::clamp(read_float(document, section, "sheen_width", style.sheen.width), 1.0f, 400.0f);
        style.sheen.speed = std::clamp(read_float(document, section, "sheen_speed", style.sheen.speed), 0.0f, 10.0f);
    }
}

namespace xiui::ui
{
    auto theme::CreateDefault(void) -> theme
    {
        theme result{};
        result.colors.text = D3DCOLOR_ARGB(255, 255, 253, 248);
        result.colors.muted = 0xFFB7C1D8;
        result.colors.outline = D3DCOLOR_ARGB(235, 0, 0, 0);
        result.colors.chrome = D3DCOLOR_ARGB(245, 13, 13, 13);
        result.colors.surface = D3DCOLOR_ARGB(235, 13, 13, 13);
        result.colors.surface_raised = 0xF81C2439;
        result.colors.border = 0xFF7381A5;
        result.colors.selected = 0x604F659A;
        result.colors.accent = 0xFFCAD6EE;
        result.colors.accent_dim = 0xFF879ABD;
        result.colors.control = 0xFF8C9BE3;
        result.colors.control_selected = 0xFF565D77;
        result.colors.gold = D3DCOLOR_ARGB(255, 244, 218, 151);
        result.colors.gold_dark = D3DCOLOR_ARGB(255, 195, 174, 121);
        result.colors.meter_track = D3DCOLOR_ARGB(225, 5, 5, 5);

        result.hud_panel = {result.colors.surface, (result.colors.border & 0xFFFFFF) | 0xD2000000, D3DCOLOR_ARGB(130, 0, 0, 0), 2.0f, 1.0f};
        result.config_panel = {D3DCOLOR_ARGB(250, 13, 13, 13), result.colors.border, D3DCOLOR_ARGB(180, 0, 0, 0), 3.0f, 1.0f};
        result.meter.track_top = D3DCOLOR_ARGB(255, 11, 18, 29);
        result.meter.track_bottom = D3DCOLOR_ARGB(255, 5, 5, 5);
        result.meter.border = D3DCOLOR_ARGB(255, 0, 0, 0);
        result.meter.bookend_top = D3DCOLOR_ARGB(255, 45, 74, 124);
        result.meter.bookend_bottom = D3DCOLOR_ARGB(255, 18, 32, 57);
        result.meter.shadow = D3DCOLOR_ARGB(255, 0, 0, 0);
        result.meter.glow = D3DCOLOR_ARGB(74, 244, 218, 151);
        result.meter.sheen = D3DCOLOR_ARGB(54, 255, 255, 255);

        result.body_text.top_color = result.colors.text;
        result.body_text.bottom_color = 0xFFD0D8EB;

        result.title_text = result.body_text;
        result.title_text.scale = 1.05f;
        result.title_text.stroke.enabled = true;
        result.title_text.stroke.color = D3DCOLOR_ARGB(245, 0, 0, 0);

        result.caption_text = result.body_text;
        result.caption_text.scale = 0.78f;
        result.caption_text.top_color = result.colors.muted;
        result.caption_text.bottom_color = 0xFF94A2BF;

        result.meter_text = result.caption_text;
        result.meter_text.top_color = result.colors.text;
        result.meter_text.bottom_color = D3DCOLOR_ARGB(255, 218, 225, 235);
        result.meter_text.stroke.enabled = true;
        result.meter_text.stroke.color = D3DCOLOR_ARGB(235, 0, 0, 0);

        result.accent_text = result.body_text;
        result.accent_text.top_color = result.colors.accent;
        result.accent_text.bottom_color = result.colors.accent_dim;
        result.accent_text.stroke.enabled = true;
        result.accent_text.glow.enabled = true;
        result.accent_text.glow.color = 0x41CAD6EE;
        return result;
    }

    design_system::design_system(void) = default;

    design_system::~design_system(void)
    {
        this->Shutdown();
    }

    auto design_system::SetDevice(IDirect3DDevice8* device) -> bool
    {
        if (this->device_ == device && this->FontAvailable(0))
            return true;

        this->ReleaseResources();
        this->device_ = device;
        if (this->device_ != nullptr && this->device_->TestCooperativeLevel() == D3D_OK)
        {
            for (int font = 0; font < font_count; ++font) this->glyphs_.Metrics(font, 15);
            this->CreateDotTexture();
            this->icons_.SetDevice(device);
            this->CreatePixelShader();
        }
        return this->device_ == nullptr || this->FontAvailable(0);
    }

    auto design_system::SetThemePath(const char* path) -> void
    {
        this->theme_path_ = path != nullptr ? path : "";
        this->configured_theme_ = this->theme_ = theme::CreateDefault();
        this->ApplyWindowPalette();
        this->theme_stamp_ = {};
        this->pending_theme_stamp_ = {};
        this->has_theme_stamp_ = false;
        this->theme_change_pending_ = false;
        this->next_theme_poll_time_ = 0;
        this->ReloadTheme();
    }

    auto design_system::ReloadTheme(void) -> bool
    {
        theme_file_stamp stamp{};
        if (!this->ReadThemeStamp(stamp))
            return false;
        this->theme_change_pending_ = false;
        return this->LoadThemeFile(stamp);
    }

    auto design_system::Shutdown(void) -> void
    {
        if (this->frame_active_)
            this->EndFrame();
        this->ReleaseResources();
        this->device_ = nullptr;
        this->theme_path_.clear();
        this->theme_stamp_ = {};
        this->pending_theme_stamp_ = {};
        this->has_theme_stamp_ = false;
        this->theme_change_pending_ = false;
        this->next_theme_poll_time_ = 0;
    }

    auto design_system::BeginFrame(void) -> bool
    {
        if (this->device_ == nullptr || this->device_->TestCooperativeLevel() != D3D_OK)
            return false;
        this->PollTheme();
        if (this->frame_active_)
            this->EndFrame();
        if (!this->FontAvailable(0))
            return false;
        if (!this->shader_attempted_)
            this->CreatePixelShader();
        if (FAILED(this->device_->CreateStateBlock(D3DSBT_ALL, &this->state_block_)))
            return false;

        this->frame_active_ = true;
        // HUD draws share these states until EndFrame restores the captured game state.
        this->ConfigureCommonPipeline();
        this->ConfigureShapePipeline();
        return true;
    }

    auto design_system::EndFrame(void) -> void
    {
        this->tracking_content_bounds_ = false;
        this->transform_ = {}; this->clip_bounds_.reset();
        if (!this->frame_active_ || this->device_ == nullptr)
            return;

        this->device_->SetTexture(0, nullptr);
        this->device_->SetPixelShader(0);
        this->device_->ApplyStateBlock(this->state_block_);
        this->device_->DeleteStateBlock(this->state_block_);
        this->state_block_ = 0;
        this->frame_active_ = false;
    }

    auto design_system::GetTheme(void) const -> const theme&
    {
        return this->theme_;
    }

    auto design_system::IsReady(void) const -> bool
    {
        return this->device_ == nullptr || this->FontAvailable(0);
    }

    auto design_system::SetGpuShaderEnabled(const bool enabled) -> void
    {
        if (this->gpu_shader_enabled_ == enabled)
            return;

        this->gpu_shader_enabled_ = enabled;
        if (this->device_ != nullptr && this->pixel_shader_ != 0)
            this->device_->DeletePixelShader(this->pixel_shader_);
        this->pixel_shader_ = 0;
        this->shader_attempted_ = false;
        if (this->device_ != nullptr)
            this->CreatePixelShader();
    }

    auto design_system::IsGpuShaderActive(void) const -> bool
    {
        return this->pixel_shader_ != 0;
    }

    auto design_system::PollTheme(void) -> bool
    {
        if (this->theme_path_.empty())
            return false;
        const uint64_t now = ::GetTickCount64();
        if (now < this->next_theme_poll_time_)
            return false;
        this->next_theme_poll_time_ = now + 250;

        theme_file_stamp current{};
        if (!this->ReadThemeStamp(current))
            return false;
        if (this->has_theme_stamp_ && current == this->theme_stamp_)
        {
            this->theme_change_pending_ = false;
            return false;
        }
        if (this->theme_change_pending_ && current == this->pending_theme_stamp_)
        {
            const bool loaded = this->LoadThemeFile(current);
            this->theme_change_pending_ = false;
            return loaded;
        }
        this->pending_theme_stamp_ = current;
        this->theme_change_pending_ = true;
        return false;
    }

    auto design_system::LoadThemeFile(const theme_file_stamp& stamp) -> bool
    {
        if (this->theme_path_.empty())
            return false;

        std::string content;
        if (!this->ReadThemeSnapshot(stamp, content))
            return false;
        const ini_document document{content};
        theme loaded = theme::CreateDefault();
        loaded.colors.text = read_color(document, "colors", "text", loaded.colors.text);
        loaded.colors.muted = read_color(document, "colors", "muted", loaded.colors.muted);
        loaded.colors.outline = read_color(document, "colors", "outline", loaded.colors.outline);
        loaded.colors.chrome = read_color(document, "colors", "chrome", loaded.colors.chrome);
        loaded.colors.surface = read_color(document, "colors", "surface", loaded.colors.surface);
        loaded.colors.surface_raised = read_color(document, "colors", "surface_raised", loaded.colors.surface_raised);
        loaded.colors.border = read_color(document, "colors", "border", loaded.colors.border);
        loaded.colors.selected = read_color(document, "colors", "selected", loaded.colors.selected);
        loaded.colors.gold = read_color(document, "colors", "gold", loaded.colors.gold);
        loaded.colors.gold_dark = read_color(document, "colors", "gold_dark", loaded.colors.gold_dark);
        loaded.colors.accent = read_color(document, "colors", "accent", loaded.colors.gold == 0xFFF4DA97 ? loaded.colors.accent : loaded.colors.gold);
        loaded.colors.accent_dim = read_color(document, "colors", "accent_dim", loaded.colors.gold_dark == 0xFFC3AE79 ? loaded.colors.accent_dim : loaded.colors.gold_dark);
        loaded.colors.control = read_color(document, "colors", "control", loaded.colors.control);
        loaded.colors.control_selected = read_color(document, "colors", "control_selected", loaded.colors.control_selected);
        loaded.colors.meter_track = read_color(document, "colors", "meter_track", loaded.colors.meter_track);

        loaded.spacing.xxs = std::clamp(read_float(document, "spacing", "xxs", loaded.spacing.xxs), 0.0f, 100.0f);
        loaded.spacing.xs = std::clamp(read_float(document, "spacing", "xs", loaded.spacing.xs), 0.0f, 100.0f);
        loaded.spacing.sm = std::clamp(read_float(document, "spacing", "sm", loaded.spacing.sm), 0.0f, 100.0f);
        loaded.spacing.md = std::clamp(read_float(document, "spacing", "md", loaded.spacing.md), 0.0f, 100.0f);
        loaded.spacing.lg = std::clamp(read_float(document, "spacing", "lg", loaded.spacing.lg), 0.0f, 100.0f);
        loaded.spacing.xl = std::clamp(read_float(document, "spacing", "xl", loaded.spacing.xl), 0.0f, 100.0f);

        const auto loadPanel = [&document](const char* section, panel_style& panel) {
            panel.background = read_color(document, section, "background", panel.background);
            panel.border = read_color(document, section, "border", panel.border);
            panel.shadow = read_color(document, section, "shadow", panel.shadow);
            panel.shadow_offset = std::clamp(read_float(document, section, "shadow_offset", panel.shadow_offset), 0.0f, 30.0f);
            panel.border_width = std::clamp(read_float(document, section, "border_width", panel.border_width), 0.0f, 10.0f);
        };
        loadPanel("panel.hud", loaded.hud_panel);
        loadPanel("panel.config", loaded.config_panel);

        const int meterStyleVersion = static_cast<int>(read_float(document, "meter", "style_version", 0.0f));
        loaded.meter.appearance = static_cast<int>(std::clamp(read_float(document, "meter", "appearance", static_cast<float>(loaded.meter.appearance)), 0.0f, 1.0f));
        const D3DCOLOR legacyTrack = read_color(document, "meter", "track", loaded.meter.track_bottom);
        loaded.meter.track_top = read_color(document, "meter", "track_top", legacyTrack);
        loaded.meter.track_bottom = read_color(document, "meter", "track_bottom", legacyTrack);
        loaded.meter.border = read_color(document, "meter", "border", loaded.meter.border);
        loaded.meter.bookend_top = read_color(document, "meter", "bookend_top", loaded.meter.bookend_top);
        loaded.meter.bookend_bottom = read_color(document, "meter", "bookend_bottom", loaded.meter.bookend_bottom);
        loaded.meter.shadow = read_color(document, "meter", "shadow", loaded.meter.shadow);
        loaded.meter.glow = read_color(document, "meter", "glow", loaded.meter.glow);
        loaded.meter.sheen = read_color(document, "meter", "sheen", loaded.meter.sheen);
        loaded.meter.height = std::clamp(read_float(document, "meter", "height", loaded.meter.height), 0.0f, 80.0f);
        loaded.meter.border_width = std::clamp(read_float(document, "meter", "border_width", loaded.meter.border_width), 0.0f, 8.0f);
        loaded.meter.corner_radius = std::clamp(read_float(document, "meter", "corner_radius", loaded.meter.corner_radius), 0.0f, 100.0f);
        loaded.meter.bookend_width = std::clamp(read_float(document, "meter", "bookend_width", loaded.meter.bookend_width), 0.0f, 100.0f);
        loaded.meter.track_opacity = std::clamp(read_float(document, "meter", "track_opacity", loaded.meter.track_opacity), 0.0f, 1.0f);
        loaded.meter.shadow_offset_x = std::clamp(read_float(document, "meter", "shadow_offset_x", loaded.meter.shadow_offset_x), -20.0f, 20.0f);
        loaded.meter.shadow_offset_y = std::clamp(read_float(document, "meter", "shadow_offset_y", loaded.meter.shadow_offset_y), -20.0f, 20.0f);
        loaded.meter.shadow_blur = std::clamp(read_float(document, "meter", "shadow_blur", loaded.meter.shadow_blur), 0.0f, 24.0f);
        loaded.meter.shadow_strength = std::clamp(read_float(document, "meter", "shadow_strength", loaded.meter.shadow_strength), 0.0f, 1.0f);
        loaded.meter.glow_size = std::clamp(read_float(document, "meter", "glow_size", loaded.meter.glow_size), 0.0f, 20.0f);
        loaded.meter.fill_glow_radius = std::clamp(read_float(document, "meter", "fill_glow_radius", loaded.meter.fill_glow_radius), 0.0f, 20.0f);
        loaded.meter.fill_glow_strength = std::clamp(read_float(document, "meter", "fill_glow_strength", loaded.meter.fill_glow_strength), 0.0f, 1.0f);
        loaded.meter.sheen_width = std::clamp(read_float(document, "meter", "sheen_width", loaded.meter.sheen_width), 1.0f, 400.0f);
        loaded.meter.sheen_speed = std::clamp(read_float(document, "meter", "sheen_speed", loaded.meter.sheen_speed), 0.0f, 5.0f);
        loaded.meter.animation_speed = std::clamp(read_float(document, "meter", "animation_speed", loaded.meter.animation_speed), 0.1f, 30.0f);
        loaded.meter.bookends = read_bool(document, "meter", "bookends", loaded.meter.bookends);
        loaded.meter.themed_bookends = read_bool(document, "meter", "themed_bookends", loaded.meter.themed_bookends);
        loaded.meter.shadow_enabled = read_bool(document, "meter", "shadow_enabled", loaded.meter.shadow_enabled);
        loaded.meter.glow_enabled = read_bool(document, "meter", "glow_enabled", loaded.meter.glow_enabled);
        loaded.meter.fill_glow_enabled = read_bool(document, "meter", "fill_glow_enabled", loaded.meter.fill_glow_enabled);
        loaded.meter.sheen_enabled = read_bool(document, "meter", "sheen_enabled", loaded.meter.sheen_enabled);
        loaded.meter.animation_enabled = read_bool(document, "meter", "animation_enabled", loaded.meter.animation_enabled);

        if (meterStyleVersion < 4 && loaded.meter.border == D3DCOLOR_ARGB(255, 77, 70, 60))
            loaded.meter.border = D3DCOLOR_ARGB(255, 0, 0, 0);

        if (meterStyleVersion < 3)
        {
            const float topAlpha = static_cast<float>((loaded.meter.track_top >> 24) & 0xFF) / 255.0f;
            const float bottomAlpha = static_cast<float>((loaded.meter.track_bottom >> 24) & 0xFF) / 255.0f;
            loaded.meter.track_opacity = (topAlpha + bottomAlpha) * 0.5f;
            loaded.meter.track_top |= 0xFF000000u;
            loaded.meter.track_bottom |= 0xFF000000u;
            if (std::abs(loaded.meter.corner_radius) < 0.001f)
                loaded.meter.corner_radius = 8.0f;
            if (std::abs(loaded.meter.border_width - 1.0f) < 0.001f)
                loaded.meter.border_width = 2.0f;
            loaded.meter.shadow_enabled = true;
            if (std::abs(loaded.meter.shadow_strength - 0.55f) < 0.001f)
                loaded.meter.shadow_strength = 0.2f;
            if (std::abs(loaded.meter.shadow_offset_x - 2.0f) < 0.001f)
                loaded.meter.shadow_offset_x = 0.0f;
            if (std::abs(loaded.meter.shadow_offset_y - 2.0f) < 0.001f)
                loaded.meter.shadow_offset_y = 1.0f;
            if (std::abs(loaded.meter.shadow_blur - 2.0f) < 0.001f)
                loaded.meter.shadow_blur = 4.0f;
        }

        load_text_style(document, "text.body", loaded.body_text);
        load_text_style(document, "text.title", loaded.title_text);
        load_text_style(document, "text.caption", loaded.caption_text);
        load_text_style(document, "text.meter", loaded.meter_text);
        load_text_style(document, "text.accent", loaded.accent_text);
        if (read_int(document, "theme", "style_version", 0) < 2)
        {
            const auto migrate = [](D3DCOLOR& value, const D3DCOLOR old, const D3DCOLOR replacement) { if (value == old) value = replacement; };
            migrate(loaded.colors.selected, 0x40F4DA97, 0x604F659A);
            migrate(loaded.colors.border, 0xFF4D463C, 0xFF7381A5);
            migrate(loaded.colors.muted, 0xFF999388, 0xFFB7C1D8);
            migrate(loaded.colors.surface_raised, 0xF8191713, 0xF81C2439);
            migrate(loaded.config_panel.border, 0xFF4D463C, loaded.colors.border);
            migrate(loaded.hud_panel.border, 0xD24D463C, 0xD27381A5);
            migrate(loaded.accent_text.top_color, 0xFFFFF0BE, loaded.colors.accent);
            migrate(loaded.accent_text.bottom_color, 0xFFC3AE79, loaded.colors.accent_dim);
            migrate(loaded.accent_text.glow.color, 0x41F4DA97, 0x41CAD6EE);
            migrate(loaded.caption_text.top_color, 0xFF999388, loaded.colors.muted);
            migrate(loaded.caption_text.bottom_color, 0xFF807B72, 0xFF94A2BF);
            migrate(loaded.body_text.bottom_color, 0xFFE0DACF, 0xFFD0D8EB);
            migrate(loaded.title_text.bottom_color, 0xFFE0DACF, 0xFFD0D8EB);
        }

        this->configured_theme_ = loaded;
        this->ApplyWindowPalette();
        this->theme_stamp_ = stamp;
        this->has_theme_stamp_ = true;
        return true;
    }

    auto design_system::SetDarkBackingOpacity(const float opacity) -> void
    {
        this->dark_backing_opacity_ = std::clamp(opacity, 0.0f, 1.0f);
    }

    auto design_system::SetAppearanceTheme(const int value) -> void
    {
        const auto theme = AppearanceTheme(value);
        if (this->appearance_theme_ == theme) return;
        this->appearance_theme_ = theme;
        this->ApplyWindowPalette();
    }

    auto design_system::ResolveHudText(const text_style& source) const -> text_style
    {
        auto style = source;
        if (!this->IsXiv1Theme() || !style.theme_text) return style;
        if (style.font != xiv1_font && style.font != xiv1_italic_font && style.font != xiv1_number_font) style.font = xiv1_font;
        const auto alpha = std::max(style.top_color >> 24, style.bottom_color >> 24);
        style.stroke = {true, std::min<D3DCOLOR>({240, alpha, style.stroke.color >> 24}) << 24};
        style.shadow = {true, 1.5f, 1.5f, std::min<D3DCOLOR>({224, alpha, style.shadow.color >> 24}) << 24, false};
        return style;
    }

    auto design_system::SetWindowPalette(const D3DCOLOR background) -> void
    {
        if (background == this->window_background_) return;
        this->window_background_ = background;
        this->ApplyWindowPalette();
    }

    auto design_system::SetControlPalette(const D3DCOLOR base, const D3DCOLOR highlighted) -> void
    {
        this->control_palette_ = {base, highlighted};
        this->ApplyWindowPalette();
    }

    auto design_system::ApplyWindowPalette(void) -> void
    {
        this->theme_ = this->configured_theme_;
        const auto defaults = theme::CreateDefault();
        auto& colors = this->theme_.colors;
        if (this->IsXiv1Theme())
        {
            colors.chrome = 0xFA10132E; colors.surface = 0xEB0B1030; colors.surface_raised = 0xD2222B50;
            colors.text = 0xFFF0EBD6; colors.muted = 0xFFBDB9A6;
            colors.border = 0xFFA49B7F; colors.accent = 0xFFE1D6AD; colors.accent_dim = 0xFF9F9475;
            colors.selected = 0x745F8FBC; colors.control = 0xFFBBB394; colors.control_selected = 0xFFE7DDAE;
            this->theme_.accent_text.top_color = colors.accent;
            this->theme_.accent_text.bottom_color = colors.accent_dim;
            return;
        }
        const auto automatic = [](D3DCOLOR& value, const D3DCOLOR original, const D3DCOLOR replacement) {
            if (value == original) value = replacement;
        };
        if (this->window_background_ == 0 && this->control_palette_[0] != 0)
            automatic(colors.control, defaults.colors.control, this->control_palette_[0]);
        if (this->control_palette_[1] != 0) automatic(colors.control_selected, defaults.colors.control_selected, this->control_palette_[1]);
        if (this->window_background_ == 0) return;
        const auto mix = [this](const unsigned white, const unsigned alpha) {
            D3DCOLOR color = alpha << 24;
            for (unsigned shift = 0; shift < 24; shift += 8)
            {
                const auto channel = (this->window_background_ >> shift) & 255;
                color |= ((channel * (255 - white) + 255 * white) / 255) << shift;
            }
            return color;
        };
        automatic(colors.chrome, defaults.colors.chrome, mix(0, 248));
        automatic(colors.surface, defaults.colors.surface, mix(0, 235));
        automatic(colors.surface_raised, defaults.colors.surface_raised, mix(20, 210));
        automatic(colors.selected, defaults.colors.selected, mix(100, 100));
        automatic(colors.accent, defaults.colors.accent, mix(210, 255));
        automatic(colors.accent_dim, defaults.colors.accent_dim, mix(140, 255));
        automatic(colors.border, defaults.colors.border, mix(155, 255));
        automatic(colors.control, defaults.colors.control, mix(100, 255));
        if (this->theme_.accent_text.top_color == defaults.accent_text.top_color)
            this->theme_.accent_text.top_color = colors.accent;
        if (this->theme_.accent_text.bottom_color == defaults.accent_text.bottom_color)
            this->theme_.accent_text.bottom_color = colors.accent_dim;
    }

    auto design_system::ReadThemeSnapshot(const theme_file_stamp& expected, std::string& content) const -> bool
    {
        content.clear();
        const HANDLE file = ::CreateFileA(
            this->theme_path_.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        BY_HANDLE_FILE_INFORMATION before{};
        LARGE_INTEGER size{};
        if (::GetFileInformationByHandle(file, &before) == FALSE || ::GetFileSizeEx(file, &size) == FALSE || size.QuadPart < 0 || size.QuadPart > 1024 * 1024)
        {
            ::CloseHandle(file);
            return false;
        }

        content.resize(static_cast<size_t>(size.QuadPart));
        DWORD bytesRead{};
        const BOOL readResult = content.empty()
            ? TRUE
            : ::ReadFile(file, content.data(), static_cast<DWORD>(content.size()), &bytesRead, nullptr);
        BY_HANDLE_FILE_INFORMATION after{};
        const BOOL afterResult = ::GetFileInformationByHandle(file, &after);
        ::CloseHandle(file);
        if (readResult == FALSE || afterResult == FALSE || static_cast<size_t>(bytesRead) != content.size())
            return false;

        const auto toStamp = [](const BY_HANDLE_FILE_INFORMATION& information) -> theme_file_stamp {
            ULARGE_INTEGER writeTime{};
            writeTime.HighPart = information.ftLastWriteTime.dwHighDateTime;
            writeTime.LowPart = information.ftLastWriteTime.dwLowDateTime;
            ULARGE_INTEGER fileSize{};
            fileSize.HighPart = information.nFileSizeHigh;
            fileSize.LowPart = information.nFileSizeLow;
            return {writeTime.QuadPart, fileSize.QuadPart};
        };
        const theme_file_stamp beforeStamp = toStamp(before);
        const theme_file_stamp afterStamp = toStamp(after);
        theme_file_stamp pathStamp{};
        return beforeStamp == expected && afterStamp == expected && this->ReadThemeStamp(pathStamp) && pathStamp == expected;
    }

    auto design_system::ReadThemeStamp(theme_file_stamp& stamp) const -> bool
    {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (this->theme_path_.empty() || ::GetFileAttributesExA(this->theme_path_.c_str(), GetFileExInfoStandard, &attributes) == FALSE)
            return false;
        ULARGE_INTEGER writeTime{};
        writeTime.HighPart = attributes.ftLastWriteTime.dwHighDateTime;
        writeTime.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
        ULARGE_INTEGER size{};
        size.HighPart = attributes.nFileSizeHigh;
        size.LowPart = attributes.nFileSizeLow;
        stamp.write_time = writeTime.QuadPart;
        stamp.size = size.QuadPart;
        return true;
    }

    auto design_system::CreatePixelShader(void) -> void
    {
        if (this->device_ == nullptr || this->shader_attempted_)
            return;

        this->shader_attempted_ = true;
        if (!this->gpu_shader_enabled_)
            return;

        D3DCAPS8 caps{};
        if (FAILED(this->device_->GetDeviceCaps(&caps)) || caps.PixelShaderVersion < D3DPS_VERSION(1, 1))
            return;
        if (FAILED(this->device_->CreatePixelShader(font_pixel_shader, &this->pixel_shader_)))
            this->pixel_shader_ = 0;
    }

    auto design_system::CreateDotTexture(void) -> void
    {
        constexpr uint32_t size = 64;
        if (FAILED(this->device_->CreateTexture(size, size, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &this->dot_texture_))) return;
        D3DLOCKED_RECT locked{};
        if (FAILED(this->dot_texture_->LockRect(0, &locked, nullptr, 0)))
        { this->dot_texture_->Release(); this->dot_texture_ = nullptr; return; }
        for (uint32_t y = 0; y < size; ++y)
        for (uint32_t x = 0; x < size; ++x)
        {
            const float dx = (x + 0.5f) / size - 0.5f, dy = (y + 0.5f) / size - 0.5f;
            const float distance = std::sqrt(dx * dx + dy * dy);
            const auto alpha = static_cast<uint32_t>(255 * std::clamp((0.48f - distance) * 16, 0.0f, 1.0f));
            const float rim = std::clamp((0.45f - distance) * 30, 0.0f, 1.0f);
            const auto shade = static_cast<uint32_t>(std::clamp((0.55f + rim * 0.45f) * (0.92f - dy * 0.16f), 0.0f, 1.0f) * 255);
            auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(locked.pBits) + y * locked.Pitch);
            row[x] = (alpha << 24) | (shade << 16) | (shade << 8) | shade;
        }
        this->dot_texture_->UnlockRect(0);
    }

    auto design_system::ReleaseResources(void) -> void
    {
        this->icons_.Shutdown();
        this->glyphs_.Clear();
        if (this->device_ != nullptr && this->pixel_shader_ != 0)
            this->device_->DeletePixelShader(this->pixel_shader_);
        this->pixel_shader_ = 0;
        this->shader_attempted_ = false;

        if (this->dot_texture_ != nullptr) this->dot_texture_->Release();
        this->dot_texture_ = nullptr;
    }

    auto design_system::ConfigureCommonPipeline(void) const -> void
    {
        if (this->device_ == nullptr)
            return;

        this->device_->SetRenderState(D3DRS_ZENABLE, FALSE);
        this->device_->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        this->device_->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        this->device_->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        this->device_->SetRenderState(D3DRS_LIGHTING, FALSE);
        this->device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
        this->device_->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        this->device_->SetRenderState(D3DRS_CLIPPING, TRUE);
        this->device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        this->device_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        this->device_->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
        this->device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        this->device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        this->device_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        this->device_->SetRenderState(
            D3DRS_COLORWRITEENABLE,
            D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        this->device_->SetTextureStageState(0, D3DTSS_RESULTARG, D3DTA_CURRENT);
        this->device_->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        this->device_->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
        this->device_->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
        this->device_->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
        this->device_->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
        this->device_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        this->device_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    }

    auto design_system::ConfigureShapePipeline(void) const -> void
    {
        if (this->device_ == nullptr) return;
        this->device_->SetTexture(0, nullptr);
        this->device_->SetPixelShader(0);
        this->device_->SetVertexShader(shape_vertex_format);
        this->device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        this->device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    }

    auto design_system::ConfigureImagePipeline(IDirect3DTexture8* texture) const -> void
    {
        if (this->device_ == nullptr)
            return;
        this->device_->SetTexture(0, texture);
        this->device_->SetPixelShader(0);
        this->device_->SetVertexShader(text_vertex_format);
        this->device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }

    auto design_system::ConfigureTextPipeline(IDirect3DTexture8* texture, const bool additive) const -> void
    {
        if (this->device_ == nullptr)
            return;

        this->device_->SetTexture(0, texture);
        this->device_->SetVertexShader(text_vertex_format);
        this->device_->SetPixelShader(this->pixel_shader_);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        this->device_->SetRenderState(D3DRS_DESTBLEND, additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    }

    auto design_system::DrawLayoutGrid(const rect& viewport, const float spacing, const float opacity) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || viewport.width <= 0 || viewport.height <= 0 || !std::isfinite(spacing) || spacing < 4 || opacity <= 0)
            return;
        this->ConfigureShapePipeline();
        std::array<shape_vertex, 768> vertices{};
        size_t used{};
        const auto flush = [&] {
            if (used != 0) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(used / 3), vertices.data(), sizeof(shape_vertex));
            used = 0;
        };
        const auto line = [&](const rect& bounds, const int index) {
            const auto color = index % 5 == 0 ? WithAlpha(this->theme_.colors.accent, std::min(1.0f, opacity * 1.5f)) : WithAlpha(this->theme_.colors.accent_dim, opacity);
            const float left = bounds.x - 0.5f, top = bounds.y - 0.5f;
            const float right = left + bounds.width, bottom = top + bounds.height;
            vertices[used++] = {left, top, 0, 1, color}; vertices[used++] = {right, top, 0, 1, color}; vertices[used++] = {left, bottom, 0, 1, color};
            vertices[used++] = {left, bottom, 0, 1, color}; vertices[used++] = {right, top, 0, 1, color}; vertices[used++] = {right, bottom, 0, 1, color};
            if (used == vertices.size()) flush();
        };
        for (int index = 0; index * spacing < viewport.width; ++index)
            line({viewport.x + index * spacing, viewport.y, 1, viewport.height}, index);
        for (int index = 0; index * spacing < viewport.height; ++index)
            line({viewport.x, viewport.y + index * spacing, viewport.width, 1}, index);
        flush();
    }

    auto design_system::FillRect(const rect& bounds, const D3DCOLOR color) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;
        this->ConfigureShapePipeline();

        const float left = bounds.x - 0.5f;
        const float top = bounds.y - 0.5f;
        const float right = bounds.x + bounds.width - 0.5f;
        const float bottom = bounds.y + bounds.height - 0.5f;
        const shape_vertex vertices[] = {
            {left, top, 0.0f, 1.0f, color},
            {right, top, 0.0f, 1.0f, color},
            {left, bottom, 0.0f, 1.0f, color},
            {right, bottom, 0.0f, 1.0f, color},
        };
        this->Submit(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(shape_vertex));
    }

    auto design_system::FillCircles(const std::span<const circle_fill> circles) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || circles.empty()) return;
        static const auto directions = [] {
            std::array<std::array<point, 72>, 11> result{};
            for (size_t level = 0; level < result.size(); ++level)
                for (size_t i = 0; i < (level + 8) * 4; ++i)
                {
                    const float angle = -1.570796327f + i * 6.283185307f / ((level + 8) * 4);
                    result[level][i] = {std::cos(angle), std::sin(angle)};
                }
            return result;
        }();
        this->ConfigureShapePipeline();
        std::array<shape_vertex, 32 * 9 * 16> vertices;
        size_t count{};
        const auto flush = [&] {
            if (count != 0) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(count / 3), vertices.data(), sizeof(shape_vertex));
            count = 0;
        };
        for (const auto& circle : circles)
        {
            if (!std::isfinite(circle.radius) || circle.radius <= 0 || (circle.color >> 24) == 0) continue;
            const size_t level = static_cast<size_t>(std::clamp(std::ceil(circle.radius * 0.8f), 8.0f, 18.0f)) - 8;
            const size_t segments = (level + 8) * 4;
            if (count + segments * 9 > vertices.size()) flush();
            const float innerRadius = circle.radius - std::min(1.0f / std::max(this->transform_.x, this->transform_.y), circle.radius * 0.5f);
            const shape_vertex center{circle.center.x - 0.5f, circle.center.y - 0.5f, 0, 1, circle.color};
            const auto at = [&](const size_t index, const float radius, const D3DCOLOR color) -> shape_vertex {
                const auto& direction = directions[level][index];
                return {center.x + direction.x * radius, center.y + direction.y * radius, 0, 1, color};
            };
            this->TrackContentBounds({circle.center.x - circle.radius, circle.center.y - circle.radius, circle.radius * 2, circle.radius * 2});
            for (size_t i = 0; i < segments; ++i)
            {
                const size_t next = (i + 1) % segments;
                const auto a = at(i, innerRadius, circle.color), b = at(next, innerRadius, circle.color);
                const auto c = at(i, circle.radius, circle.color & 0x00FFFFFF), d = at(next, circle.radius, circle.color & 0x00FFFFFF);
                for (const auto& vertex : {center, a, b, a, c, b, b, c, d}) vertices[count++] = vertex;
            }
        }
        flush();
    }

    auto design_system::DrawTriangle(const float x1, const float y1, const float x2, const float y2, const float x3, const float y3, const D3DCOLOR color) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr) return;
        this->ConfigureShapePipeline();
        const shape_vertex vertices[]{{x1 - 0.5f, y1 - 0.5f, 0, 1, color}, {x2 - 0.5f, y2 - 0.5f, 0, 1, color}, {x3 - 0.5f, y3 - 0.5f, 0, 1, color}};
        this->TrackContentBounds({std::min({x1, x2, x3}), std::min({y1, y2, y3}), std::max({x1, x2, x3}) - std::min({x1, x2, x3}), std::max({y1, y2, y3}) - std::min({y1, y2, y3})});
        this->Submit(D3DPT_TRIANGLELIST, 1, vertices, sizeof(shape_vertex));
    }

    auto design_system::DrawTriangleFan(const point center, const std::span<const point> arc, const D3DCOLOR centerColor, const D3DCOLOR edgeColor) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || arc.size() < 2) return;
        this->ConfigureShapePipeline();
        std::vector<shape_vertex> vertices; vertices.reserve((arc.size() - 1) * 3);
        float left = center.x, right = center.x, top = center.y, bottom = center.y;
        for (size_t i = 0; i < arc.size(); ++i)
        {
            left = std::min(left, arc[i].x); right = std::max(right, arc[i].x);
            top = std::min(top, arc[i].y); bottom = std::max(bottom, arc[i].y);
            if (i == 0) continue;
            vertices.push_back({center.x - 0.5f, center.y - 0.5f, 0, 1, centerColor});
            vertices.push_back({arc[i - 1].x - 0.5f, arc[i - 1].y - 0.5f, 0, 1, edgeColor});
            vertices.push_back({arc[i].x - 0.5f, arc[i].y - 0.5f, 0, 1, edgeColor});
        }
        this->TrackContentBounds({left, top, right - left, bottom - top});
        this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(arc.size() - 1), vertices.data(), sizeof(shape_vertex));
    }

    auto design_system::DrawSlotDots(const std::span<const slot_dot> dots) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || this->dot_texture_ == nullptr || dots.empty()) return;
        this->ConfigureImagePipeline(this->dot_texture_);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        this->device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        std::array<text_vertex, 256 * 6> vertices{};
        size_t count{};
        const auto flush = [&]() {
            if (count == 0) return;
            this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(count / 3), vertices.data(), sizeof(text_vertex));
            count = 0;
        };
        for (const auto& dot : dots)
        {
            if (dot.bounds.width <= 0 || dot.bounds.height <= 0) continue;
            this->TrackContentBounds(dot.bounds);
            const auto& b = dot.bounds;
            const float left = b.x - 0.5f, top = b.y - 0.5f, right = left + b.width, bottom = top + b.height;
            const text_vertex corners[]{{left, top, 0, 1, dot.color, 0, 0}, {right, top, 0, 1, dot.color, 1, 0},
                {left, bottom, 0, 1, dot.color, 0, 1}, {right, bottom, 0, 1, dot.color, 1, 1}};
            for (const auto index : {0, 1, 2, 2, 1, 3}) vertices[count++] = corners[index];
            if (count == vertices.size()) flush();
        }
        flush();
    }

    auto design_system::DrawTexture(IDirect3DTexture8* texture, const rect& bounds, const D3DCOLOR color, const rect& uv, const bool repeatX, const bool repeatY) const -> void
    {
        texture_style style{};
        style.colors.fill(color);
        style.repeat_x = repeatX;
        style.repeat_y = repeatY;
        this->DrawTexture(texture, bounds, uv, style);
    }

    auto design_system::DrawTexture(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, const texture_style& style) const -> void
    {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) return;
        this->DrawTextureQuad(texture, {{{bounds.x, bounds.y}, {bounds.x + bounds.width, bounds.y},
            {bounds.x, bounds.y + bounds.height}, {bounds.x + bounds.width, bounds.y + bounds.height}}}, uv, style);
    }

    auto design_system::DrawTextureQuad(IDirect3DTexture8* texture, const std::array<point, 4>& points, const rect& uv, const texture_style& style) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || texture == nullptr) return;
        this->ConfigureImagePipeline(texture);
        float left = points[0].x, right = left, top = points[0].y, bottom = top;
        for (const auto& point : points)
        {
            left = std::min(left, point.x); right = std::max(right, point.x);
            top = std::min(top, point.y); bottom = std::max(bottom, point.y);
        }
        this->TrackContentBounds({left, top, right - left, bottom - top});
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, style.repeat_x ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, style.repeat_y ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        this->device_->SetRenderState(D3DRS_DESTBLEND, style.additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
        const auto modulation = style.double_modulation ? D3DTOP_MODULATE2X : D3DTOP_MODULATE;
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, modulation);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, modulation);

        const std::array<text_vertex, 4> vertices{{
            {points[0].x - 0.5f, points[0].y - 0.5f, 0.0f, 1.0f, style.colors[0], uv.x, uv.y},
            {points[1].x - 0.5f, points[1].y - 0.5f, 0.0f, 1.0f, style.colors[1], uv.x + uv.width, uv.y},
            {points[2].x - 0.5f, points[2].y - 0.5f, 0.0f, 1.0f, style.colors[2], uv.x, uv.y + uv.height},
            {points[3].x - 0.5f, points[3].y - 0.5f, 0.0f, 1.0f, style.colors[3], uv.x + uv.width, uv.y + uv.height},
        }};
        if (this->clip_bounds_)
        {
            std::vector<text_vertex> clipped;
            AppendClippedQuad(clipped, vertices, this->clip_bounds_);
            if (!clipped.empty()) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(clipped.size() / 3), clipped.data(), sizeof(text_vertex));
        }
        else this->Submit(D3DPT_TRIANGLESTRIP, 2, vertices.data(), sizeof(text_vertex));
    }

    auto design_system::DrawTextureQuads(IDirect3DTexture8* texture, const std::span<const texture_quad> quads) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || texture == nullptr || quads.empty()) return;
        constexpr size_t vertexLimit = 1536;
        std::vector<text_vertex> vertices;
        vertices.reserve(std::min(quads.size(), vertexLimit / 6) * 6);
        texture_style paint{};
        bool configured{};
        const auto flush = [&] {
            if (vertices.empty()) return;
            if (!configured) { this->ConfigureImagePipeline(texture); configured = true; }
            this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, paint.repeat_x ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
            this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, paint.repeat_y ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
            this->device_->SetRenderState(D3DRS_DESTBLEND, paint.additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
            const auto modulation = paint.double_modulation ? D3DTOP_MODULATE2X : D3DTOP_MODULATE;
            this->device_->SetTextureStageState(0, D3DTSS_COLOROP, modulation);
            this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, modulation);
            this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
            vertices.clear();
        };
        for (const auto& quad : quads)
        {
            if (paint.repeat_x != quad.style.repeat_x || paint.repeat_y != quad.style.repeat_y ||
                paint.additive != quad.style.additive || paint.double_modulation != quad.style.double_modulation) flush();
            paint = quad.style;
            float left = quad.points[0].x, right = left, top = quad.points[0].y, bottom = top;
            for (const auto& point : quad.points)
            {
                left = std::min(left, point.x); right = std::max(right, point.x);
                top = std::min(top, point.y); bottom = std::max(bottom, point.y);
            }
            this->TrackContentBounds({left, top, right - left, bottom - top});
            const auto& uv = quad.uv;
            AppendClippedQuad(vertices, {{{quad.points[0].x - 0.5f, quad.points[0].y - 0.5f, 0, 1, paint.colors[0], uv.x, uv.y},
                {quad.points[1].x - 0.5f, quad.points[1].y - 0.5f, 0, 1, paint.colors[1], uv.x + uv.width, uv.y},
                {quad.points[2].x - 0.5f, quad.points[2].y - 0.5f, 0, 1, paint.colors[2], uv.x, uv.y + uv.height},
                {quad.points[3].x - 0.5f, quad.points[3].y - 0.5f, 0, 1, paint.colors[3], uv.x + uv.width, uv.y + uv.height}}}, this->clip_bounds_);
            if (vertices.size() >= vertexLimit) flush();
        }
        flush();
    }

    auto design_system::DrawTextureDisc(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, const D3DCOLOR color, const float feather) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || texture == nullptr || bounds.width <= 0 || bounds.height <= 0) return;
        this->ConfigureImagePipeline(texture);
        this->TrackContentBounds(bounds);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        this->device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        const auto vertex = [&](const float x, const float y, const float opacity = 1.0f) -> text_vertex {
            return {bounds.x + bounds.width * x - 0.5f, bounds.y + bounds.height * y - 0.5f, 0, 1, WithAlpha(color, opacity), uv.x + uv.width * x, uv.y + uv.height * y};
        };
        const float falloff = std::clamp(feather * 2 / std::min(bounds.width, bounds.height), 0.0f, 1.0f);
        const int bands = falloff > 0 ? 6 : 0;
        const float inner = 1 - falloff;
        std::vector<text_vertex> vertices; vertices.reserve(96 * (6 + bands * 6));
        for (int i = 0; i < 96; ++i)
        {
            const float a = i * 6.283185307f / 96, b = (i + 1) * 6.283185307f / 96;
            const float ax = std::cos(a) * .5f, ay = std::sin(a) * .5f, bx = std::cos(b) * .5f, by = std::sin(b) * .5f;
            const auto center = vertex(0.5f, 0.5f), first = vertex(0.5f + ax * inner, 0.5f + ay * inner),
                second = vertex(0.5f + bx * inner, 0.5f + by * inner);
            AppendClippedQuad(vertices, {center, first, second, second}, this->clip_bounds_);
            for (int band = 0; band < bands; ++band)
            {
                const float t0 = static_cast<float>(band) / bands, t1 = static_cast<float>(band + 1) / bands;
                const float r0 = inner + falloff * t0, r1 = inner + falloff * t1;
                const float opacity0 = 1 - t0 * t0 * (3 - 2 * t0), opacity1 = 1 - t1 * t1 * (3 - 2 * t1);
                AppendClippedQuad(vertices, {vertex(.5f + ax * r0, .5f + ay * r0, opacity0), vertex(.5f + bx * r0, .5f + by * r0, opacity0),
                    vertex(.5f + ax * r1, .5f + ay * r1, opacity1), vertex(.5f + bx * r1, .5f + by * r1, opacity1)}, this->clip_bounds_);
            }
        }
        if (!vertices.empty()) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
    }

    auto design_system::DrawTextureRoundedRect(IDirect3DTexture8* texture, const rect& bounds, const rect& uv, const float radius, const D3DCOLOR color) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || texture == nullptr || bounds.width <= 0 || bounds.height <= 0) return;
        if (radius < 1) { this->DrawTexture(texture, bounds, color, uv); return; }
        this->ConfigureImagePipeline(texture); this->TrackContentBounds(bounds);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
        this->device_->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
        this->device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        this->device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        this->device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        const auto vertex = [&](const point& at, const D3DCOLOR tint) -> text_vertex {
            return {at.x - 0.5f, at.y - 0.5f, 0, 1, tint,
                uv.x + uv.width * (at.x - bounds.x) / bounds.width, uv.y + uv.height * (at.y - bounds.y) / bounds.height};
        };
        const auto center = vertex({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}, color);
        std::vector<text_vertex> vertices; vertices.reserve(rounded_rect_points * 18);
        for (size_t i = 0; i < rounded_rect_points; ++i)
        {
            const auto next = (i + 1) % rounded_rect_points;
            const auto a = vertex(RoundedRectPoint(bounds, radius, i, 1), color), b = vertex(RoundedRectPoint(bounds, radius, next, 1), color);
            AppendClippedQuad(vertices, {center, a, b, b}, this->clip_bounds_);
            const auto c = vertex(RoundedRectPoint(bounds, radius, i), color & 0xFFFFFF), d = vertex(RoundedRectPoint(bounds, radius, next), color & 0xFFFFFF);
            AppendClippedQuad(vertices, {a, b, c, d}, this->clip_bounds_);
        }
        if (!vertices.empty()) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
    }

    auto design_system::DrawContour(const rect& bounds, const std::span<const point> contour, const contour_style& style, IDirect3DTexture8* texture, const rect& uv) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || contour.size() < 3 || bounds.width <= 0 || bounds.height <= 0) return;
        if (texture != nullptr) this->ConfigureImagePipeline(texture);
        else this->ConfigureShapePipeline();
        this->TrackContentBounds(bounds);
        const auto vertex = [&](const point at, const D3DCOLOR color) -> text_vertex {
            return {at.x - 0.5f, at.y - 0.5f, 0, 1, color,
                uv.x + (at.x - bounds.x) / bounds.width * uv.width, uv.y + (at.y - bounds.y) / bounds.height * uv.height};
        };
        const auto center = vertex({bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}, style.color);
        const float feather = std::max(0.0f, style.feather), inner = feather + std::max(0.0f, style.edge_width);
        std::vector<text_vertex> vertices; vertices.reserve(contour.size() * (style.edge_width > 0 ? 18 : 12));
        for (size_t i = 0; i < contour.size(); ++i)
        {
            const auto next = (i + 1) % contour.size();
            const auto a = vertex(ContourPoint(bounds, contour[i], inner), style.color), b = vertex(ContourPoint(bounds, contour[next], inner), style.color);
            AppendClippedQuad(vertices, {center, a, b, b}, this->clip_bounds_);
            const auto c = vertex(ContourPoint(bounds, contour[i], feather), style.edge_width > 0 ? style.edge_color : style.color);
            const auto d = vertex(ContourPoint(bounds, contour[next], feather), style.edge_width > 0 ? style.edge_color : style.color);
            if (style.edge_width > 0) AppendClippedQuad(vertices, {a, b, c, d}, this->clip_bounds_);
            const auto e = vertex(ContourPoint(bounds, contour[i]), c.color & 0xFFFFFF), f = vertex(ContourPoint(bounds, contour[next]), d.color & 0xFFFFFF);
            AppendClippedQuad(vertices, {c, d, e, f}, this->clip_bounds_);
        }
        if (!vertices.empty()) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
    }

    auto design_system::DrawRing(const rect& bounds, const D3DCOLOR color, const float thickness) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0 || bounds.height <= 0) return;
        this->ConfigureShapePipeline(); this->TrackContentBounds(bounds);
        std::array<shape_vertex, 96 * 6> vertices{};
        const float rx = bounds.width / 2, ry = bounds.height / 2, inset = std::clamp(thickness, 0.0f, std::min(rx, ry));
        const auto vertex = [&](const float a, const bool inner) -> shape_vertex {
            return {bounds.x + rx + std::cos(a) * (rx - (inner ? inset : 0)) - 0.5f,
                bounds.y + ry + std::sin(a) * (ry - (inner ? inset : 0)) - 0.5f, 0, 1, color};
        };
        for (int i = 0; i < 96; ++i)
        {
            const float a = i * 6.283185307f / 96, b = (i + 1) * 6.283185307f / 96;
            const std::array<shape_vertex, 4> quad{vertex(a, false), vertex(b, false), vertex(a, true), vertex(b, true)};
            size_t offset = i * 6;
            for (const int index : {0, 1, 2, 1, 3, 2}) vertices[offset++] = quad[index];
        }
        this->Submit(D3DPT_TRIANGLELIST, 96 * 2, vertices.data(), sizeof(shape_vertex));
    }

    auto design_system::GradientRect(const rect& bounds, const D3DCOLOR leftColor, const D3DCOLOR rightColor) const -> void
    {
        this->GradientRect(bounds, {leftColor, rightColor, leftColor, rightColor});
    }

    auto design_system::GradientRect(const rect& bounds, const std::array<D3DCOLOR, 4>& colors) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;
        this->ConfigureShapePipeline();

        const float left = bounds.x - 0.5f;
        const float top = bounds.y - 0.5f;
        const float right = bounds.x + bounds.width - 0.5f;
        const float bottom = bounds.y + bounds.height - 0.5f;
        const shape_vertex vertices[] = {
            {left, top, 0.0f, 1.0f, colors[0]},
            {right, top, 0.0f, 1.0f, colors[1]},
            {left, bottom, 0.0f, 1.0f, colors[2]},
            {right, bottom, 0.0f, 1.0f, colors[3]},
        };
        this->Submit(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(shape_vertex));
    }

    auto design_system::BorderRect(const rect& bounds, const D3DCOLOR color, const float thickness) const -> void
    {
        this->FillRect({bounds.x, bounds.y, bounds.width, thickness}, color);
        this->FillRect({bounds.x, bounds.y + bounds.height - thickness, bounds.width, thickness}, color);
        this->FillRect({bounds.x, bounds.y, thickness, bounds.height}, color);
        this->FillRect({bounds.x + bounds.width - thickness, bounds.y, thickness, bounds.height}, color);
    }

    auto design_system::GradientRoundedRect(
        const rect& bounds,
        const D3DCOLOR topColor,
        const D3DCOLOR bottomColor,
        const float requestedRadius,
        const uint8_t roundedCorners) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;
        this->ConfigureShapePipeline();

        const bool oneSide = roundedCorners == 0x3 || roundedCorners == 0xC;
        const float radius = std::clamp(requestedRadius, 0.0f,
            std::min(bounds.width * (oneSide ? 1.0f : 0.5f), bounds.height * 0.5f));
        if (radius < 1.0f)
        {
            const float left = bounds.x - 0.5f;
            const float top = bounds.y - 0.5f;
            const float right = bounds.x + bounds.width - 0.5f;
            const float bottom = bounds.y + bounds.height - 0.5f;
            const shape_vertex vertices[] = {
                {left, top, 0.0f, 1.0f, topColor},
                {right, top, 0.0f, 1.0f, topColor},
                {left, bottom, 0.0f, 1.0f, bottomColor},
                {right, bottom, 0.0f, 1.0f, bottomColor},
            };
            this->Submit(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(shape_vertex));
            return;
        }

        struct edge_point
        {
            float x{};
            float y{};
        };
        constexpr float pi = 3.14159265358979323846f;
        const int segments = std::clamp(static_cast<int>(std::ceil(radius * 0.8f)), 8, 18);
        const float fringe = std::min(1.0f / std::max(this->transform_.x, this->transform_.y), std::min(bounds.width, bounds.height) * 0.25f);
        const float leftInset = roundedCorners == 0x3 ? 0.0f : fringe;
        const float rightInset = roundedCorners == 0xC ? 0.0f : fringe;
        const rect innerBounds{
            bounds.x + leftInset,
            bounds.y + fringe,
            bounds.width - leftInset - rightInset,
            bounds.height - fringe * 2.0f};
        const float innerRadius = std::max(0.0f, radius - fringe);
        std::array<edge_point, 80> outerPoints{};
        std::array<edge_point, 80> innerPoints{};
        size_t perimeterCount{};
        const auto addPerimeter = [segments, roundedCorners](const rect& target, const float targetRadius, std::array<edge_point, 80>& points) {
            size_t count{};
            const auto addCorner = [&points, &count, segments](const float centerX, const float centerY, const float startAngle, const float cornerRadius) {
                for (int segment = 0; segment <= segments; ++segment)
                {
                    const float angle = startAngle + (pi * 0.5f) * static_cast<float>(segment) / static_cast<float>(segments);
                    points[count++] = {centerX + std::cos(angle) * cornerRadius, centerY + std::sin(angle) * cornerRadius};
                }
            };
            const auto r = [&](const uint8_t corner) { return (roundedCorners & corner) != 0 ? targetRadius : 0.0f; };
            addCorner(target.x + target.width - r(1), target.y + r(1), -pi * 0.5f, r(1));
            addCorner(target.x + target.width - r(2), target.y + target.height - r(2), 0.0f, r(2));
            addCorner(target.x + r(4), target.y + target.height - r(4), pi * 0.5f, r(4));
            addCorner(target.x + r(8), target.y + r(8), pi, r(8));
            return count;
        };
        perimeterCount = addPerimeter(bounds, radius, outerPoints);
        addPerimeter(innerBounds, innerRadius, innerPoints);

        const auto colorAt = [&bounds, topColor, bottomColor](const float y) {
            return LerpColor(topColor, bottomColor, std::clamp((y - bounds.y) / bounds.height, 0.0f, 1.0f));
        };
        std::array<shape_vertex, 80> body{};
        size_t bodyCount{};
        body[bodyCount++] = {
            bounds.x + bounds.width * 0.5f - 0.5f,
            bounds.y + bounds.height * 0.5f - 0.5f,
            0.0f,
            1.0f,
            colorAt(bounds.y + bounds.height * 0.5f)};
        for (size_t index = 0; index < perimeterCount; ++index)
        {
            const auto& point = innerPoints[index];
            body[bodyCount++] = {point.x - 0.5f, point.y - 0.5f, 0.0f, 1.0f, colorAt(point.y)};
        }
        body[bodyCount++] = body[1];
        this->Submit(D3DPT_TRIANGLEFAN, static_cast<UINT>(bodyCount - 2), body.data(), sizeof(shape_vertex));

        std::array<shape_vertex, 160> fringeVertices{};
        size_t fringeCount{};
        for (size_t index = 0; index <= perimeterCount; ++index)
        {
            const size_t pointIndex = index % perimeterCount;
            const auto& inner = innerPoints[pointIndex];
            const auto& outer = outerPoints[pointIndex];
            const D3DCOLOR innerColor = colorAt(inner.y);
            const D3DCOLOR outerColor = colorAt(outer.y) & 0x00FFFFFFu;
            fringeVertices[fringeCount++] = {inner.x - 0.5f, inner.y - 0.5f, 0.0f, 1.0f, innerColor};
            fringeVertices[fringeCount++] = {outer.x - 0.5f, outer.y - 0.5f, 0.0f, 1.0f, outerColor};
        }
        this->Submit(D3DPT_TRIANGLESTRIP, static_cast<UINT>(fringeCount - 2), fringeVertices.data(), sizeof(shape_vertex));
    }

    auto design_system::DrawPanel(const rect& bounds, const panel_style& style) const -> void
    {
        this->FillRect({bounds.x + style.shadow_offset, bounds.y + style.shadow_offset, bounds.width, bounds.height}, style.shadow);
        this->FillRect(bounds, style.background);
        this->BorderRect(bounds, style.border, style.border_width);
    }

    auto design_system::DrawMeterCap(const rect& bounds, const bool right, const float opacity) const -> void
    {
        if (bounds.width <= 0 || bounds.height <= 0) return;
        const auto& colors = this->theme_.colors;
        const uint8_t corners = right ? 0x3 : 0xC;
        const float radius = std::min(bounds.width, bounds.height * 0.5f);
        const float edge = std::min({1.0f, bounds.width * 0.2f, bounds.height * 0.16f});
        const auto inset = [&](const float amount) {
            return rect{bounds.x + (right ? 0 : amount), bounds.y + amount,
                bounds.width - amount, bounds.height - amount * 2};
        };
        this->GradientRoundedRect(bounds, WithAlpha(Darken(colors.accent_dim, 0.65f), opacity),
            WithAlpha(Darken(colors.chrome, 0.65f), opacity), radius, corners);
        this->GradientRoundedRect(inset(edge), WithAlpha(LerpColor(colors.border, colors.accent, 0.6f), opacity),
            WithAlpha(Darken(colors.accent_dim, 0.55f), opacity), std::max(0.0f, radius - edge), corners);
        this->GradientRoundedRect(inset(edge * 1.7f), WithAlpha(Darken(colors.border, 0.8f), opacity),
            WithAlpha(Darken(colors.border, 0.5f), opacity), std::max(0.0f, radius - edge * 1.7f), corners);
    }

    auto design_system::DrawMeter(const rect& bounds, const float ratio, const D3DCOLOR color, const float sheenPhase) const -> void
    {
        this->DrawMeter(bounds, ratio, Darken(color, 0.45f), color, sheenPhase, this->theme_.meter);
    }

    auto design_system::DrawMeter(
        const rect& bounds,
        const float ratio,
        const D3DCOLOR fillTop,
        const D3DCOLOR fillBottom,
        const float sheenPhase,
        const meter_style& style,
        const meter_skin* skin,
        const std::span<const meter_fill_layer> layers,
        const bool fillFromRight,
        const std::span<const meter_strip> strips,
        const std::span<const meter_range> ranges,
        const std::span<const meter_range> feedback) const -> void
    {
        if (!this->frame_active_ || bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;

        const bool themed = style.appearance == meter_style::window_theme && skin != nullptr;
        const float shapeRadius = themed ? (style.themed_bookends ? bounds.height * 0.5f : 0.0f) : style.bookends
            ? bounds.height * 0.5f : std::clamp(style.corner_radius, 0.0f, std::min(bounds.width, bounds.height) * 0.5f);
        const float borderWidth = themed ? 0.0f : std::clamp(style.border_width, 0.0f, 20.0f);
        const rect outlineBounds{
            bounds.x - borderWidth,
            bounds.y - borderWidth,
            bounds.width + borderWidth * 2.0f,
            bounds.height + borderWidth * 2.0f};
        const float outlineRadius = style.bookends
            ? outlineBounds.height * 0.5f
            : (shapeRadius > 0.0f ? shapeRadius + borderWidth : 0.0f);
        const rect& effectBounds = borderWidth > 0.0f ? outlineBounds : bounds;
        this->TrackContentBounds(effectBounds);
        const float effectRadius = borderWidth > 0.0f ? outlineRadius : shapeRadius;
        const auto drawSoftLayer = [this](const rect& targetBounds, const float targetRadius, const D3DCOLOR color, const float extent, const float offsetX, const float offsetY) {
            const int layers = std::clamp(static_cast<int>(std::ceil(extent)), 1, 8);
            for (int layer = layers; layer >= 1; --layer)
            {
                const float expansion = extent * static_cast<float>(layer) / static_cast<float>(layers);
                const float alpha = 0.12f + 0.28f * (1.0f - static_cast<float>(layer) / static_cast<float>(layers));
                this->GradientRoundedRect(
                    {targetBounds.x + offsetX - expansion, targetBounds.y + offsetY - expansion, targetBounds.width + expansion * 2.0f, targetBounds.height + expansion * 2.0f},
                    WithAlpha(color, alpha),
                    WithAlpha(color, alpha),
                    targetRadius + expansion);
            }
        };

        if (style.shadow_enabled && style.shadow_strength > 0.0f)
        {
            const D3DCOLOR shadowColor = WithAlpha(style.shadow, style.shadow_strength);
            if (style.shadow_blur > 0.0f)
                drawSoftLayer(effectBounds, effectRadius, shadowColor, style.shadow_blur, style.shadow_offset_x, style.shadow_offset_y);
            else
                this->GradientRoundedRect(
                    {effectBounds.x + style.shadow_offset_x, effectBounds.y + style.shadow_offset_y, effectBounds.width, effectBounds.height},
                    shadowColor,
                    shadowColor,
                    effectRadius);
        }
        if (style.glow_enabled && style.glow_size > 0.0f)
            drawSoftLayer(effectBounds, effectRadius, style.glow, style.glow_size, 0.0f, 0.0f);

        if (borderWidth > 0.0f)
            this->GradientRoundedRect(outlineBounds, style.border, style.border, outlineRadius);

        rect content = bounds;
        float contentRadius = shapeRadius;
        const auto drawTexture = [this](const meter_texture& texture, const rect& target, const float opacity) {
            auto uv = texture.uv;
            if (texture.style.repeat_x) uv.width = target.width / texture.texture_width;
            if (texture.style.repeat_y) uv.height = target.height / texture.texture_height;
            auto paint = texture.style;
            for (auto& color : paint.colors) color = WithAlpha(color, opacity);
            this->DrawTexture(texture.texture, target, uv, paint);
        };
        if (themed)
        {
            const auto layout = LayoutMeterSkin(bounds, *skin, style.themed_bookends);
            content = layout.content;
            contentRadius = 0;
            const float backingOpacity = style.track_opacity * this->dark_backing_opacity_;
            this->FillRect(layout.frame[1], WithAlpha(0xFF000000, backingOpacity));
            drawTexture(skin->background, skin->xiv1 ? content : layout.frame[1], style.track_opacity);
            this->FillRect(content, WithAlpha(0x80000000, backingOpacity));
            if (!skin->xiv1)
            {
                this->DrawMeterCap(layout.frame[0], false, style.track_opacity);
                this->DrawMeterCap(layout.frame[2], true, style.track_opacity);
            }
        }
        else if (style.bookends)
        {
            this->GradientRoundedRect(
                bounds,
                WithAlpha(style.bookend_top, style.track_opacity),
                WithAlpha(style.bookend_bottom, style.track_opacity),
                bounds.height * 0.5f);
            const float bookendWidth = std::clamp(std::max(bounds.height * 0.5f, style.bookend_width), 0.0f, bounds.width * 0.5f);
            content.x += bookendWidth;
            content.width = std::max(0.0f, content.width - bookendWidth * 2.0f);
            contentRadius = 0.0f;
        }
        if (!themed)
            this->GradientRoundedRect(
                content,
                WithAlpha(style.track_top, style.track_opacity),
                WithAlpha(style.track_bottom, style.track_opacity),
                contentRadius);

        const meter_fill_layer single{ratio, fillTop, fillBottom};
        const auto fills = layers.empty() ? std::span{&single, 1} : layers;
        float widest{};
        for (const auto& layer : fills)
        {
            const auto fill = LayoutMeterFill(content, layer.ratio, fillFromRight);
            const float fillWidth = fill.width;
            if (fillWidth <= 0) continue;
            widest = std::max(widest, fillWidth);
            const float fillRadius = themed || style.bookends ? 0.0f : std::min(contentRadius, fillWidth * 0.5f);
            if (style.fill_glow_enabled && style.fill_glow_radius > 0 && style.fill_glow_strength > 0)
                drawSoftLayer(fill, fillRadius, WithAlpha(layer.bottom, style.fill_glow_strength), style.fill_glow_radius, 0, 0);
            if (themed)
            {
                auto paint = skin->fill.style;
                paint.additive = paint.double_modulation = false;
                paint.colors = {layer.top, layer.top, layer.bottom, layer.bottom};
                auto uv = skin->fill.uv;
                if (fillFromRight) uv.x += uv.width * (1 - std::clamp(layer.ratio, 0.0f, 1.0f));
                uv.width *= std::clamp(layer.ratio, 0.0f, 1.0f);
                this->DrawTexture(skin->fill.texture, fill, uv, paint);
            }
            else this->GradientRoundedRect(fill, layer.top, layer.bottom, fillRadius);
        }
        if (style.sheen_enabled && widest > 0)
            this->DrawSheen({content.x + (fillFromRight ? content.width - widest : 0), content.y, widest, content.height}, style.sheen, sheenPhase, style.sheen_width,
                themed || style.bookends ? 0.0f : std::min(contentRadius, widest * 0.5f));
        for (const auto collection : {feedback, ranges})
        for (const auto& range : collection)
        {
            const auto segment = LayoutMeterRange(content, range, ratio, fillFromRight);
            if (segment.width <= 0 || content.width <= 0 || ((range.top | range.bottom) >> 24) == 0) continue;
            if (themed && !range.striped)
            {
                auto paint = skin->fill.style;
                paint.additive = paint.double_modulation = false;
                paint.colors = {range.top, range.top, range.bottom, range.bottom};
                auto uv = skin->fill.uv;
                uv.x += uv.width * (segment.x - content.x) / content.width;
                uv.width *= segment.width / content.width;
                this->DrawTexture(skin->fill.texture, segment, uv, paint);
            }
            else
            {
                const auto fill = LayoutMeterFill(content, range.beyond_fill ? 1.0f : ratio, fillFromRight);
                const uint8_t corners = static_cast<uint8_t>((segment.x <= fill.x ? 0xC : 0) |
                    (segment.x + segment.width >= fill.x + fill.width - 0.01f ? 0x3 : 0));
                const float radius = themed || style.bookends ? 0.0f : std::min(contentRadius, segment.width * 0.5f);
                this->GradientRoundedRect(segment, range.top, range.bottom, radius, corners);
                if (range.striped) this->DrawMeterStripes(segment, range.bottom, radius, corners);
            }
        }
        for (const auto& strip : strips)
        {
            auto rail = LayoutMeterStrip(content, strip.height, strip.inset);
            const float inset = std::min(contentRadius, rail.width * 0.5f);
            rail.x += inset; rail.width -= inset * 2;
            this->GradientRoundedRect(rail, WithAlpha(Darken(strip.top, 0.4f), 0.6f), WithAlpha(Darken(strip.bottom, 0.4f), 0.6f), 0);
            this->GradientRoundedRect(LayoutMeterFill(rail, strip.ratio, fillFromRight), strip.top, strip.bottom, 0);
        }
        if (themed && skin->xiv1) DrawXiv1MeterFrame(*this, bounds, *skin, style.themed_bookends, style.track_opacity);
    }

    auto design_system::DrawMeterStripes(const rect& bounds, const D3DCOLOR color, const float radius, const uint8_t roundedCorners) const -> void
    {
        rect pattern = bounds;
        const float leftInset = (roundedCorners & 0xC) != 0 ? radius : 0;
        const float rightInset = (roundedCorners & 0x3) != 0 ? radius : 0;
        pattern.x += leftInset;
        pattern.width -= leftInset + rightInset;
        if (pattern.width <= 0 || pattern.height <= 0) return;
        const auto clip = this->Clip(pattern);
        const float pitch = std::clamp(bounds.height * 1.1f, 6.0f, 12.0f), width = pitch * 0.4f;
        const auto tint = Darken(color, 0.22f);
        const auto vertex = [tint](const float x, const float y) -> text_vertex { return {x - 0.5f, y - 0.5f, 0, 1, tint, 0, 0}; };
        std::vector<text_vertex> vertices;
        vertices.reserve(static_cast<size_t>((pattern.width + bounds.height) / pitch + 2) * 12);
        for (float x = bounds.x - bounds.height; x < pattern.x + pattern.width; x += pitch)
        {
            const auto a = vertex(x, bounds.y), b = vertex(x + width, bounds.y);
            const auto c = vertex(x + bounds.height, bounds.y + bounds.height), d = vertex(x + bounds.height + width, bounds.y + bounds.height);
            AppendClippedQuad(vertices, {a, b, c, d}, this->clip_bounds_);
        }
        if (vertices.empty()) return;
        this->ConfigureShapePipeline();
        this->device_->SetVertexShader(text_vertex_format);
        this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
    }

    auto design_system::DrawToggle(const rect& bounds, const bool enabled) const -> void
    {
        this->FillRect(bounds, enabled ? this->theme_.colors.selected : this->theme_.colors.surface_raised);
        this->BorderRect(bounds, enabled ? this->theme_.colors.accent_dim : this->theme_.colors.border);
        this->FillRect(
            {bounds.x + (enabled ? bounds.width - 13.0f : 3.0f), bounds.y + 3.0f, 10.0f, std::max(1.0f, bounds.height - 6.0f)},
            enabled ? this->theme_.colors.accent : this->theme_.colors.muted);
    }

    auto design_system::BeginContentBounds(void) -> void
    {
        this->content_bounds_ = {};
        this->tracking_content_bounds_ = true;
    }

    auto design_system::EndContentBounds(void) -> rect
    {
        this->tracking_content_bounds_ = false;
        return this->content_bounds_;
    }

    auto design_system::TrackContentBounds(const rect& bounds) const -> void
    {
        if (!this->tracking_content_bounds_ || bounds.width <= 0.0f || bounds.height <= 0.0f)
            return;
        auto visible = bounds;
        if (this->clip_bounds_)
        {
            const auto& clip = *this->clip_bounds_;
            const auto left = std::max(bounds.x, clip.x), top = std::max(bounds.y, clip.y);
            visible = {left, top, std::max(0.0f, std::min(bounds.x + bounds.width, clip.x + clip.width) - left),
                std::max(0.0f, std::min(bounds.y + bounds.height, clip.y + clip.height) - top)};
        }
        if (visible.width <= 0 || visible.height <= 0) return;
        if (this->content_bounds_.width <= 0.0f || this->content_bounds_.height <= 0.0f)
        {
            this->content_bounds_ = visible;
            return;
        }
        const float left = std::min(this->content_bounds_.x, visible.x);
        const float top = std::min(this->content_bounds_.y, visible.y);
        const float right = std::max(this->content_bounds_.x + this->content_bounds_.width, visible.x + visible.width);
        const float bottom = std::max(this->content_bounds_.y + this->content_bounds_.height, visible.y + visible.height);
        this->content_bounds_ = {left, top, right - left, bottom - top};
    }

    auto design_system::DrawEditFrame(const rect& bounds, const bool resizable) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr) return;
        this->ConfigureShapePipeline();
        std::array<shape_vertex, 4092> vertices;
        size_t used{};
        const auto flush = [&] {
            if (used != 0) this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(used / 3), vertices.data(), sizeof(shape_vertex));
            used = 0;
        };
        const auto fill = [&](const rect& at, const D3DCOLOR color) {
            if (at.width <= 0 || at.height <= 0) return;
            const float left = at.x - 0.5f, top = at.y - 0.5f;
            const float right = at.x + at.width - 0.5f, bottom = at.y + at.height - 0.5f;
            vertices[used++] = {left, top, 0, 1, color}; vertices[used++] = {right, top, 0, 1, color}; vertices[used++] = {left, bottom, 0, 1, color};
            vertices[used++] = {left, bottom, 0, 1, color}; vertices[used++] = {right, top, 0, 1, color}; vertices[used++] = {right, bottom, 0, 1, color};
            if (used == vertices.size()) flush();
        };
        constexpr float dash = 6.0f;
        constexpr float gap = 4.0f;
        for (float x = bounds.x; x < bounds.x + bounds.width; x += dash + gap)
        {
            fill({x, bounds.y, std::min(dash, bounds.x + bounds.width - x), 1.0f}, this->theme_.colors.accent_dim);
            fill({x, bounds.y + bounds.height - 1.0f, std::min(dash, bounds.x + bounds.width - x), 1.0f}, this->theme_.colors.accent_dim);
        }
        for (float y = bounds.y; y < bounds.y + bounds.height; y += dash + gap)
        {
            fill({bounds.x, y, 1.0f, std::min(dash, bounds.y + bounds.height - y)}, this->theme_.colors.accent_dim);
            fill({bounds.x + bounds.width - 1.0f, y, 1.0f, std::min(dash, bounds.y + bounds.height - y)}, this->theme_.colors.accent_dim);
        }
        if (resizable)
        {
            fill({bounds.x + bounds.width - 10.0f, bounds.y + bounds.height - 3.0f, 8.0f, 1.0f}, this->theme_.colors.accent);
            fill({bounds.x + bounds.width - 6.0f, bounds.y + bounds.height - 7.0f, 4.0f, 1.0f}, this->theme_.colors.accent);
        }
        flush();
    }

    design_system::clip_scope::clip_scope(const design_system& design, const rect& bounds)
        : design_(design), previous_(design.clip_bounds_)
    {
        auto clipped = bounds;
        if (this->previous_)
        {
            const auto& previous = *this->previous_;
            const auto left = std::max(bounds.x, previous.x), top = std::max(bounds.y, previous.y);
            clipped = {left, top, std::max(0.0f, std::min(bounds.x + bounds.width, previous.x + previous.width) - left),
                std::max(0.0f, std::min(bounds.y + bounds.height, previous.y + previous.height) - top)};
        }
        design.clip_bounds_ = clipped;
    }

    design_system::clip_scope::~clip_scope(void) { this->design_.clip_bounds_ = this->previous_; }

    auto design_system::DrawText(const char* text, const rect& bounds, const text_style& source) const -> void
    {
        const auto style = this->ResolveHudText(source);
        if (!this->frame_active_ || text == nullptr || text[0] == '\0' || !this->FontAvailable(0))
            return;

        const float measured = this->MeasureText(text, style);
        const float availableWidth = std::max(0.0f, bounds.width);
        float alignmentOffset{};
        if (style.alignment == text_alignment::right)
            alignmentOffset = std::max(0.0f, availableWidth - measured);
        else if (style.alignment == text_alignment::center)
            alignmentOffset = std::max(0.0f, (availableWidth - measured) * 0.5f);
        const float x = bounds.x + alignmentOffset;
        const float visibleHeight = this->TextHeight(style);
        const float y = bounds.y + std::max(0.0f, (bounds.height - visibleHeight) * 0.5f);
        // Alignment rounding must not shorten text that fits the available width.
        const float maxWidth = measured <= availableWidth ? std::max(measured, availableWidth - alignmentOffset) : availableWidth;
        this->TrackContentBounds({x, y, std::min(measured, maxWidth), visibleHeight});

        if (style.glow.enabled)
        {
            text_style glowStyle = style;
            glowStyle.top_color = style.glow.color;
            glowStyle.bottom_color = style.glow.color;
            this->DrawTextPass(text, x, y, maxWidth, glowStyle, font_mask::glow, 0.0f, 0.0f, true, false);
        }
        if (style.shadow.enabled)
        {
            text_style shadowStyle = style;
            shadowStyle.top_color = style.shadow.color;
            shadowStyle.bottom_color = style.shadow.color;
            this->DrawTextPass(
                text,
                x,
                y,
                maxWidth,
                shadowStyle,
                style.shadow.soft ? font_mask::glow : font_mask::base,
                style.shadow.offset_x,
                style.shadow.offset_y,
                false,
                false);
        }
        if (style.stroke.enabled)
        {
            text_style strokeStyle = style;
            strokeStyle.top_color = style.stroke.color;
            strokeStyle.bottom_color = style.stroke.color;
            this->DrawTextPass(text, x, y, maxWidth, strokeStyle, font_mask::outline, 0.0f, 0.0f, false, false);
        }

        this->DrawTextPass(text, x, y, maxWidth, style, font_mask::base, 0.0f, 0.0f, false, false);
        if (style.sheen.enabled)
            this->DrawTextPass(text, x, y, maxWidth, style, font_mask::base, 0.0f, 0.0f, true, true);
    }

    auto design_system::DrawTextPass(const char* text, const float x, const float y, const float maxWidth, const text_style& style, const font_mask mask, const float offsetX, const float offsetY, const bool additive, const bool sheen) const -> void
    {
        if (this->device_ == nullptr || text == nullptr || maxWidth <= 0.0f) return;
        const int pixels = ScaledTextPixelSize(style.scale, this->transform_.y);
        const float rasterScale = 1 / this->transform_.y;
        const auto font = this->glyphs_.Metrics(style.font, pixels);
        const float tracking = style.letter_spacing * TextPixelScale(style.scale);
        IDirect3DTexture8* texture{};
        std::vector<text_vertex> vertices;
        vertices.reserve(std::strlen(text) * 6);
        const auto flush = [&] {
            if (vertices.empty()) return;
            this->ConfigureTextPipeline(texture, additive);
            this->Submit(D3DPT_TRIANGLELIST, static_cast<UINT>(vertices.size() / 3), vertices.data(), sizeof(text_vertex));
            vertices.clear();
        };
        double advance{};
        const float phase = std::fmod(static_cast<float>(::GetTickCount64()) * 0.001f * style.sheen.speed, 1.0f);
        const float sheenCenter = x - style.sheen.width + phase * (maxWidth + style.sheen.width * 2.0f);
        const std::string_view content{text};
        for (size_t position = 0; position < content.size();)
        {
            auto codepoint = xiui::NextUtf8(content, position);
            if (codepoint < 32) codepoint = '?';
            const auto metrics = TextGlyphMetrics(codepoint, this->glyphs_.Advance(style.font, pixels, codepoint) * rasterScale,
                font.digit_advance * rasterScale, style.tabular_numbers);
            if (advance + metrics.advance > maxWidth + 0.001) break;
            if (codepoint != ' ')
            {
                const auto glyph = this->glyphs_.Get(this->device_, style.font, pixels, codepoint, flush);
                auto* nextTexture = glyph.textures[static_cast<size_t>(mask)];
                if (texture != nextTexture) { flush(); texture = nextTexture; }
                if (texture != nullptr)
                {
                    const float baselineOffset = (font.ascent - glyph.ascent) * rasterScale;
                    const float left = this->transform_.InverseX(std::round(this->transform_.X(x + static_cast<float>(advance) + metrics.offset + offsetX))) - glyph.padding * rasterScale - 0.5f;
                    const float top = this->transform_.InverseY(std::round(this->transform_.Y(y + baselineOffset + offsetY))) - glyph.padding * rasterScale - 0.5f;
                    const float right = left + glyph.width * rasterScale, bottom = top + glyph.height * rasterScale;
                    const D3DCOLOR topLeft = sheen ? SheenColor(style.sheen.color, left, sheenCenter, style.sheen.width) : style.top_color;
                    const D3DCOLOR topRight = sheen ? SheenColor(style.sheen.color, right, sheenCenter, style.sheen.width) : style.top_color;
                    const D3DCOLOR bottomLeft = sheen ? topLeft : style.bottom_color;
                    const D3DCOLOR bottomRight = sheen ? topRight : style.bottom_color;
                    AppendClippedQuad(vertices, {{{left, top, 0, 1, topLeft, glyph.u0, glyph.v0}, {right, top, 0, 1, topRight, glyph.u1, glyph.v0},
                        {left, bottom, 0, 1, bottomLeft, glyph.u0, glyph.v1}, {right, bottom, 0, 1, bottomRight, glyph.u1, glyph.v1}}}, this->clip_bounds_);
                }
            }
            advance += metrics.advance + static_cast<double>(tracking);
        }
        flush();
    }

    auto design_system::TextHeight(const text_style& source) const -> float
    {
        const auto style = this->ResolveHudText(source);
        return this->glyphs_.Metrics(style.font, ScaledTextPixelSize(style.scale, this->transform_.y)).height / this->transform_.y;
    }

    auto design_system::Submit(const D3DPRIMITIVETYPE type, const UINT primitives, const void* vertices, const UINT stride) const -> void
    {
        if (this->transform_.Identity()) { this->device_->DrawPrimitiveUP(type, primitives, vertices, stride); return; }
        const size_t count = type == D3DPT_TRIANGLELIST ? primitives * 3 : primitives + 2;
        this->transformed_vertices_.resize(count * stride);
        std::memcpy(this->transformed_vertices_.data(), vertices, count * stride);
        for (size_t index = 0; index < count; ++index)
        {
            auto* vertex = this->transformed_vertices_.data() + index * stride;
            float x{}, y{}; std::memcpy(&x, vertex, sizeof(float)); std::memcpy(&y, vertex + sizeof(float), sizeof(float));
            x = this->transform_.X(x + .5f) - .5f; y = this->transform_.Y(y + .5f) - .5f;
            std::memcpy(vertex, &x, sizeof(float)); std::memcpy(vertex + sizeof(float), &y, sizeof(float));
        }
        this->device_->DrawPrimitiveUP(type, primitives, this->transformed_vertices_.data(), stride);
    }

    auto design_system::MeasureText(const char* text, const text_style& source) const -> float
    {
        const auto style = this->ResolveHudText(source);
        if (text == nullptr) return 0;
        const int pixels = ScaledTextPixelSize(style.scale, this->transform_.y);
        const auto font = this->glyphs_.Metrics(style.font, pixels);
        double width{};
        const std::string_view content{text};
        for (size_t position = 0; position < content.size();)
        {
            auto codepoint = xiui::NextUtf8(content, position);
            if (codepoint < 32) codepoint = '?';
            width += TextGlyphMetrics(codepoint, this->glyphs_.Advance(style.font, pixels, codepoint), font.digit_advance, style.tabular_numbers).advance / this->transform_.y;
            if (position < content.size()) width += style.letter_spacing * TextPixelScale(style.scale);
        }
        return static_cast<float>(width);
    }

    auto design_system::DrawSheen(
        const rect& bounds,
        const D3DCOLOR color,
        const float phase,
        const float width,
        const float requestedRadius) const -> void
    {
        if (!this->frame_active_ || this->device_ == nullptr || bounds.width <= 0.0f || bounds.height <= 0.0f || width <= 0.0f)
            return;

        const float center = bounds.x - width + std::clamp(phase, 0.0f, 1.0f) * (bounds.width + width * 2.0f);
        const float rightEdge = bounds.x + bounds.width;
        const float radius = std::clamp(requestedRadius, 0.0f, std::min(bounds.width, bounds.height) * 0.5f);
        if (center + width <= bounds.x || center - width >= rightEdge)
            return;
        this->ConfigureShapePipeline();

        constexpr int baseSlices = 48;
        constexpr int cornerSlices = 12;
        std::array<float, 96> positions{};
        size_t positionCount{};
        for (int slice = 0; slice <= baseSlices; ++slice)
            positions[positionCount++] = bounds.x + bounds.width * static_cast<float>(slice) / static_cast<float>(baseSlices);
        if (radius > 0.0f)
        {
            for (int slice = 0; slice <= cornerSlices; ++slice)
            {
                const float offset = radius * static_cast<float>(slice) / static_cast<float>(cornerSlices);
                positions[positionCount++] = bounds.x + offset;
                positions[positionCount++] = rightEdge - radius + offset;
            }
        }
        positions[positionCount++] = std::clamp(center - width, bounds.x, rightEdge);
        positions[positionCount++] = std::clamp(center, bounds.x, rightEdge);
        positions[positionCount++] = std::clamp(center + width, bounds.x, rightEdge);
        positions[positionCount++] = std::min(rightEdge, bounds.x + 1.0f);
        positions[positionCount++] = std::max(bounds.x, rightEdge - 1.0f);
        std::sort(positions.begin(), positions.begin() + static_cast<ptrdiff_t>(positionCount));
        size_t uniqueCount{};
        for (size_t index = 0; index < positionCount; ++index)
        {
            if (uniqueCount == 0 || std::abs(positions[index] - positions[uniqueCount - 1]) > 0.01f)
                positions[uniqueCount++] = positions[index];
        }

        std::array<shape_vertex, 192> topFringe{};
        std::array<shape_vertex, 192> core{};
        std::array<shape_vertex, 192> bottomFringe{};
        size_t topCount{};
        size_t coreCount{};
        size_t bottomCount{};
        const D3DCOLOR transparent = color & 0x00FFFFFFu;
        for (size_t index = 0; index < uniqueCount; ++index)
        {
            const float x = positions[index];
            float inset{};
            if (radius > 0.0f && x < bounds.x + radius)
            {
                const float distance = bounds.x + radius - x;
                inset = radius - std::sqrt(std::max(0.0f, radius * radius - distance * distance));
            }
            else if (radius > 0.0f && x > rightEdge - radius)
            {
                const float distance = x - (rightEdge - radius);
                inset = radius - std::sqrt(std::max(0.0f, radius * radius - distance * distance));
            }

            const float top = bounds.y + inset;
            const float bottom = bounds.y + bounds.height - inset;
            const float fringe = std::min(1.0f, std::max(0.0f, (bottom - top) * 0.5f));
            const float edgeFade = std::clamp(std::min(x - bounds.x, rightEdge - x), 0.0f, 1.0f);
            const float intensity = std::max(0.0f, 1.0f - std::abs(x - center) / width) * edgeFade;
            const D3DCOLOR sheenColor = WithAlpha(color, intensity);
            const float drawX = x - 0.5f;
            const float outerTop = top - 0.5f;
            const float innerTop = top + fringe - 0.5f;
            const float innerBottom = bottom - fringe - 0.5f;
            const float outerBottom = bottom - 0.5f;
            topFringe[topCount++] = {drawX, outerTop, 0.0f, 1.0f, transparent};
            topFringe[topCount++] = {drawX, innerTop, 0.0f, 1.0f, sheenColor};
            core[coreCount++] = {drawX, innerTop, 0.0f, 1.0f, sheenColor};
            core[coreCount++] = {drawX, innerBottom, 0.0f, 1.0f, sheenColor};
            bottomFringe[bottomCount++] = {drawX, innerBottom, 0.0f, 1.0f, sheenColor};
            bottomFringe[bottomCount++] = {drawX, outerBottom, 0.0f, 1.0f, transparent};
        }

        this->Submit(D3DPT_TRIANGLESTRIP, static_cast<UINT>(topCount - 2), topFringe.data(), sizeof(shape_vertex));
        this->Submit(D3DPT_TRIANGLESTRIP, static_cast<UINT>(coreCount - 2), core.data(), sizeof(shape_vertex));
        this->Submit(D3DPT_TRIANGLESTRIP, static_cast<UINT>(bottomCount - 2), bottomFringe.data(), sizeof(shape_vertex));
    }

    auto design_system::LerpColor(const D3DCOLOR start, const D3DCOLOR end, const float amount) -> D3DCOLOR
    {
        const float clamped = std::clamp(amount, 0.0f, 1.0f);
        const auto interpolate = [clamped](const uint32_t from, const uint32_t to) {
            return static_cast<uint32_t>(std::lround(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * clamped));
        };
        return D3DCOLOR_ARGB(
            interpolate((start >> 24) & 0xFF, (end >> 24) & 0xFF),
            interpolate((start >> 16) & 0xFF, (end >> 16) & 0xFF),
            interpolate((start >> 8) & 0xFF, (end >> 8) & 0xFF),
            interpolate(start & 0xFF, end & 0xFF));
    }

    auto design_system::Darken(const D3DCOLOR color, const float amount) -> D3DCOLOR
    {
        const auto red = static_cast<uint32_t>(static_cast<float>((color >> 16) & 0xFF) * amount);
        const auto green = static_cast<uint32_t>(static_cast<float>((color >> 8) & 0xFF) * amount);
        const auto blue = static_cast<uint32_t>(static_cast<float>(color & 0xFF) * amount);
        return D3DCOLOR_ARGB(alpha_of(color), red, green, blue);
    }

    auto design_system::WithAlpha(const D3DCOLOR color, const float amount) -> D3DCOLOR
    {
        const auto alpha = static_cast<uint32_t>(std::lround(static_cast<float>(alpha_of(color)) * std::clamp(amount, 0.0f, 1.0f)));
        return (color & 0x00FFFFFFu) | (alpha << 24);
    }

    auto design_system::SheenColor(const D3DCOLOR color, const float x, const float center, const float width) -> D3DCOLOR
    {
        if (width <= 0.0f)
            return color & 0x00FFFFFFu;
        const float intensity = std::max(0.0f, 1.0f - std::abs(x - center) / width);
        return WithAlpha(color, intensity);
    }
} // namespace xiui::ui
