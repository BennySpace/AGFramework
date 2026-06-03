#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "dx12/DescriptorHeap.h"
#include "dx12/d3dUtil.h"

class Gbuffer
{
  public:
	enum class Target : std::uint32_t
	{
		Albedo = 0,
		Normal = 1,
		Material = 2,
		Count
	};

	struct Desc
	{
		UINT Width = 0;
		UINT Height = 0;
		DXGI_FORMAT AlbedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT NormalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT MaterialFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		DXGI_FORMAT DepthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		float ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		float ClearNormal[4] = {0.5f, 0.5f, 1.0f, 0.0f};
		float ClearMaterial[4] = {0.0f, 0.5f, 1.0f, 1.0f};
		float ClearDepth = 1.0f;
		std::uint8_t ClearStencil = 0;
	};

	Gbuffer() = default;

	bool Initialize(ID3D12Device *device, const Desc &desc);
	bool Resize(UINT width, UINT height);
	void Clear(ID3D12GraphicsCommandList *commandList) const;

	UINT Width() const
	{
		return m_desc.Width;
	}
	UINT Height() const
	{
		return m_desc.Height;
	}

	DXGI_FORMAT GetFormat(Target target) const;
	ID3D12Resource *GetResource(Target target) const;
	ID3D12Resource *GetDepthResource() const
	{
		return m_depthStencilBuffer.Get();
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetRtv(Target target) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrv(Target target) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetDsv() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(Target target) const;

	ID3D12DescriptorHeap *GetSrvHeap() const
	{
		return m_srvHeap.Get();
	}
	ID3D12DescriptorHeap *GetRtvHeap() const
	{
		return m_rtvHeap.Get();
	}
	ID3D12DescriptorHeap *GetDsvHeap() const
	{
		return m_dsvHeap.Get();
	}

  private:
	static constexpr UINT kTargetCount = static_cast<UINT>(Target::Count);

	bool CreateResources();
	void CreateDescriptorHeaps();
	void CreateViews();
	DXGI_FORMAT ResolveTargetFormat(Target target) const;
	UINT GetTargetIndex(Target target) const;

  private:
	ID3D12Device *m_device = nullptr;
	Desc m_desc{};

	DescriptorHeap m_rtvHeap;
	DescriptorHeap m_srvHeap;
	DescriptorHeap m_dsvHeap;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> m_targets;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
};
