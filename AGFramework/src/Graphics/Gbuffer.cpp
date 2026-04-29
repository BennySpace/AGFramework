#include "Gbuffer.h"

#include <stdexcept>

bool Gbuffer::Initialize(ID3D12Device* device, const Desc& desc)
{
    if (device == nullptr)
    {
        return false;
    }

    if (desc.Width == 0 || desc.Height == 0)
    {
        return false;
    }

    m_device = device;
    m_desc = desc;
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CreateDescriptorHeaps();
    return CreateResources();
}

bool Gbuffer::Resize(UINT width, UINT height)
{
    if (m_device == nullptr || width == 0 || height == 0)
    {
        return false;
    }

    if (m_desc.Width == width && m_desc.Height == height)
    {
        return true;
    }

    m_desc.Width = width;
    m_desc.Height = height;
    return CreateResources();
}

void Gbuffer::Clear(ID3D12GraphicsCommandList* commandList) const
{
    if (commandList == nullptr)
    {
        return;
    }

    commandList->ClearRenderTargetView(GetRtv(Target::Albedo), m_desc.ClearColor, 0, nullptr);
    commandList->ClearRenderTargetView(GetRtv(Target::Normal), m_desc.ClearNormal, 0, nullptr);
    commandList->ClearRenderTargetView(GetRtv(Target::Position), m_desc.ClearPosition, 0, nullptr);
    commandList->ClearDepthStencilView(
        GetDsv(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        m_desc.ClearDepth,
        m_desc.ClearStencil,
        0,
        nullptr);
}

DXGI_FORMAT Gbuffer::GetFormat(Target target) const
{
    return ResolveTargetFormat(target);
}

ID3D12Resource* Gbuffer::GetResource(Target target) const
{
    return m_targets[GetTargetIndex(target)].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Gbuffer::GetRtv(Target target) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(static_cast<INT>(GetTargetIndex(target)), static_cast<INT>(m_rtvDescriptorSize));
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Gbuffer::GetSrv(Target target) const
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(static_cast<INT>(GetTargetIndex(target)), static_cast<INT>(m_srvDescriptorSize));
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Gbuffer::GetDsv() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE Gbuffer::GetSrvGpuHandle(Target target) const
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE handle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    handle.Offset(static_cast<INT>(GetTargetIndex(target)), static_cast<INT>(m_srvDescriptorSize));
    return handle;
}

bool Gbuffer::CreateResources()
{
    for (auto& target : m_targets)
    {
        target.Reset();
    }
    m_depthStencilBuffer.Reset();

    for (UINT i = 0; i < kTargetCount; ++i)
    {
        const auto target = static_cast<Target>(i);
        const auto targetDesc = CD3DX12_RESOURCE_DESC::Tex2D(
            ResolveTargetFormat(target),
            m_desc.Width,
            m_desc.Height,
            1,
            1,
            1,
            0,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

        const float* clearValue = m_desc.ClearColor;
        if (target == Target::Normal)
        {
            clearValue = m_desc.ClearNormal;
        }
        else if (target == Target::Position)
        {
            clearValue = m_desc.ClearPosition;
        }

        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = ResolveTargetFormat(target);
        optimizedClearValue.Color[0] = clearValue[0];
        optimizedClearValue.Color[1] = clearValue[1];
        optimizedClearValue.Color[2] = clearValue[2];
        optimizedClearValue.Color[3] = clearValue[3];

        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &targetDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &optimizedClearValue,
            IID_PPV_ARGS(m_targets[i].GetAddressOf())));
    }

    D3D12_RESOURCE_DESC depthDesc = {};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = m_desc.Width;
    depthDesc.Height = m_desc.Height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE depthClearValue = {};
    depthClearValue.Format = m_desc.DepthFormat;
    depthClearValue.DepthStencil.Depth = m_desc.ClearDepth;
    depthClearValue.DepthStencil.Stencil = m_desc.ClearStencil;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthClearValue,
        IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    CreateViews();
    return true;
}

void Gbuffer::CreateDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = kTargetCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = kTargetCount;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
}

void Gbuffer::CreateViews()
{
    for (UINT i = 0; i < kTargetCount; ++i)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = ResolveTargetFormat(static_cast<Target>(i));
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_device->CreateRenderTargetView(m_targets[i].Get(), &rtvDesc, GetRtv(static_cast<Target>(i)));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = ResolveTargetFormat(static_cast<Target>(i));
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        m_device->CreateShaderResourceView(m_targets[i].Get(), &srvDesc, GetSrv(static_cast<Target>(i)));
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_desc.DepthFormat;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, GetDsv());
}

DXGI_FORMAT Gbuffer::ResolveTargetFormat(Target target) const
{
    switch (target)
    {
    case Target::Albedo:
        return m_desc.AlbedoFormat;
    case Target::Normal:
        return m_desc.NormalFormat;
    case Target::Position:
        return m_desc.PositionFormat;
    default:
        throw std::runtime_error("Unknown gbuffer target.");
    }
}

UINT Gbuffer::GetTargetIndex(Target target) const
{
    return static_cast<UINT>(target);
}
