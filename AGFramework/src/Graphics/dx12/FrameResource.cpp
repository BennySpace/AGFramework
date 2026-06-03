#include "FrameResource.h"

FrameResource::~FrameResource()
{
	if (m_objectConstantBuffer != nullptr && m_mappedObjectConstantBuffer != nullptr)
	{
		m_objectConstantBuffer->Unmap(0, nullptr);
		m_mappedObjectConstantBuffer = nullptr;
	}

	if (m_shadowPassConstantBuffer != nullptr && m_mappedShadowPassConstantBuffer != nullptr)
	{
		m_shadowPassConstantBuffer->Unmap(0, nullptr);
		m_mappedShadowPassConstantBuffer = nullptr;
	}
}

void FrameResource::Initialize(ID3D12Device *device, UINT objectConstantBufferByteSize, UINT shadowPassConstantBufferByteSize)
{
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf())));

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
	                                             &CD3DX12_RESOURCE_DESC::Buffer(objectConstantBufferByteSize),
	                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                             IID_PPV_ARGS(m_objectConstantBuffer.GetAddressOf())));

	ThrowIfFailed(m_objectConstantBuffer->Map(0, nullptr, reinterpret_cast<void **>(&m_mappedObjectConstantBuffer)));

	ThrowIfFailed(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
	                                             &CD3DX12_RESOURCE_DESC::Buffer(shadowPassConstantBufferByteSize),
	                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                             IID_PPV_ARGS(m_shadowPassConstantBuffer.GetAddressOf())));

	ThrowIfFailed(m_shadowPassConstantBuffer->Map(0, nullptr, reinterpret_cast<void **>(&m_mappedShadowPassConstantBuffer)));
}
