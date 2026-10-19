#include "minimap_module.hpp"
#include "ui/focus_frame.hpp"
#include "ui/contour.hpp"

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace xiui
{
    auto minimap_module::ApplyTextDefaults(const ui::theme& theme) -> void
    {
        this->text_ = {theme.caption_text}; this->text_.style.scale = 0.8f; this->text_.style.alignment = ui::text_alignment::center;
        this->zoom_text_ = this->text_; this->zoom_text_.style.alignment = ui::text_alignment::right;
    }

    auto minimap_module::ResetDefaults(const ui::theme& theme) -> void
    {
        const auto bounds = this->state_.bounds; const auto* data = this->data_; auto* maps = this->maps_;
        *this = {}; this->state_.bounds = bounds; this->data_ = data; this->maps_ = maps; this->ApplyTextDefaults(theme);
    }

    auto minimap_module::Load(const settings_store& store) -> void
    {
        this->state_.enabled = store.ReadBool("minimap", "enabled", true);
        this->state_.bounds = store.ReadRect("layout.minimap", this->state_.bounds);
        this->state_.bounds.width = std::clamp(this->state_.bounds.width, 120.0f, 600.0f);
        this->shape_ = std::clamp(store.ReadInt("minimap", "shape", 0), 0, 2);
        this->artwork_ = std::clamp(store.ReadInt("minimap", "artwork", 1), 0, 1);
        this->zoom_ = this->default_zoom_ = std::clamp(store.ReadFloat("minimap", "zoom", 2), 1.0f, 8.0f);
        this->map_opacity_ = std::clamp(store.ReadFloat("minimap", "map_opacity", 1), 0.0f, 1.0f);
        this->background_opacity_ = std::clamp(store.ReadFloat("minimap", "background_opacity", 0.8f), 0.0f, 1.0f);
        this->frame_opacity_ = std::clamp(store.ReadFloat("minimap", "frame_opacity", 1), 0.0f, 1.0f);
        this->marker_size_ = std::clamp(store.ReadFloat("minimap", "marker_size", 4), 2.0f, 12.0f);
        this->marker_opacity_ = std::clamp(store.ReadFloat("minimap", "marker_opacity", 1), 0.0f, 1.0f);
        this->frame_ = store.ReadBool("minimap", "frame", true);
        this->background_ = store.ReadBool("minimap", "background", true);
        this->party_ = store.ReadBool("minimap", "party", true);
        this->alliance_ = store.ReadBool("minimap", "alliance", true);
        this->pet_ = store.ReadBool("minimap", "pet", true);
        this->target_ = store.ReadBool("minimap", "target", true);
        this->zone_name_ = store.ReadBool("minimap", "zone_name", true);
        this->coordinates_ = store.ReadBool("minimap", "coordinates", true);
        this->map_number_ = store.ReadBool("minimap", "map_number", this->coordinates_);
        this->zoom_readout_ = store.ReadBool("minimap", "zoom_readout", true);
        this->selected_text_ = std::clamp(store.ReadInt("minimap", "selected_text", 0), 0, 1);
        this->north_ = store.ReadBool("minimap", "north", true);
        this->vision_cone_ = store.ReadBool("minimap", "vision_cone", false);
        this->npcs_ = store.ReadBool("minimap", "npcs", true);
        this->distant_party_ = store.ReadBool("minimap", "distant_party", true);
        static constexpr const char* edgeKeys[]{"coordinate_edge_left", "coordinate_edge_top", "coordinate_edge_right", "coordinate_edge_bottom"};
        for (size_t i = 0; i < this->coordinate_edges_.size(); ++i)
            this->coordinate_edges_[i] = store.ReadBool("minimap", edgeKeys[i], true);
        this->cone_angle_ = std::clamp(store.ReadFloat("minimap", "cone_angle", 80), 20.0f, 160.0f);
        this->cone_size_ = std::clamp(store.ReadFloat("minimap", "cone_size", 0.9f), 0.2f, 1.0f);
        this->cone_opacity_ = std::clamp(store.ReadFloat("minimap", "cone_opacity", 0.2f), 0.0f, 1.0f);
        this->edge_style_ = std::clamp(store.ReadInt("minimap", "edge_style", store.ReadBool("minimap", "rounded_corners", false) ? 1 : 0), 0, 2);
        this->shadow_ = store.ReadBool("minimap", "shadow", false);
        const auto read = [&](const char* key, const float fallback, const float minimum, const float maximum) {
            const auto value = store.ReadFloat("minimap", key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
        };
        this->corner_radius_ = read("corner_radius", 12, 1, 48);
        this->shadow_opacity_ = read("shadow_opacity", 0.45f, 0, 1);
        this->shadow_softness_ = read("shadow_softness", 6, 0, 20);
        this->shadow_x_ = read("shadow_x", 2, -20, 20);
        this->shadow_y_ = read("shadow_y", 3, -20, 20);
        this->zoom_zone_ = 0;
        this->zone_zoom_.fill(0);
        for (size_t chunk = 0; chunk < 32; ++chunk)
        {
            std::istringstream values{store.ReadString("minimap.zoom", std::to_string(chunk).c_str())};
            float value{};
            for (size_t slot = 0; slot < 32 && values >> value; ++slot)
                this->zone_zoom_[chunk * 32 + slot] = std::isfinite(value) && value > 0 ? std::clamp(value, 1.0f, 8.0f) : 0;
        }
        this->text_ = LoadTextProfile(store, "text.minimap.labels", this->text_);
        auto zoomDefaults = this->text_;
        zoomDefaults.placement = 0; zoomDefaults.style.alignment = ui::text_alignment::right;
        this->zoom_text_ = LoadTextProfile(store, "text.minimap.zoom", zoomDefaults);
        const auto zoomOffset = [&](const char* key, const float fallback) {
            const float value = store.ReadFloat("text.minimap.zoom", key, fallback);
            return std::isfinite(value) ? std::clamp(value, -600.0f, 600.0f) : fallback;
        };
        this->zoom_text_.offset_x = zoomOffset("offset_x", zoomDefaults.offset_x);
        this->zoom_text_.offset_y = zoomOffset("offset_y", zoomDefaults.offset_y);
    }

    auto minimap_module::Save(const settings_store& store) const -> void
    {
        store.WriteBool("minimap", "enabled", this->state_.enabled); store.WriteRect("layout.minimap", this->state_.bounds);
        store.WriteBool("minimap", "distant_party", this->distant_party_);
        static constexpr const char* edgeKeys[]{"coordinate_edge_left", "coordinate_edge_top", "coordinate_edge_right", "coordinate_edge_bottom"};
        for (size_t i = 0; i < this->coordinate_edges_.size(); ++i)
            store.WriteBool("minimap", edgeKeys[i], this->coordinate_edges_[i]);
        store.WriteInt("minimap", "shape", this->shape_); store.WriteInt("minimap", "artwork", this->artwork_);
        store.WriteFloat("minimap", "zoom", this->default_zoom_);
        store.WriteFloat("minimap", "map_opacity", this->map_opacity_); store.WriteFloat("minimap", "background_opacity", this->background_opacity_);
        store.WriteFloat("minimap", "frame_opacity", this->frame_opacity_);
        store.WriteFloat("minimap", "marker_size", this->marker_size_); store.WriteFloat("minimap", "marker_opacity", this->marker_opacity_);
        store.WriteBool("minimap", "frame", this->frame_); store.WriteBool("minimap", "background", this->background_);
        store.WriteBool("minimap", "party", this->party_); store.WriteBool("minimap", "alliance", this->alliance_);
        store.WriteBool("minimap", "pet", this->pet_); store.WriteBool("minimap", "target", this->target_);
        store.WriteBool("minimap", "zone_name", this->zone_name_); store.WriteBool("minimap", "coordinates", this->coordinates_); store.WriteBool("minimap", "north", this->north_);
        store.WriteBool("minimap", "map_number", this->map_number_);
        store.WriteBool("minimap", "zoom_readout", this->zoom_readout_);
        store.WriteInt("minimap", "selected_text", this->selected_text_);
        store.WriteBool("minimap", "vision_cone", this->vision_cone_); store.WriteBool("minimap", "npcs", this->npcs_);
        store.WriteFloat("minimap", "cone_angle", this->cone_angle_); store.WriteFloat("minimap", "cone_size", this->cone_size_);
        store.WriteFloat("minimap", "cone_opacity", this->cone_opacity_);
        store.WriteInt("minimap", "edge_style", this->edge_style_);
        store.WriteBool("minimap", "rounded_corners", this->edge_style_ == 1); store.WriteBool("minimap", "shadow", this->shadow_);
        store.WriteFloat("minimap", "corner_radius", this->corner_radius_);
        store.WriteFloat("minimap", "shadow_opacity", this->shadow_opacity_); store.WriteFloat("minimap", "shadow_softness", this->shadow_softness_);
        store.WriteFloat("minimap", "shadow_x", this->shadow_x_); store.WriteFloat("minimap", "shadow_y", this->shadow_y_);
        for (size_t chunk = 0; chunk < 32; ++chunk)
        {
            std::ostringstream values;
            for (size_t slot = 0; slot < 32; ++slot) values << this->zone_zoom_[chunk * 32 + slot] << ' ';
            store.WriteString("minimap.zoom", std::to_string(chunk).c_str(), values.str());
        }
        SaveTextProfile(store, "text.minimap.labels", this->text_);
        SaveTextProfile(store, "text.minimap.zoom", this->zoom_text_);
    }

    auto minimap_module::OnOptionChanged(const size_t index) -> void
    {
        if (index == zoom_option)
        {
            if (this->zoom_zone_ > 0 && this->zoom_zone_ < this->zone_zoom_.size()) this->zone_zoom_[this->zoom_zone_] = this->zoom_;
            else this->default_zoom_ = this->zoom_;
        }
    }

    auto minimap_module::UpdateZoomZone(void) -> void
    {
        if (this->data_ == nullptr || !this->data_->available || this->data_->zone == this->zoom_zone_) return;
        this->zoom_zone_ = this->data_->zone;
        this->zoom_ = this->zoom_zone_ < this->zone_zoom_.size() && this->zone_zoom_[this->zoom_zone_] > 0 ? this->zone_zoom_[this->zoom_zone_] : this->default_zoom_;
    }

    auto minimap_module::HandleMouseWheel(const int delta, const float x, const float y, bool& save) -> bool
    {
        save = false;
        if (!this->state_.enabled || delta == 0) return false;
        const ui::rect view{this->state_.bounds.x, this->state_.bounds.y, this->state_.bounds.width, MinimapHeight(this->state_.bounds.width, this->shape_)};
        if (!MinimapContains(view, this->shape_, {x, y}, 0, this->edge_style_ == 1 ? this->corner_radius_ : 0)) return false;
        if (this->edge_style_ == 2 && this->shape_ != 1 && this->maps_ != nullptr && this->data_ != nullptr && this->data_->available)
        {
            const auto image = this->maps_->Acquire(this->data_->zone, this->data_->floor, this->artwork_ == 1);
            if (image.outline && !ui::ContourContains(view, *image.outline, {x, y})) return false;
        }
        this->UpdateZoomZone();
        const auto option = this->Option(zoom_option);
        const float zoom = std::clamp(this->zoom_ + static_cast<float>(delta) / WHEEL_DELTA * option.step, option.minimum, option.maximum);
        if (zoom != this->zoom_)
        {
            this->zoom_ = zoom;
            this->OnOptionChanged(zoom_option);
            save = true;
        }
        return true;
    }

    auto minimap_module::ConfigSections(void) const -> std::span<const module_config_section>
    {
        static constexpr module_config_section sections[]{
            {"General", "Map artwork, shape, zoom and pinned coordinate edges. Enhanced maps use Remapster art (CC BY-SA 4.0).", "general"},
            {"Appearance", "Map dimensions, Straight/Rounded/Burnt edges, drop shadow, background, frame and opacity.", "appearance"},
            {"Markers", "Facing cone, green NPC dots, party, alliance, pet and selected target.", "markers"},
            {"Text", "Choose Labels or Zoom to edit visibility, position offsets and text styling independently.", "text"}};
        return sections;
    }

    auto minimap_module::ConfigSectionContains(const size_t section, const size_t index) -> bool
    {
        if (index >= this->OptionCount()) return false;
        if (index == coordinate_edges_option) return section == 0;
        if (index == distant_party_option) return section == 2;
        if (index >= map_number_option || index == zone_name_option || index == coordinates_option) return section == 3;
        if (index >= edge_style_option) return section == 1;
        if (index >= vision_cone_option) return section == 2;
        if (index >= marker_size_option && index <= target_option || index == north_option) return section == 2;
        if (index == width_option || index >= frame_option && index <= background_opacity_option || index == frame_opacity_option) return section == 1;
        return section == 0;
    }

    auto minimap_module::Option(const size_t index) -> module_option
    {
        static constexpr const char* shapes[]{"Square", "Circle", "Wide (16:9)"};
        static constexpr const char* artwork[]{"Regular", "Enhanced (Remapster)"};
        static constexpr const char* textElements[]{"Labels", "Zoom"};
        static constexpr const char* edgeNames[]{"Left", "Top", "Right", "Bottom"};
        static constexpr const char* edgeStyles[]{"Straight", "Rounded", "Burnt"};
        const bool burnt = this->edge_style_ == 2 && this->shape_ != 1;
        switch (index)
        {
        case enabled_option: return module_option::Toggle("Enabled", "Show an independent minimap centered on your position.", &this->state_.enabled).Ordered(-50);
        case shape_option: return module_option::Choice("Shape", "Choose Square, Circle or a wide 16:9 map. Artwork keeps its proportions.", &this->shape_, shapes, 3);
        case artwork_option: return module_option::Choice("Map artwork", "Enhanced uses Remapster maps, with Regular art where no enhanced map is available.", &this->artwork_, artwork, 2);
        case width_option: return module_option::Slider("Width", "Map width; height follows the selected shape.", &this->state_.bounds.width, 120, 600, 1).Ordered(110);
        case zoom_option: return module_option::Slider("Zoom", "Magnification remembered separately for each zone. Scroll over the map to zoom.", &this->zoom_, 1, 8, 0.25f);
        case frame_option: return module_option::Toggle("Frame", "Native window frame, or a matching rim for Circle and rounded maps.", &this->frame_).VisibleWhen(!burnt);
        case background_option: return module_option::Toggle("Background", "Dark backing under map artwork and uncovered map edges.", &this->background_);
        case map_opacity_option: return module_option::Slider("Map opacity", "Map artwork opacity.", &this->map_opacity_, 0, 1, 0.05f);
        case background_opacity_option: return module_option::Slider("Background opacity", "Dark backing opacity.", &this->background_opacity_, 0, 1, 0.05f).VisibleWhen(this->background_);
        case marker_size_option: return module_option::Slider("Marker size", "Size of map dots and the player arrow.", &this->marker_size_, 2, 12, 0.5f);
        case marker_opacity_option: return module_option::Slider("Marker opacity", "Player arrow and map marker opacity.", &this->marker_opacity_, 0, 1, 0.05f);
        case party_option: return module_option::Toggle("Party", "Show party members on your map and floor.", &this->party_);
        case alliance_option: return module_option::Toggle("Alliance", "Show alliance members on your map and floor.", &this->alliance_);
        case pet_option: return module_option::Toggle("Pet", "Show your pet on your map and floor.", &this->pet_);
        case target_option: return module_option::Toggle("Target", "Show the current target on your map and floor.", &this->target_);
        case zone_name_option: return module_option::Toggle("Zone name", "Show the zone name below the map.", &this->zone_name_).VisibleWhen(this->selected_text_ == 0);
        case coordinates_option: return module_option::Toggle("Coordinates", "Show your map grid in parentheses, for example (G-8).", &this->coordinates_).VisibleWhen(this->selected_text_ == 0);
        case north_option: return module_option::Toggle("North indicator", "Show N at the top of this north-up map.", &this->north_);
        case frame_opacity_option: return module_option::Slider("Frame opacity", "Frame or circle rim opacity.", &this->frame_opacity_, 0, 1, 0.05f).VisibleWhen(this->frame_ && !burnt);
        case vision_cone_option: return module_option::Toggle("Vision cone", "Show a fading cone in the direction your character faces.", &this->vision_cone_).Ordered(100);
        case cone_angle_option: return module_option::Slider("Cone angle", "Opening angle in degrees.", &this->cone_angle_, 20, 160, 5).Ordered(110).VisibleWhen(this->vision_cone_);
        case cone_size_option: {
            auto option = module_option::Slider("Cone size (%)", "Size of the cone inside the map.", &this->cone_size_, 0.2f, 1, 0.05f).Ordered(120);
            option.display_scale = 100; return option.VisibleWhen(this->vision_cone_);
        }
        case cone_opacity_option: return module_option::Slider("Cone opacity", "Strength of the cone, fading toward its outer edge.", &this->cone_opacity_, 0, 1, 0.05f).Ordered(130).VisibleWhen(this->vision_cone_);
        case npcs_option: return module_option::Toggle("NPCs", "Show nearby visible NPCs as green dots on your current map and floor.", &this->npcs_);
        case edge_style_option: {
            auto option = module_option::Choice("Edge style", "Straight, Rounded or a burnt parchment outline with transparent cutouts and no coordinate strips. Circle keeps its shape.", &this->edge_style_, edgeStyles, 3).Ordered(120);
            return option.VisibleWhen(this->shape_ != 1);
        }
        case corner_radius_option: {
            auto option = module_option::Slider("Corner radius", "Corner rounding in pixels for the image, backing and frame.", &this->corner_radius_, 1, 48, 1).Ordered(130);
            return option.VisibleWhen(this->edge_style_ == 1 && this->shape_ != 1);
        }
        case shadow_option: return module_option::Toggle("Drop shadow", "Draw a soft shadow following the map shape.", &this->shadow_).Ordered(300);
        case shadow_opacity_option: {
            auto option = module_option::Slider("Shadow opacity", "Darkness of the map shadow.", &this->shadow_opacity_, 0, 1, 0.05f).Ordered(330);
            return option.VisibleWhen(this->shadow_);
        }
        case shadow_softness_option: {
            auto option = module_option::Slider("Shadow softness", "Softness of the shadow edge in pixels.", &this->shadow_softness_, 0, 20, 1).Ordered(320);
            return option.VisibleWhen(this->shadow_);
        }
        case shadow_x_option: {
            auto option = module_option::Slider("Shadow X", "Horizontal shadow offset.", &this->shadow_x_, -20, 20, 1).Ordered(310, true);
            return option.VisibleWhen(this->shadow_);
        }
        case shadow_y_option: {
            auto option = module_option::Slider("Shadow Y", "Vertical shadow offset.", &this->shadow_y_, -20, 20, 1).Ordered(311);
            return option.VisibleWhen(this->shadow_);
        }
        case map_number_option: return module_option::Toggle("Map number", "Show the native map label below the map, for example Map 1.", &this->map_number_).VisibleWhen(this->selected_text_ == 0);
        case text_selector_option: return module_option::Element("Selected text", "Choose map labels or the independently positioned zoom readout.", &this->selected_text_, textElements, 2);
        case zoom_readout_option: return module_option::Toggle("Zoom readout", "Show the current magnification. X/Y offsets move it relative to the map's bottom-right corner.", &this->zoom_readout_).Ordered(90).VisibleWhen(this->selected_text_ == 1);
        case distant_party_option: return module_option::Toggle("Distant party markers", "Refresh missing same-zone party/alliance map positions every ten seconds. Snapshots expire after twenty seconds.", &this->distant_party_);
        case coordinate_edges_option: return module_option::ToggleButtons("Coordinate edges", "Toggle Left, Top, Right and Bottom independently. Edges stay aligned and scaled with map zoom.", this->coordinate_edges_, edgeNames).Ordered(100).VisibleWhen(!burnt);
        default: {
            auto option = TextProfileOption(index - text_profile_begin, this->selected_text_ == 1 ? this->zoom_text_ : this->text_);
            if (this->selected_text_ == 1 && (index == text_x_option || index == text_y_option)) { option.minimum = -600; option.maximum = 600; }
            return option.VisibleWhen(this->selected_text_ == 0 || this->zoom_readout_);
        }
        }
    }

    auto minimap_module::Render(const hud_snapshot&, const module_render_context& context) -> void
    {
        const auto& design = context.Design(); const auto& colors = design.GetTheme().colors;
        const bool live = this->data_ != nullptr && this->data_->available;
        this->UpdateZoomZone();
        const float width = this->state_.bounds.width, height = MinimapHeight(width, this->shape_);
        const float lineHeight = std::max(20.0f, design.TextHeight(this->text_.style) + 4);
        this->state_.bounds.height = height + ((this->zone_name_ || this->coordinates_ || this->map_number_) ? lineHeight : 0);
        const ui::rect view{this->state_.bounds.x, this->state_.bounds.y, width, height};
        const ui::point center{view.x + width / 2, view.y + height / 2};
        const uint16_t zone = live ? this->data_->zone : 100;
        const int floor = live ? this->data_->floor : 0;
        const auto image = this->maps_ != nullptr && (live || context.IsEditing()) ? this->maps_->Acquire(zone, floor, this->artwork_ == 1) : minimap_image{};
        const bool burntStyle = this->edge_style_ == 2 && this->shape_ != 1;
        const bool burnt = burntStyle && image.texture != nullptr && image.outline != nullptr;
        const auto coordinateEdges = burntStyle ? std::array<bool, 4>{} : this->coordinate_edges_;
        const bool drawFrame = this->frame_ && !burntStyle;
        const float cornerRadius = MinimapCornerRadius(view, this->shape_, this->edge_style_ == 1 ? this->corner_radius_ : 0);
        const auto alpha = [](const uint32_t color, const float opacity) { return (color & 0xFFFFFF) | (static_cast<uint32_t>((color >> 24) * opacity) << 24); };
        if (this->shadow_ && this->shadow_opacity_ > 0 && burnt)
        {
            const float spread = this->shadow_softness_;
            const ui::rect shadow{view.x + this->shadow_x_ - spread, view.y + this->shadow_y_ - spread, width + spread * 2, height + spread * 2};
            design.DrawContour(shadow, *image.outline, {alpha(0xFF000000, this->shadow_opacity_), 0, 0, std::max(1.0f, spread)});
        }
        else if (this->shadow_ && this->shadow_opacity_ > 0)
        {
            ui::focus_frame_style shadow{};
            shadow.color = 0; shadow.glow = false; shadow.padding = 0; shadow.thickness = 4; shadow.radius = cornerRadius;
            shadow.shadow_color = 0xFF000000; shadow.shadow_opacity = this->shadow_opacity_;
            shadow.shadow_size = this->shadow_softness_; shadow.shadow_x = this->shadow_x_; shadow.shadow_y = this->shadow_y_;
            design.DrawFocusFrame(view, shadow);
        }
        const auto player = live ? this->data_->player : ui::point{0.5f, 0.5f};
        const float size = std::min(width, height) * this->zoom_;
        const ui::rect mapBounds{center.x - player.x * size, center.y - player.y * size, size, size};
        const float edgeInset = size * minimap_coordinate_band + (drawFrame ? 4 : 0);
        const ui::rect mapContent{view.x + (coordinateEdges[0] ? edgeInset : 0), view.y + (coordinateEdges[1] ? edgeInset : 0),
            width - (coordinateEdges[0] + coordinateEdges[2]) * edgeInset,
            height - (coordinateEdges[1] + coordinateEdges[3]) * edgeInset};
        const auto drawMap = [&](const ui::rect& contour, const ui::rect& uv, const float radius) {
            const auto tint = alpha(0xFFFFFFFF, this->map_opacity_);
            if (burnt) design.DrawContour(contour, *image.outline, {tint, alpha(0xFF482717, this->map_opacity_), 2, 1}, image.texture, uv);
            else if (this->shape_ == 1) design.DrawTextureDisc(image.texture, contour, uv, tint);
            else if (radius > 0) design.DrawTextureRoundedRect(image.texture, contour, uv, radius, tint);
            else design.DrawTexture(image.texture, contour, tint, uv);
        };
        {
            const auto clipContent = design.Clip(image.texture != nullptr ? mapContent : view);
            if (this->background_)
            {
                const auto color = alpha(0xFF10121C, this->background_opacity_);
                if (burnt) design.DrawContour(view, *image.outline, {color});
                else
                {
                    const auto backing = image.texture != nullptr ? mapContent : view;
                    design.FillRoundedRect(backing, color, MinimapCornerRadius(backing, this->shape_, cornerRadius));
                }
            }
            if (image.texture != nullptr)
            {
                const auto clipView = design.Clip(view);
                const float band = size * minimap_coordinate_band;
                const auto crop = burntStyle ? std::array<bool, 4>{true, true, true, true} : coordinateEdges;
                const ui::rect artworkBounds{mapBounds.x + (crop[0] ? band : 0), mapBounds.y + (crop[1] ? band : 0),
                    size - (crop[0] + crop[2]) * band, size - (crop[1] + crop[3]) * band};
                const auto clipImage = design.Clip(artworkBounds);
                const ui::rect uv{(view.x - mapBounds.x) / size, (view.y - mapBounds.y) / size, width / size, height / size};
                drawMap(view, uv, cornerRadius);
            }
            if (image.texture == nullptr)
            {
                const ui::rect message{view.x + 8, center.y - lineHeight / 2, width - 16, lineHeight};
                context.DrawText(image.loading ? "Loading map..." : "Map unavailable", message, message, this->text_);
            }
            if (this->vision_cone_ && image.texture != nullptr && (live || context.IsEditing()))
            {
                auto arc = MinimapVisionArc(view, live ? this->data_->heading : -1.570796327f, this->cone_size_, this->cone_angle_);
                if (burnt)
                    for (auto& point : arc)
                        if (!ui::ContourContains(view, *image.outline, point, 2))
                        {
                            float low{}, high{1};
                            for (int step = 0; step < 10; ++step)
                            {
                                const float middle = (low + high) / 2;
                                const ui::point at{center.x + (point.x - center.x) * middle, center.y + (point.y - center.y) * middle};
                                if (ui::ContourContains(view, *image.outline, at, 2)) low = middle; else high = middle;
                            }
                            point = {center.x + (point.x - center.x) * low, center.y + (point.y - center.y) * low};
                        }
                design.DrawTriangleFan(center, arc, alpha(0xFFB5DCFF, this->cone_opacity_), 0x00B5DCFF);
            }
            if (image.texture != nullptr && live && this->marker_opacity_ > 0)
            {
                std::array<ui::circle_fill, 64> dots;
                size_t dotCount{};
                const auto flushDots = [&] {
                    design.FillCircles(std::span{dots}.first(dotCount)); dotCount = 0;
                };
                for (const auto& marker : this->data_->markers)
                {
                    if (marker.kind == minimap_marker_kind::party && !this->party_ || marker.kind == minimap_marker_kind::alliance && !this->alliance_ ||
                        marker.kind == minimap_marker_kind::pet && !this->pet_ || marker.kind == minimap_marker_kind::target && !this->target_ ||
                        marker.kind == minimap_marker_kind::npc && !this->npcs_) continue;
                    const ui::point point{mapBounds.x + marker.uv.x * size, mapBounds.y + marker.uv.y * size};
                    const float radius = this->marker_size_ * (marker.kind == minimap_marker_kind::target ? 1.25f : 1);
                    if (!MinimapContains(view, this->shape_, point, radius + 2, cornerRadius)) continue;
                    if (burnt && !ui::ContourContains(view, *image.outline, point, radius + 3)) continue;
                    if (marker.kind == minimap_marker_kind::target)
                    {
                        flushDots();
                        design.DrawRing({point.x - radius - 2, point.y - radius - 2, radius * 2 + 4, radius * 2 + 4}, alpha(colors.text, this->marker_opacity_), 1);
                    }
                    dots[dotCount++] = {point, radius, alpha(0xFF000000, this->marker_opacity_)};
                    dots[dotCount++] = {point, radius - 1, alpha(marker.color, this->marker_opacity_)};
                    if (dotCount == dots.size()) flushDots();
                }
                flushDots();
            }
            if (image.texture != nullptr && (live || context.IsEditing()))
            {
                const float heading = live ? this->data_->heading : -1.570796327f;
                const float dx = std::cos(heading), dy = std::sin(heading), r = this->marker_size_ * 2;
                const auto arrow = [&](const float scale, const uint32_t color) {
                    design.DrawTriangle(center.x + dx * r * scale, center.y + dy * r * scale,
                        center.x - dx * r * 0.6f * scale - dy * r * 0.6f * scale, center.y - dy * r * 0.6f * scale + dx * r * 0.6f * scale,
                        center.x - dx * r * 0.6f * scale + dy * r * 0.6f * scale, center.y - dy * r * 0.6f * scale - dx * r * 0.6f * scale, alpha(color, this->marker_opacity_));
                };
                arrow(1.3f, 0xFF000000); arrow(1, 0xFFFFFFFF);
            }
        }
        if (image.texture != nullptr)
        {
            const float inset = drawFrame ? 4.0f : 0.0f;
            const ui::rect edgeView{view.x + inset, view.y + inset, width - inset * 2, height - inset * 2};
            const float band = size * minimap_coordinate_band;
            // Horizontal strips own the corners so adjacent coordinate labels never overlap.
            for (const size_t edge : {size_t{1}, size_t{3}, size_t{0}, size_t{2}})
            {
                if (!coordinateEdges[edge]) continue;
                auto stripView = edgeView;
                if (edge == 0 || edge == 2)
                {
                    stripView.y += coordinateEdges[1] ? band : 0;
                    stripView.height -= (coordinateEdges[1] + coordinateEdges[3]) * band;
                }
                const auto strip = MinimapCoordinateStrip(stripView, mapBounds, static_cast<minimap_coordinate_edge>(edge));
                if (strip.bounds.width <= 0 || strip.bounds.height <= 0) continue;
                const auto clip = design.Clip(strip.bounds);
                const ui::rect uv{strip.uv.x - (strip.bounds.x - edgeView.x) / size, strip.uv.y - (strip.bounds.y - edgeView.y) / size,
                    edgeView.width / size, edgeView.height / size};
                drawMap(edgeView, uv, std::max(0.0f, cornerRadius - inset));
            }
        }
        if (drawFrame)
        {
            if (this->shape_ == 1)
            {
                design.DrawRing(view, alpha(colors.surface_raised, this->frame_opacity_), 4);
                design.DrawRing({view.x + 1, view.y + 1, width - 2, height - 2}, alpha(colors.muted, this->frame_opacity_), 1);
            }
            else if (cornerRadius > 0)
            {
                ui::focus_frame_style rim{};
                rim.padding = 0; rim.glow = rim.shadow = false; rim.radius = cornerRadius; rim.thickness = 4;
                rim.color = alpha(colors.surface_raised, this->frame_opacity_); design.DrawFocusFrame(view, rim);
                rim.radius = std::max(0.0f, cornerRadius - 1); rim.thickness = 1; rim.color = alpha(colors.muted, this->frame_opacity_);
                design.DrawFocusFrame({view.x + 1, view.y + 1, width - 2, height - 2}, rim);
            }
            else context.DrawContainerFrame(view, {this->frame_opacity_, 4, 0, false, true});
        }
        if (this->north_)
        {
            const float inset = burntStyle ? std::min(width, height) * minimap_coordinate_band + 4 : 0;
            const ui::rect north{center.x - 12, view.y + (coordinateEdges[1] ? edgeInset : inset) + 5, 24, lineHeight};
            context.DrawText("N", north, north, this->text_);
        }
        if (this->zoom_readout_)
        {
            char zoom[24]{};
            std::snprintf(zoom, sizeof(zoom), "%.2fx", this->zoom_);
            auto profile = this->zoom_text_;
            profile.style.tabular_numbers = true;
            const float inset = burntStyle ? std::min(width, height) * minimap_coordinate_band + 6 : 6 + cornerRadius * (1 - 0.70710678f);
            const float bandInset = edgeInset + 2;
            const float insetX = coordinateEdges[2] ? std::max(inset, bandInset) : inset;
            const float insetY = coordinateEdges[3] ? std::max(inset, bandInset) : inset;
            const float textHeight = std::max(20.0f, design.TextHeight(profile.style) + 4);
            const float textWidth = std::min(design.MeasureText(zoom, profile.style) + 1, width - insetX * 2);
            const ui::rect label{view.x + width - insetX - textWidth, view.y + height - insetY - textHeight, textWidth, textHeight};
            const ui::rect anchor{view.x + insetX, label.y, width - insetX * 2, textHeight};
            context.DrawText(zoom, label, anchor, profile);
        }
        if (this->zone_name_ || this->coordinates_ || this->map_number_)
        {
            std::string caption = this->zone_name_ ? (live ? this->data_->name : "Minimap") : "";
            if (this->coordinates_ && live)
            {
                const int column = static_cast<int>(std::floor((player.x * 256 - 8) / 16)), row = static_cast<int>(std::floor((player.y * 256 - 8) / 16));
                char grid[48]{};
                if (column >= 0 && column < 15 && row >= 0 && row < 15)
                    std::snprintf(grid, sizeof(grid), "(%c-%d)", 'A' + column, row + 1);
                if (grid[0] != '\0')
                {
                    if (!caption.empty()) caption += "  "; caption += grid;
                }
            }
            if (this->map_number_ && live)
            {
                char number[24]{};
                std::snprintf(number, sizeof(number), "Map %d", this->data_->map_number);
                if (!caption.empty()) caption += "  "; caption += number;
            }
            const ui::rect line{view.x, view.y + height, width, lineHeight};
            context.DrawText(caption.c_str(), line, line, this->text_);
        }
    }
}
