#pragma once

#include "d3dUtil.h"

class FrameResource
{
  public:
	FrameResource() = default;
	FrameResource(const FrameResource &) = delete;
	FrameResource &operator=(const FrameResource &) = delete;
	~FrameResource();

	void Initialize(ID3D12Device *device, UINT objectConstantBufferByteSize, UINT shadowPassConstantBufferByteSize);

	ID3D12CommandAllocator *CommandAllocator() const
	{
		return m_commandAllocator.Get();
	}

	ID3D12Resource *ObjectConstantBuffer() const
	{
		return m_objectConstantBuffer.Get();
	}

	ID3D12Resource *ShadowPassConstantBuffer() const
	{
		return m_shadowPassConstantBuffer.Get();
	}

	UINT8 *MappedObjectConstantBuffer() const
	{
		return m_mappedObjectConstantBuffer;
	}

	UINT8 *MappedShadowPassConstantBuffer() const
	{
		return m_mappedShadowPassConstantBuffer;
	}

	UINT64 FenceValue = 0;

  private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_objectConstantBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowPassConstantBuffer;
	UINT8 *m_mappedObjectConstantBuffer = nullptr;
	UINT8 *m_mappedShadowPassConstantBuffer = nullptr;
};
