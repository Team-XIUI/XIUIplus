#include "target_rings.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include "target_ring_draw_scope.hpp"
#include "entities/entity_name_colors.hpp"

namespace xiui
{
    auto target_ring_settings::Load(const settings_store& store) -> void
    {
        this->enabled = store.ReadBool("target.rings", "enabled", true);
        this->hp_fill = store.ReadBool("target.rings", "hp_fill", true);
        this->follow_name_color = store.ReadBool("target.rings", "follow_name_color", true);
        this->show_npcs = store.ReadBool("target.rings", "show_npcs", false);
        this->show_players = store.ReadBool("target.rings", "show_players", false);
        const auto radius = [&](const char* key, float fallback, float minimum) {
            const float value = store.ReadFloat("target.rings", key, fallback);
            return std::isfinite(value) ? std::clamp(value, minimum, 10.0f) : fallback;
        };
        this->outer_radius = radius("outer_radius", 1.4f, 0.1f);
        this->inner_radius = std::min(radius("inner_radius", 1.15f, 0.05f), this->outer_radius - 0.05f);
        this->color = store.ReadArgb("target.rings", "color", 0xD060E8F0);
    }

    auto target_ring_settings::Save(const settings_store& store) const -> void
    {
        store.WriteBool("target.rings", "enabled", this->enabled);
        store.WriteBool("target.rings", "hp_fill", this->hp_fill);
        store.WriteBool("target.rings", "follow_name_color", this->follow_name_color);
        store.WriteBool("target.rings", "show_npcs", this->show_npcs);
        store.WriteBool("target.rings", "show_players", this->show_players);
        store.WriteFloat("target.rings", "inner_radius", this->inner_radius);
        store.WriteFloat("target.rings", "outer_radius", this->outer_radius);
        store.WriteArgb("target.rings", "color", this->color);
    }

    auto target_ring_settings::Changed(size_t index) -> void
    {
        if (index == 2) this->outer_radius = std::max(this->outer_radius, this->inner_radius + 0.05f);
        if (index == 3) this->inner_radius = std::min(this->inner_radius, this->outer_radius - 0.05f);
    }

    auto target_ring_settings::Option(size_t index) -> module_option
    {
        switch (index)
        {
        case 0: return module_option::Toggle("Enabled", "Draw a terrain-following ring beneath the current target.", &this->enabled).Ordered(-50);
        case 1: return module_option::Toggle("HP fill", "The band fills clockwise with the enemy's actual HP percentage. Turn off for a full decorative ring.", &this->hp_fill);
        case 2: return module_option::Slider("Inner radius", "Radius of the empty center, in world units (yalms).", &this->inner_radius, 0.05f, 9.95f, 0.05f).Ordered(100, true);
        case 3: return module_option::Slider("Outer radius", "Outside radius of the band, in world units (yalms).", &this->outer_radius, 0.1f, 10, 0.05f).Ordered(101);
        case 4: return module_option::Color("Ring color", "Opacity always applies. Custom RGB applies when Follow target name color is off.", &this->color).Ordered(500, true);
        case 5: return module_option::Toggle("Follow target name color", "Match native name and claim colors, including your shared entity-color overrides. Keeps the configured ring opacity.", &this->follow_name_color).Ordered(490);
        case 6: return module_option::Toggle("Show for NPCs", "Also show the ring when targeting an NPC. NPCs without HP use a full decorative band.", &this->show_npcs).Ordered(10);
        default: return module_option::Toggle("Show for players", "Also show the ring when targeting yourself, party/alliance members or other players.", &this->show_players).Ordered(11);
        }
    }

    auto target_ring_settings::Allows(uint32_t spawn_flags) const -> bool
    {
        const auto kind=ClassifyEntity(spawn_flags,0,false,false);
        return IsMonster(kind) || (kind==entity_class::npc && this->show_npcs) ||
            ((kind==entity_class::player || kind==entity_class::group_player) && this->show_players);
    }

    auto target_ring_settings::Color(D3DCOLOR name_color) const -> D3DCOLOR
    { return this->follow_name_color ? (this->color&0xFF000000)|(name_color&0xFFFFFF) : this->color; }

    namespace
    {
        constexpr float tau = std::numbers::pi_v<float> * 2;
        auto Cross(const ground_point& a, const ground_point& b, const ground_point& p) -> float
        { return (b[0]-a[0])*(p[2]-a[2]) - (b[2]-a[2])*(p[0]-a[0]); }
        auto Alpha(D3DCOLOR color, float amount) -> D3DCOLOR
        { return (color & 0xFFFFFF) | (static_cast<DWORD>((color >> 24) * amount) << 24); }

        auto ProjectPolygon(const zone_ground& ground, const std::vector<uint32_t>& nearby,
            const ground_point& center, float reach, std::span<const ground_point> shape,
            D3DCOLOR color, std::vector<target_ring_renderer::vertex>& vertices) -> void
        {
            const float winding = Cross(shape[0], shape[1], shape[2]) < 0 ? -1.0f : 1.0f;
            float x0=shape[0][0], x1=x0, z0=shape[0][2], z1=z0;
            for (const auto& p : shape) { x0=std::min(x0,p[0]); x1=std::max(x1,p[0]); z0=std::min(z0,p[2]); z1=std::max(z1,p[2]); }
            for (auto index : nearby)
            {
                const auto& triangle = ground.triangles[index];
                if (std::min({triangle[0][0],triangle[1][0],triangle[2][0]}) > x1 ||
                    std::max({triangle[0][0],triangle[1][0],triangle[2][0]}) < x0 ||
                    std::min({triangle[0][2],triangle[1][2],triangle[2][2]}) > z1 ||
                    std::max({triangle[0][2],triangle[1][2],triangle[2][2]}) < z0) continue;
                std::array<ground_point, 12> polygon{}, clipped{};
                std::copy(triangle.begin(), triangle.end(), polygon.begin());
                size_t count = 3;
                for (size_t edge=0; edge<shape.size() && count>=3; ++edge)
                {
                    size_t output=0;
                    const auto &a=shape[edge], &b=shape[(edge+1)%shape.size()];
                    auto previous=polygon[count-1]; float dp=Cross(a,b,previous)*winding;
                    for (size_t p=0; p<count; ++p)
                    {
                        const auto current=polygon[p]; const float dc=Cross(a,b,current)*winding;
                        if ((dc>=0) != (dp>=0))
                        {
                            const float fraction=dp/(dp-dc);
                            ground_point hit{};
                            for (unsigned c=0; c<3; ++c) hit[c]=previous[c]+(current[c]-previous[c])*fraction;
                            clipped[output++]=hit;
                        }
                        if (dc>=0) clipped[output++]=current;
                        previous=current; dp=dc;
                    }
                    count=output; polygon=clipped;
                }
                if (count<3) continue;
                ground_point middle{};
                for (size_t p=0; p<count; ++p) for (unsigned c=0; c<3; ++c) middle[c]+=polygon[p][c]/static_cast<float>(count);
                float floor{};
                if (!ground.Height(middle[0],middle[2],center[1],reach,floor) || std::abs(floor-middle[1])>0.015f) continue;
                for (size_t p=1; p+1<count; ++p)
                {
                    if (std::abs(Cross(polygon[0],polygon[p],polygon[p+1]))<1e-8f) continue;
                    for (size_t v : {size_t{0},p,p+1})
                        vertices.push_back({polygon[v][0],polygon[v][1]-0.018f,polygon[v][2],color});
                }
            }
        }
    }

    auto target_ring_renderer::Build(const zone_ground& ground, const ground_point& center, float hp,
        const target_ring_settings& settings, std::vector<vertex>& vertices, float yaw) -> void
    {
        vertices.clear();
        if (!std::isfinite(settings.inner_radius) || !std::isfinite(settings.outer_radius) ||
            settings.inner_radius < 0.05f || settings.outer_radius <= settings.inner_radius || settings.outer_radius > 10) return;
        for (float v : center) if (!std::isfinite(v)) return;
        std::vector<uint32_t> nearby;
        ground.Nearby(center,settings.outer_radius+0.4f,nearby);
        const float reach=0.75f+settings.outer_radius*1.5f;
        const auto point = [&](float radius, float angle) -> ground_point {
            return {center[0]+std::cos(angle)*radius,center[1],center[2]+std::sin(angle)*radius};
        };
        const auto band = [&](float inner, float outer, float start, float end, D3DCOLOR color) {
            constexpr float step=tau/96;
            for (float angle=start; angle<end-1e-6f; angle+=step)
            {
                const float next=std::min(angle+step,end);
                const std::array shape{point(inner,angle),point(outer,angle),point(outer,next),point(inner,next)};
                ProjectPolygon(ground,nearby,center,reach,shape,color,vertices);
            }
        };
        const float inner=settings.inner_radius, outer=settings.outer_radius;
        const float trim=std::min(0.025f,(outer-inner)*0.2f);
        const auto color=settings.color;
        band(outer,outer+0.045f,0,tau,Alpha(color,0.12f));
        band(outer+0.045f,outer+0.09f,0,tau,Alpha(color,0.04f));
        band(inner,outer,0,tau,Alpha(color,0.14f));
        const float fill=settings.hp_fill ? std::clamp(hp,0.0f,1.0f) : 1.0f;
        band(inner,outer,-tau*0.25f,-tau*0.25f+tau*fill,Alpha(color,0.72f));
        band(inner,inner+trim,0,tau,Alpha(color,0.8f));
        band(outer-trim,outer,0,tau,color);
        const float facing=std::isfinite(yaw) ? -yaw : 0;
        const std::array marker{point(outer+0.23f,facing),point(outer-0.08f,facing-0.09f),point(outer-0.08f,facing+0.09f)};
        ProjectPolygon(ground,nearby,center,reach,marker,color,vertices);
    }

    auto target_ring_renderer::Project(std::span<const vertex> vertices, const hud_snapshot& snapshot, std::vector<screen_vertex>& screen,
        double depth_unit) -> void
    {
        screen.clear();
        using point=std::array<double,4>;
        const auto transform=[](const point& p,const D3DMATRIX& matrix) {
            point result{};
            for (unsigned c=0;c<4;++c) for (unsigned r=0;r<4;++r) result[c]+=p[r]*matrix.m[r][c];
            return result;
        };
        const auto distance=[](const point& p,unsigned plane) {
            switch (plane) {
            case 0: return p[3]-0.0001;
            case 1: return p[0]+p[3]; case 2: return p[3]-p[0];
            case 3: return p[1]+p[3]; case 4: return p[3]-p[1];
            case 5: return p[2]; default: return p[3]-p[2];
            }
        };
        const auto& viewport=snapshot.scene_viewport;
        for (size_t t=0;t+2<vertices.size();t+=3)
        {
            std::array<point,16> polygon{},clipped{};
            bool finite=true;
            for (unsigned v=0;v<3;++v) {
                const auto& p=vertices[t+v];
                polygon[v]=transform(transform({p.x,p.y,p.z,1},snapshot.scene_view),snapshot.scene_projection);
                for (double value : polygon[v]) finite &= std::isfinite(value);
            }
            if (!finite) continue;
            size_t count=3;
            for (unsigned plane=0;plane<7 && count>=3;++plane)
            {
                size_t output=0; auto previous=polygon[count-1]; double dp=distance(previous,plane);
                for (size_t v=0;v<count;++v) {
                    const auto current=polygon[v]; const double dc=distance(current,plane);
                    if ((dc>=0)!=(dp>=0)) {
                        point hit{}; const double fraction=dp/(dp-dc);
                        for (unsigned c=0;c<4;++c) hit[c]=previous[c]+(current[c]-previous[c])*fraction;
                        clipped[output++]=hit;
                    }
                    if (dc>=0) clipped[output++]=current;
                    previous=current; dp=dc;
                }
                count=output; polygon=clipped;
            }
            for (size_t v=1;v+1<count;++v) {
                std::array<point,3> projected{};
                double cap=1;
                const double dw=0.02*std::abs(snapshot.scene_projection._34);
                const std::array indices{size_t{0},v,v+1};
                for (size_t i=0;i<3;++i) {
                    const auto& p=polygon[indices[i]]; const double rhw=1/p[3];
                    projected[i]={viewport.X+(p[0]*rhw+1)*viewport.Width*0.5,
                        viewport.Y+(1-p[1]*rhw)*viewport.Height*0.5,
                        viewport.MinZ+p[2]*rhw*(viewport.MaxZ-viewport.MinZ),rhw};
                    // Bound the decal offset to two centimeters toward the camera at every vertex.
                    const double limit=p[3]>dw && std::abs(snapshot.scene_projection._44)<1e-8 ?
                        (viewport.MaxZ-viewport.MinZ)*std::abs(snapshot.scene_projection._43)*dw/(p[3]*(p[3]-dw)) : 0;
                    cap=std::min(cap,limit);
                }
                const auto &a=projected[0], &b=projected[1], &c=projected[2];
                const double dx1=b[0]-a[0], dy1=b[1]-a[1], dz1=b[2]-a[2];
                const double dx2=c[0]-a[0], dy2=c[1]-a[1], dz2=c[2]-a[2];
                const double area=dx1*dy2-dx2*dy1;
                double bias=2*depth_unit;
                if (std::abs(area)>1e-10)
                    bias+=std::max(std::abs((dz1*dy2-dz2*dy1)/area),std::abs((dx1*dz2-dx2*dz1)/area));
                bias=std::clamp(bias,0.0,cap);
                for (const auto& p : projected)
                    screen.push_back({static_cast<float>(p[0]),static_cast<float>(p[1]),
                        static_cast<float>(std::max(static_cast<double>(viewport.MinZ),p[2]-bias)),static_cast<float>(p[3]),vertices[t].color});
            }
        }
    }

    target_ring_renderer::~target_ring_renderer()
    {
        if (this->cancel_) *this->cancel_=true;
        if (this->loading_.valid()) this->loading_.wait();
    }

    auto target_ring_renderer::Status(void) const -> std::string
    {
        char text[512]{};
        std::snprintf(text,sizeof(text),"Target rings: %s; zone %u; floors %zu; vertices %zu; target %u/%08X HP %u; camera %s; draw %08lX.",
            this->stage_,this->zone_,this->ground_ ? this->ground_->triangles.size() : 0,this->vertices_.size(),
            this->current_index_,this->current_id_,this->current_hp_,
            this->camera_valid_ ? "ready" : "missing",static_cast<unsigned long>(this->draw_result_));
        return text;
    }

    auto target_ring_renderer::UpdateGround(uint16_t zone) -> void
    {
        if (this->zone_ != zone)
        {
            this->zone_=zone; this->ground_.reset(); this->mesh_valid_=false;
            if (this->cancel_) *this->cancel_=true;
        }
        if (this->loading_.valid())
        {
            if (this->loading_.wait_for(std::chrono::seconds{0}) != std::future_status::ready) return;
            try { auto ground=this->loading_.get(); if (this->loading_zone_==zone && !*this->cancel_) this->ground_=std::move(ground); }
            catch (...) { if (this->loading_zone_==zone) this->ground_=std::make_unique<zone_ground>(); }
        }
        if (this->ground_ || zone==0) return;
        wchar_t path[MAX_PATH]{};
        const auto module=::GetModuleHandleW(L"FFXiMain.dll");
        if (!module || !::GetModuleFileNameW(module,path,MAX_PATH)) return;
        const auto root=std::filesystem::path{path}.parent_path();
        this->loading_zone_=zone;
        this->cancel_=std::make_shared<std::atomic_bool>(false);
        this->loading_=std::async(std::launch::async,[zone,root,cancel=this->cancel_] { return LoadZoneGround(zone,root,*cancel); });
    }

    auto target_ring_renderer::Draw(IAshitaCore* core, IDirect3DDevice8* device, const hud_snapshot& snapshot, const target_ring_settings& settings) -> void
    {
        this->stage_="disabled";
        this->current_id_=snapshot.target_server_id; this->current_hp_=snapshot.target_hp_percent; this->camera_valid_=snapshot.camera_valid;
        if (!settings.enabled || !core || !device) return;
        auto* memory=core->GetMemoryManager();
        auto* party=memory ? memory->GetParty() : nullptr;
        auto* player=memory ? memory->GetPlayer() : nullptr;
        auto* entity=memory ? memory->GetEntity() : nullptr;
        auto* target=memory ? memory->GetTarget() : nullptr;
        const uint16_t zone=party && player && !player->GetIsZoning() ? party->GetMemberZone(0) : 0;
        this->UpdateGround(zone);
        this->stage_=!zone ? "zoning" : !this->ground_ ? "loading terrain" : !snapshot.camera_valid ? "missing camera" : "no living target";
        if (!zone || !this->ground_ || !entity || !target || !snapshot.camera_valid || !snapshot.target_valid) return;
        const auto index=target->GetTargetIndex(0);
        this->current_index_=index; this->spawn_flags_=index<entity->GetEntityMapSize() ? entity->GetSpawnFlags(index) : 0;
        this->render_flags_=index<entity->GetEntityMapSize() ? entity->GetRenderFlags0(index) : 0;
        this->stage_="target type disabled or not visible";
        if (index==0 || index>=entity->GetEntityMapSize() || entity->GetServerId(index)!=snapshot.target_server_id ||
            !settings.Allows(this->spawn_flags_) || (entity->GetRenderFlags0(index)&0x4200)!=0x200) return;
        const bool hasHealth=EntityHasHealth(this->spawn_flags_);
        if (hasHealth && snapshot.target_hp_percent==0) { this->stage_="target is dead"; return; }
        auto appearance=settings;
        appearance.color=settings.Color(ReadEntityNameColor(*party,*entity,index));
        const ground_point center{entity->GetLocalPositionX(index),entity->GetLocalPositionZ(index),entity->GetLocalPositionY(index)};
        const auto now=::GetTickCount64();
        if (!this->mesh_valid_ || this->last_id_!=snapshot.target_server_id || now-this->hp_animation_.last_tick>500)
            this->hp_animation_={};
        const float hp=this->hp_animation_.Update(hasHealth ? snapshot.target_hp_percent/100.0f : 1.0f,now);
        const float nativeYaw=entity->GetLocalPositionYaw(index);
        const float yaw=std::isfinite(nativeYaw) ? nativeYaw : 0;
        if (!this->mesh_valid_ || this->last_id_!=snapshot.target_server_id || this->last_center_!=center ||
            (settings.hp_fill && this->last_hp_!=hp) || this->last_yaw_!=yaw || this->last_settings_.inner_radius!=settings.inner_radius ||
            this->last_settings_.outer_radius!=settings.outer_radius || this->last_settings_.hp_fill!=settings.hp_fill || this->last_settings_.color!=appearance.color)
        {
            Build(*this->ground_,center,hp,appearance,this->vertices_,yaw);
            this->last_center_=center; this->last_id_=snapshot.target_server_id; this->last_hp_=hp; this->last_yaw_=yaw;
            this->last_settings_=appearance; this->mesh_valid_=true;
        }
        this->stage_="no floor under target";
        if (this->vertices_.empty()) return;
        D3DSURFACE_DESC depth{};
        if (snapshot.scene_depth) snapshot.scene_depth->GetDesc(&depth);
        const double depthUnit=depth.Format==D3DFMT_D16 || depth.Format==D3DFMT_D16_LOCKABLE ? 1.0/65535.0 :
            depth.Format==D3DFMT_D32 ? 1.0/4294967295.0 : 1.0/16777215.0;
        Project(this->vertices_,snapshot,this->screen_vertices_,depthUnit);
        if (this->screen_vertices_.empty()) { this->stage_="outside camera"; return; }
        this->stage_="capturing device state";
        target_ring_draw_scope saved{device};
        if (!saved.Ready()) return;
        if (!snapshot.scene_target || !snapshot.scene_depth) { this->stage_="missing scene surfaces"; return; }
        for (DWORD stage=0;stage<8;++stage) device->SetTexture(stage,nullptr);
        if (!saved.Matches(snapshot.scene_target,snapshot.scene_depth)) {
            this->draw_result_=device->SetRenderTarget(snapshot.scene_target,snapshot.scene_depth);
            if (FAILED(this->draw_result_)) { this->stage_="scene target unavailable"; return; }
        }
        device->SetViewport(&snapshot.scene_viewport);
        device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE); device->SetPixelShader(0);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
        device->SetTextureStageState(0,D3DTSS_RESULTARG,D3DTA_CURRENT);
        for (DWORD stage=1;stage<8;++stage) {
            device->SetTextureStageState(stage,D3DTSS_COLOROP,D3DTOP_DISABLE);
            device->SetTextureStageState(stage,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
        }
        device->SetRenderState(D3DRS_LIGHTING,FALSE); device->SetRenderState(D3DRS_FOGENABLE,FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE,FALSE); device->SetRenderState(D3DRS_COLORVERTEX,TRUE);
        device->SetRenderState(D3DRS_CLIPPING,TRUE); device->SetRenderState(D3DRS_ZBIAS,0);
        device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE); device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
        device->SetRenderState(D3DRS_SHADEMODE,D3DSHADE_GOURAUD); device->SetRenderState(D3DRS_CLIPPLANEENABLE,0);
        device->SetRenderState(D3DRS_VERTEXBLEND,D3DVBF_DISABLE); device->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE,FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE,FALSE); device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        device->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE); device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE); device->SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE); device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        device->SetRenderState(D3DRS_COLORWRITEENABLE,0xF);
        this->draw_result_=device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,static_cast<UINT>(this->screen_vertices_.size()/3),this->screen_vertices_.data(),sizeof(screen_vertex));
        this->stage_=SUCCEEDED(this->draw_result_) ? "submitted" : "draw failed";
    }
}
