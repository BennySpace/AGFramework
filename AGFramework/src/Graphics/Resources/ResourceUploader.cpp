#include "ResourceUploader.h"

#include "../dx12/DirectX12Context.h"

#include <stdexcept>

namespace
{
std::string WStringToUtf8(const std::wstring &value)
{
	if (value.empty())
	{
		return std::string();
	}

	const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(sizeRequired > 0 ? sizeRequired - 1 : 0, '\0');
	if (sizeRequired > 1)
	{
		WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], sizeRequired - 1, nullptr, nullptr);
	}

	return result;
}

bool FileExists(const std::wstring &filename)
{
	if (filename.empty())
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(filename.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
} // namespace

void ResourceUploader::UploadTexture2D(DirectX12Context &context, Texture &texture, const void *pixelData, UINT width, UINT height,
                                       DXGI_FORMAT format)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
	const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
	                                                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
	                                                           IID_PPV_ARGS(&texture.Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Resource.Get(), 0, 1);
	const auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
	                                                           D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                                           IID_PPV_ARGS(&texture.UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = pixelData;
	subresourceData.RowPitch = static_cast<LONG_PTR>(width * 4);
	subresourceData.SlicePitch = subresourceData.RowPitch * height;

	UpdateSubresources(context.GetCommandList(), texture.Resource.Get(), texture.UploadHeap.Get(), 0, 0, 1, &subresourceData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
	                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.GetCommandList()->ResourceBarrier(1, &transition);
}

void ResourceUploader::UploadSceneTexture(DirectX12Context &context, Texture &texture, const TextureLoader::SceneTextureSource &source,
                                          DXGI_FORMAT format)
{
	texture.Filename = source.Filename;

	if (source.SourceKind == TextureLoader::SceneTextureSource::Kind::DdsFile)
	{
		if (!FileExists(source.Filename))
		{
			throw std::runtime_error("Required DDS texture asset not found: " + WStringToUtf8(source.Filename));
		}

		ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(context.GetDevice(), context.GetCommandList(), source.Filename.c_str(),
		                                                  texture.Resource, texture.UploadHeap));
		return;
	}

	UploadTexture2D(context, texture, source.DecodedImage.Pixels.data(), source.DecodedImage.Width, source.DecodedImage.Height, format);
}

void ResourceUploader::UploadTextureCube(DirectX12Context &context, Texture &texture, const void *facePixelData, UINT faceWidth,
                                         UINT faceHeight, DXGI_FORMAT format)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(format, faceWidth, faceHeight, 6, 1);
	const CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
	const CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
	                                                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
	                                                           IID_PPV_ARGS(&texture.Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Resource.Get(), 0, 6);
	const auto uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc,
	                                                           D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
	                                                           IID_PPV_ARGS(&texture.UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresources[6] = {};
	for (UINT faceIndex = 0; faceIndex < 6; ++faceIndex)
	{
		subresources[faceIndex].pData = facePixelData;
		subresources[faceIndex].RowPitch = static_cast<LONG_PTR>(faceWidth * 4);
		subresources[faceIndex].SlicePitch = subresources[faceIndex].RowPitch * faceHeight;
	}

	UpdateSubresources(context.GetCommandList(), texture.Resource.Get(), texture.UploadHeap.Get(), 0, 0, 6, subresources);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture.Resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
	                                                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	context.GetCommandList()->ResourceBarrier(1, &transition);
}
