#pragma once

#include <cstdint>

#include "dx12/d3dUtil.h"

class SpotShadowMap
{
public:
    struct Desc
    {
        UINT Width = 1024;
        UINT Height = 1024;
        DXGI_FORMAT ResourceFormat = DXGI_FORMAT_R32_TYPELESS;
        DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;
        DXGI_FORMAT SrvFormat = DXGI_FORMAT_R32_FLOAT;
        float ClearDepth = 1.0f;
    };

    bool Initialize(ID3D12Device* device, const Desc& desc);

    const Desc& GetDesc() const { return m_desc; }
    ID3D12Resource* GetResource() const { return m_shadowMap.Get(); }
    ID3D12DescriptorHeap* GetSrvHeap() const { return m_srvHeap.Get(); }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrv() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const;

    const D3D12_VIEWPORT& GetViewport() const { return m_viewport; }
    const D3D12_RECT& GetScissorRect() const { return m_scissorRect; }

    void Clear(ID3D12GraphicsCommandList* commandList) const;

private:
    void CreateDescriptorHeaps();
    void CreateResource();
    void CreateViews();

private:
    ID3D12Device* m_device = nullptr;
    Desc m_desc{};
    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowMap;
};
