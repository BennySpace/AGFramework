#include "DescriptorHeap.h"

#include <stdexcept>

void DescriptorHeap::Initialize(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT descriptorCount,
                                D3D12_DESCRIPTOR_HEAP_FLAGS flags)
{
	if (device == nullptr)
	{
		throw std::invalid_argument("DescriptorHeap requires a valid D3D12 device.");
	}

	if (descriptorCount == 0)
	{
		throw std::invalid_argument("DescriptorHeap requires at least one descriptor.");
	}

	m_type = type;
	m_flags = flags;
	m_capacity = descriptorCount;
	m_allocatedCount = 0;
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = descriptorCount;
	heapDesc.Type = type;
	heapDesc.Flags = flags;
	heapDesc.NodeMask = 0;
	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_heap.ReleaseAndGetAddressOf())));
}

void DescriptorHeap::Reset()
{
	m_allocatedCount = 0;
}

DescriptorHeap::Allocation DescriptorHeap::Allocate(UINT descriptorCount)
{
	if (m_heap == nullptr)
	{
		throw std::runtime_error("DescriptorHeap must be initialized before allocation.");
	}

	if (descriptorCount == 0 || m_allocatedCount + descriptorCount > m_capacity)
	{
		throw std::runtime_error("DescriptorHeap allocation exceeds capacity.");
	}

	Allocation allocation;
	allocation.Index = m_allocatedCount;
	allocation.Count = descriptorCount;
	allocation.CpuHandle = CpuHandleAt(allocation.Index);
	allocation.GpuHandle = GpuHandleAt(allocation.Index);
	m_allocatedCount += descriptorCount;
	return allocation;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::CpuHandleAt(UINT index) const
{
	if (m_heap == nullptr || index >= m_capacity)
	{
		throw std::runtime_error("DescriptorHeap CPU handle index is out of range.");
	}

	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptorSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GpuHandleAt(UINT index) const
{
	if (!IsShaderVisible())
	{
		return D3D12_GPU_DESCRIPTOR_HANDLE{};
	}

	if (m_heap == nullptr || index >= m_capacity)
	{
		throw std::runtime_error("DescriptorHeap GPU handle index is out of range.");
	}

	return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(index), m_descriptorSize);
}
