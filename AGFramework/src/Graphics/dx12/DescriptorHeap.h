#pragma once

#include "d3dUtil.h"

class DescriptorHeap
{
  public:
	struct Allocation
	{
		D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle{};
		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle{};
		UINT Index = 0;
		UINT Count = 0;
	};

	DescriptorHeap() = default;

	void Initialize(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
	void Reset();

	Allocation Allocate(UINT descriptorCount = 1);
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleAt(UINT index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleAt(UINT index) const;

	ID3D12DescriptorHeap *Get() const
	{
		return m_heap.Get();
	}

	UINT DescriptorSize() const
	{
		return m_descriptorSize;
	}

	UINT Capacity() const
	{
		return m_capacity;
	}

	UINT AllocatedCount() const
	{
		return m_allocatedCount;
	}

	bool IsValid() const
	{
		return m_heap != nullptr;
	}

  private:
	bool IsShaderVisible() const
	{
		return (m_flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0;
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	D3D12_DESCRIPTOR_HEAP_FLAGS m_flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	UINT m_descriptorSize = 0;
	UINT m_capacity = 0;
	UINT m_allocatedCount = 0;
};
