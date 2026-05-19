#include "CascadedShadowMap.h"

bool CascadedShadowMap::Initialize(ID3D12Device *device, const Desc &desc)
{
	if (device == nullptr || desc.Width == 0 || desc.Height == 0 || desc.CascadeCount == 0)
	{
		return false;
	}

	m_device = device;
	m_desc = desc;
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_viewport = {0.0f, 0.0f, static_cast<float>(m_desc.Width), static_cast<float>(m_desc.Height), 0.0f, 1.0f};
	m_scissorRect = {0, 0, static_cast<LONG>(m_desc.Width), static_cast<LONG>(m_desc.Height)};

	CreateDescriptorHeaps();
	CreateResource();
	CreateViews();
	return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE CascadedShadowMap::GetDsv(std::uint32_t cascadeIndex) const
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
	handle.Offset(static_cast<INT>(cascadeIndex), static_cast<INT>(m_dsvDescriptorSize));
	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE CascadedShadowMap::GetSrv() const
{
	return m_srvHeap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE CascadedShadowMap::GetSrvGpuHandle() const
{
	return m_srvHeap->GetGPUDescriptorHandleForHeapStart();
}

void CascadedShadowMap::ClearCascade(ID3D12GraphicsCommandList *commandList, std::uint32_t cascadeIndex) const
{
	if (commandList == nullptr || cascadeIndex >= m_desc.CascadeCount)
	{
		return;
	}

	commandList->ClearDepthStencilView(GetDsv(cascadeIndex), D3D12_CLEAR_FLAG_DEPTH, m_desc.ClearDepth, 0, 0, nullptr);
}

void CascadedShadowMap::ClearAll(ID3D12GraphicsCommandList *commandList) const
{
	for (std::uint32_t cascadeIndex = 0; cascadeIndex < m_desc.CascadeCount; ++cascadeIndex)
	{
		ClearCascade(commandList, cascadeIndex);
	}
}

void CascadedShadowMap::CreateDescriptorHeaps()
{
	m_dsvHeap.Reset();
	m_srvHeap.Reset();

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = m_desc.CascadeCount;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_srvHeap.GetAddressOf())));
}

void CascadedShadowMap::CreateResource()
{
	m_shadowArray.Reset();

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = m_desc.Width;
	resourceDesc.Height = m_desc.Height;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(m_desc.CascadeCount);
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = m_desc.ResourceFormat;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = m_desc.DsvFormat;
	clearValue.DepthStencil.Depth = m_desc.ClearDepth;
	clearValue.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &resourceDesc,
	                                                D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
	                                                IID_PPV_ARGS(m_shadowArray.GetAddressOf())));
}

void CascadedShadowMap::CreateViews()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_desc.SrvFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = 1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = m_desc.CascadeCount;
	srvDesc.Texture2DArray.PlaneSlice = 0;
	srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	m_device->CreateShaderResourceView(m_shadowArray.Get(), &srvDesc, GetSrv());

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < m_desc.CascadeCount; ++cascadeIndex)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = m_desc.DsvFormat;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		dsvDesc.Texture2DArray.MipSlice = 0;
		dsvDesc.Texture2DArray.FirstArraySlice = cascadeIndex;
		dsvDesc.Texture2DArray.ArraySize = 1;
		m_device->CreateDepthStencilView(m_shadowArray.Get(), &dsvDesc, GetDsv(cascadeIndex));
	}
}
