#include "ResourceUploader.h"

#include "../dx12/DirectX12Context.h"

void ResourceUploader::UploadTexture2D(
	DirectX12Context& context,
	Texture& texture,
	const void* pixelData,
	UINT width,
	UINT height,
	DXGI_FORMAT format)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		format,
		width,
		height,
		1,
		1);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture.Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Resource.Get(), 0, 1);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture.UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = pixelData;
	subresourceData.RowPitch = static_cast<LONG_PTR>(width * 4);
	subresourceData.SlicePitch = subresourceData.RowPitch * height;

	UpdateSubresources(
		context.GetCommandList(),
		texture.Resource.Get(),
		texture.UploadHeap.Get(),
		0,
		0,
		1,
		&subresourceData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.GetCommandList()->ResourceBarrier(1, &transition);
}

void ResourceUploader::UploadTextureCube(
	DirectX12Context& context,
	Texture& texture,
	const void* facePixelData,
	UINT faceWidth,
	UINT faceHeight,
	DXGI_FORMAT format)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		format,
		faceWidth,
		faceHeight,
		6,
		1);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture.Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Resource.Get(), 0, 6);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture.UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresources[6] = {};
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		subresources[faceIndex].pData = facePixelData;
		subresources[faceIndex].RowPitch = static_cast<LONG_PTR>(faceWidth * 4);
		subresources[faceIndex].SlicePitch = subresources[faceIndex].RowPitch * faceHeight;
	}

	UpdateSubresources(
		context.GetCommandList(),
		texture.Resource.Get(),
		texture.UploadHeap.Get(),
		0,
		0,
		6,
		subresources);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.GetCommandList()->ResourceBarrier(1, &transition);
}
