#pragma once

#include <cstdint>

#include "dx12/DescriptorHeap.h"
#include "dx12/d3dUtil.h"

class CascadedShadowMap
{
  public:
	struct Desc
	{
		UINT Width = 2048;
		UINT Height = 2048;
		std::uint32_t CascadeCount = 4;
		DXGI_FORMAT ResourceFormat = DXGI_FORMAT_R32_TYPELESS;
		DXGI_FORMAT DsvFormat = DXGI_FORMAT_D32_FLOAT;
		DXGI_FORMAT SrvFormat = DXGI_FORMAT_R32_FLOAT;
		float ClearDepth = 1.0f;
	};

	bool Initialize(ID3D12Device *device, const Desc &desc);

	const Desc &GetDesc() const
	{
		return m_desc;
	}
	ID3D12Resource *GetResource() const
	{
		return m_shadowArray.Get();
	}
	ID3D12DescriptorHeap *GetSrvHeap() const
	{
		return m_srvHeap.Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv(std::uint32_t cascadeIndex) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrv() const;

	const D3D12_VIEWPORT &GetViewport() const
	{
		return m_viewport;
	}
	const D3D12_RECT &GetScissorRect() const
	{
		return m_scissorRect;
	}

	void ClearCascade(ID3D12GraphicsCommandList *commandList, std::uint32_t cascadeIndex) const;

  private:
	void CreateDescriptorHeaps();
	void CreateResource();
	void CreateViews();

  private:
	ID3D12Device *m_device = nullptr;
	Desc m_desc{};
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	DescriptorHeap m_dsvHeap;
	DescriptorHeap m_srvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowArray;
};
