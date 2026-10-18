#pragma once

#include <d3d8.h>
#include <array>
#include <wrl/client.h>

namespace xiui
{
    class target_ring_draw_scope
    {
    public:
        explicit target_ring_draw_scope(IDirect3DDevice8* device) : device_(device)
        {
            for (DWORD stage=0;stage<textures_.size();++stage)
                if (FAILED(device_->GetTexture(stage,&textures_[stage]))) return;
            if (FAILED(device_->CreateStateBlock(D3DSBT_ALL,&state_))) return;
            captured_=true;
            device_->GetDepthStencilSurface(&depth_);
            ready_=SUCCEEDED(device_->GetRenderTarget(&target_)) &&
                SUCCEEDED(device_->GetStreamSource(0,&stream_,&stride_)) && SUCCEEDED(device_->GetIndices(&indices_,&base_)) &&
                SUCCEEDED(device_->GetTransform(D3DTS_WORLD,&world_)) && SUCCEEDED(device_->GetTransform(D3DTS_VIEW,&view_)) &&
                SUCCEEDED(device_->GetTransform(D3DTS_PROJECTION,&projection_)) && SUCCEEDED(device_->GetViewport(&viewport_));
        }
        ~target_ring_draw_scope()
        {
            if (!captured_) return;
            if (ready_)
            {
                Microsoft::WRL::ComPtr<IDirect3DSurface8> current,depth;
                const auto result=device_->GetRenderTarget(&current);
                device_->GetDepthStencilSurface(&depth);
                if (FAILED(result) || !Matches(current.Get(),depth.Get()))
                {
                    for (DWORD stage=0;stage<textures_.size();++stage) device_->SetTexture(stage,nullptr);
                    device_->SetRenderTarget(target_.Get(),depth_.Get());
                }
            }
            device_->ApplyStateBlock(state_); device_->DeleteStateBlock(state_);
            if (!ready_) return;
            for (DWORD stage=0;stage<textures_.size();++stage) device_->SetTexture(stage,textures_[stage].Get());
            device_->SetTransform(D3DTS_WORLD,&world_); device_->SetTransform(D3DTS_VIEW,&view_); device_->SetTransform(D3DTS_PROJECTION,&projection_);
            device_->SetViewport(&viewport_);
            // UP draws clear stream 0; state blocks do not reliably restore that implicit change.
            device_->SetStreamSource(0,stream_.Get(),stride_); device_->SetIndices(indices_.Get(),base_);
        }
        target_ring_draw_scope(const target_ring_draw_scope&)=delete;
        auto operator=(const target_ring_draw_scope&) -> target_ring_draw_scope& = delete;
        auto Ready(void) const -> bool { return ready_; }
        auto Matches(IDirect3DSurface8* target, IDirect3DSurface8* depth) const -> bool
        { return ready_ && target_.Get()==target && depth_.Get()==depth; }
    private:
        IDirect3DDevice8* device_{};
        DWORD state_{};
        bool captured_{},ready_{};
        Microsoft::WRL::ComPtr<IDirect3DSurface8> target_,depth_;
        std::array<Microsoft::WRL::ComPtr<IDirect3DBaseTexture8>,8> textures_;
        Microsoft::WRL::ComPtr<IDirect3DVertexBuffer8> stream_;
        Microsoft::WRL::ComPtr<IDirect3DIndexBuffer8> indices_;
        UINT stride_{},base_{};
        D3DMATRIX world_{},view_{},projection_{};
        D3DVIEWPORT8 viewport_{};
    };
}
