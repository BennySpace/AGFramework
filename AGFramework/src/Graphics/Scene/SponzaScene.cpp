#include "SponzaScene.h"

#include "../ObjModelLoader.h"
#include "../dx12/DirectX12Context.h"

#include <limits>
#include <stdexcept>

using namespace DirectX;

namespace
{
	struct TgaTextureData
	{
		UINT Width = 0;
		UINT Height = 0;
		std::vector<std::uint8_t> Pixels;
	};

	std::wstring ResolveAssetPath(const std::wstring& assetRelativePath)
	{
		const std::wstring candidates[] =
		{
			assetRelativePath,
			L"..\\" + assetRelativePath,
			L"..\\..\\" + assetRelativePath,
			L"AGFramework\\" + assetRelativePath
		};

		for (const std::wstring& candidate : candidates)
		{
			const DWORD attributes = GetFileAttributesW(candidate.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return candidate;
			}
		}

		return assetRelativePath;
	}

	std::uint16_t ReadUInt16LE(const std::uint8_t* bytes)
	{
		return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
	}

	std::string WStringToString(const std::wstring& wideString)
	{
		if (wideString.empty())
		{
			return std::string();
		}

		const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string result(sizeRequired > 0 ? sizeRequired - 1 : 0, '\0');

		if (sizeRequired > 1)
		{
			WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &result[0], sizeRequired - 1, nullptr, nullptr);
		}

		return result;
	}

	TgaTextureData LoadUncompressedTga(const std::wstring& filename)
	{
		std::ifstream input(filename, std::ios::binary);
		if (!input)
		{
			throw std::runtime_error("Failed to open TGA texture file.");
		}

		std::array<std::uint8_t, 18> header{};
		input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
		if (!input)
		{
			throw std::runtime_error("Failed to read TGA header.");
		}

		const std::uint8_t idLength = header[0];
		const std::uint8_t colorMapType = header[1];
		const std::uint8_t imageType = header[2];
		const std::uint16_t width = ReadUInt16LE(&header[12]);
		const std::uint16_t height = ReadUInt16LE(&header[14]);
		const std::uint8_t bitsPerPixel = header[16];
		const std::uint8_t imageDescriptor = header[17];

		if (colorMapType != 0 || imageType != 2)
		{
			throw std::runtime_error("Only uncompressed true-color TGA textures are supported.");
		}

		if (bitsPerPixel != 24 && bitsPerPixel != 32)
		{
			throw std::runtime_error("Unsupported TGA pixel format.");
		}

		if (idLength > 0)
		{
			input.seekg(idLength, std::ios::cur);
		}

		const UINT srcPixelSize = bitsPerPixel / 8;
		const size_t srcDataSize = static_cast<size_t>(width) * height * srcPixelSize;
		std::vector<std::uint8_t> srcPixels(srcDataSize);
		input.read(reinterpret_cast<char*>(srcPixels.data()), static_cast<std::streamsize>(srcDataSize));
		if (!input)
		{
			throw std::runtime_error("Failed to read TGA pixel data.");
		}

		TgaTextureData textureData;
		textureData.Width = width;
		textureData.Height = height;
		textureData.Pixels.resize(static_cast<size_t>(width) * height * 4);

		const bool topLeftOrigin = (imageDescriptor & 0x20) != 0;

		for (UINT y = 0; y < height; ++y)
		{
			const UINT srcY = topLeftOrigin ? y : (height - 1 - y);
			for (UINT x = 0; x < width; ++x)
			{
				const size_t srcIndex = (static_cast<size_t>(srcY) * width + x) * srcPixelSize;
				const size_t dstIndex = (static_cast<size_t>(y) * width + x) * 4;

				textureData.Pixels[dstIndex + 0] = srcPixels[srcIndex + 2];
				textureData.Pixels[dstIndex + 1] = srcPixels[srcIndex + 1];
				textureData.Pixels[dstIndex + 2] = srcPixels[srcIndex + 0];
				textureData.Pixels[dstIndex + 3] = (srcPixelSize == 4) ? srcPixels[srcIndex + 3] : 255;
			}
		}

		return textureData;
	}
}

void SponzaScene::Initialize(DirectX12Context& context)
{
	BuildGeometry(context);
	BuildTextures(context);
	BuildDescriptorHeap(context);
}

void SponzaScene::DisposeUploaders()
{
	if (m_geometry)
	{
		m_geometry->DisposeUploaders();
	}

	for (auto& textureEntry : m_textures)
	{
		textureEntry.second->UploadHeap.Reset();
	}
}

void SponzaScene::BuildGeometry(DirectX12Context& context)
{
	const std::wstring modelPath = ResolveAssetPath(L"Assets\\sponza\\sponza.obj");
	std::vector<ObjModelLoader::MeshData> meshes = ObjModelLoader().Load(WStringToString(modelPath));
	if (meshes.empty())
	{
		throw std::runtime_error("No meshes were loaded from the OBJ model.");
	}

	std::vector<GeometryGenerator::Vertex> vertices;
	std::vector<std::uint32_t> indices;
	m_drawItems.clear();

	XMFLOAT3 minPoint(
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)());
	XMFLOAT3 maxPoint(
		-(std::numeric_limits<float>::max)(),
		-(std::numeric_limits<float>::max)(),
		-(std::numeric_limits<float>::max)());

	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const ObjModelLoader::MeshData& mesh = meshes[meshIndex];
		vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
		indices.insert(indices.end(), mesh.Indices32.begin(), mesh.Indices32.end());

		for (const GeometryGenerator::Vertex& vertex : mesh.Vertices)
		{
			minPoint.x = (std::min)(minPoint.x, vertex.Position.x);
			minPoint.y = (std::min)(minPoint.y, vertex.Position.y);
			minPoint.z = (std::min)(minPoint.z, vertex.Position.z);
			maxPoint.x = (std::max)(maxPoint.x, vertex.Position.x);
			maxPoint.y = (std::max)(maxPoint.y, vertex.Position.y);
			maxPoint.z = (std::max)(maxPoint.z, vertex.Position.z);
		}

		DeferredRenderer::ModelDrawItem drawItem;
		drawItem.DrawName = "mesh_" + std::to_string(meshIndex);
		drawItem.DiffuseTexturePath = mesh.DiffuseTexturePath;
		drawItem.HasAlphaCutout = mesh.HasAlphaCutout;
		m_drawItems.push_back(std::move(drawItem));
	}

	m_sceneCenter = XMFLOAT3(
		0.5f * (minPoint.x + maxPoint.x),
		0.5f * (minPoint.y + maxPoint.y),
		0.5f * (minPoint.z + maxPoint.z));

	const float extentX = maxPoint.x - minPoint.x;
	const float extentY = maxPoint.y - minPoint.y;
	const float extentZ = maxPoint.z - minPoint.z;
	const float maxExtent = (std::max)(extentX, (std::max)(extentY, extentZ));
	m_sceneScale = maxExtent > 0.0f ? 20.0f / maxExtent : 1.0f;

	const float scaledHeight = extentY * m_sceneScale;
	const float scaledDepth = extentZ * m_sceneScale;
	const float cameraDistance = (std::max)(18.0f, scaledDepth + 12.0f);
	m_initialCamera.EyePos = XMFLOAT3(
		0.0f,
		(std::max)(6.0f, 0.35f * scaledHeight + 4.0f),
		-cameraDistance);
	m_initialCamera.LookDirection = XMFLOAT3(
		-m_initialCamera.EyePos.x,
		-m_initialCamera.EyePos.y,
		-m_initialCamera.EyePos.z);

	const XMVECTOR initialLook = XMVector3Normalize(XMLoadFloat3(&m_initialCamera.LookDirection));
	m_initialCamera.Yaw = atan2f(XMVectorGetX(initialLook), XMVectorGetZ(initialLook));
	m_initialCamera.Pitch = -asinf(XMVectorGetY(initialLook));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(GeometryGenerator::Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->Name = "sponzaGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
	CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
	CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		context.GetDevice(), context.GetCommandList(), vertices.data(), vbByteSize, geometry->VertexBufferUploader);
	geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		context.GetDevice(), context.GetCommandList(), indices.data(), ibByteSize, geometry->IndexBufferUploader);

	geometry->VertexByteStride = sizeof(GeometryGenerator::Vertex);
	geometry->VertexBufferByteSize = vbByteSize;
	geometry->IndexFormat = DXGI_FORMAT_R32_UINT;
	geometry->IndexBufferByteSize = ibByteSize;

	UINT runningBaseVertex = 0;
	UINT runningStartIndex = 0;
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		SubmeshGeometry submesh;
		submesh.IndexCount = static_cast<UINT>(meshes[meshIndex].Indices32.size());
		submesh.StartIndexLocation = runningStartIndex;
		submesh.BaseVertexLocation = runningBaseVertex;
		geometry->DrawArgs[m_drawItems[meshIndex].DrawName] = submesh;

		runningBaseVertex += static_cast<UINT>(meshes[meshIndex].Vertices.size());
		runningStartIndex += static_cast<UINT>(meshes[meshIndex].Indices32.size());
	}

	m_geometry = std::move(geometry);
}

void SponzaScene::CreateTextureResource(DirectX12Context& context, Texture& texture, const void* pixelData, UINT width, UINT height)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
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

void SponzaScene::BuildTextures(DirectX12Context& context)
{
	m_textures.clear();
	m_orderedTextures.clear();

	const std::array<std::uint8_t, 4> whitePixel = { 255, 255, 255, 255 };
	std::unordered_map<std::string, UINT> textureIndices;

	for (DeferredRenderer::ModelDrawItem& drawItem : m_drawItems)
	{
		const std::string textureKey = drawItem.DiffuseTexturePath.empty() ? "__default_white__" : drawItem.DiffuseTexturePath;
		const auto existing = textureIndices.find(textureKey);
		if (existing != textureIndices.end())
		{
			drawItem.DiffuseSrvHeapIndex = existing->second;
			continue;
		}

		auto texture = std::make_unique<Texture>();
		texture->Name = textureKey;

		if (drawItem.DiffuseTexturePath.empty())
		{
			texture->Filename = L"default-white";
			CreateTextureResource(context, *texture, whitePixel.data(), 1, 1);
		}
		else
		{
			const std::wstring texturePath = AnsiToWString(drawItem.DiffuseTexturePath);
			const TgaTextureData textureData = LoadUncompressedTga(texturePath);
			texture->Filename = texturePath;
			CreateTextureResource(context, *texture, textureData.Pixels.data(), textureData.Width, textureData.Height);
		}

		const UINT textureIndex = static_cast<UINT>(m_orderedTextures.size());
		drawItem.DiffuseSrvHeapIndex = textureIndex;
		textureIndices[textureKey] = textureIndex;
		m_orderedTextures.push_back(texture.get());
		m_textures[textureKey] = std::move(texture);
	}
}

void SponzaScene::BuildDescriptorHeap(DirectX12Context& context)
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = static_cast<UINT>(m_orderedTextures.size());
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (Texture* texture : m_orderedTextures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture->Resource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		context.GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, handle);
		handle.Offset(1, context.GetCbvSrvUavDescriptorSize());
	}
}
