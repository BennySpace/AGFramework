#include "SponzaScene.h"

#include "../ObjModelLoader.h"
#include "../Resources/ResourceUploader.h"
#include "../Resources/TextureLoader.h"
#include "../dx12/DirectX12Context.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>

using namespace DirectX;

namespace
{
std::wstring ResolveAssetPath(const std::wstring &assetRelativePath)
{
	const std::wstring candidates[] = {assetRelativePath, L"..\\" + assetRelativePath, L"..\\..\\" + assetRelativePath,
	                                   L"AGFramework\\" + assetRelativePath};

	for (const std::wstring &candidate : candidates)
	{
		const DWORD attributes = GetFileAttributesW(candidate.c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
		{
			return candidate;
		}
	}

	return assetRelativePath;
}

std::string WStringToString(const std::wstring &wideString)
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

std::string ToLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
	               [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

bool ContainsKeyword(const std::string &text, const char *keyword)
{
	return text.find(keyword) != std::string::npos;
}

XMFLOAT4 BuildDefaultPbrParams(const ObjModelLoader::MeshData &mesh)
{
	const std::string materialName = ToLowerAscii(mesh.MaterialName);
	const std::string diffuseTexturePath = ToLowerAscii(mesh.DiffuseTexturePath);
	const std::string materialKey = materialName + " " + diffuseTexturePath;

	if (mesh.HasAlphaCutout || ContainsKeyword(materialKey, "leaf") || ContainsKeyword(materialKey, "vase_plant") ||
	    ContainsKeyword(materialKey, "chain"))
	{
		return XMFLOAT4(0.0f, 0.8f, 1.0f, 1.0f);
	}

	if (ContainsKeyword(materialKey, "metal") || ContainsKeyword(materialKey, "iron") || ContainsKeyword(materialKey, "steel") ||
	    ContainsKeyword(materialKey, "gold") || ContainsKeyword(materialKey, "copper") || ContainsKeyword(materialKey, "bronze") ||
	    ContainsKeyword(materialKey, "brass"))
	{
		return XMFLOAT4(1.0f, 0.28f, 1.0f, 1.0f);
	}

	if (ContainsKeyword(materialKey, "fabric") || ContainsKeyword(materialKey, "cloth") || ContainsKeyword(materialKey, "curtain") ||
	    ContainsKeyword(materialKey, "flag") || ContainsKeyword(materialKey, "banner") || ContainsKeyword(materialKey, "rug"))
	{
		return XMFLOAT4(0.0f, 0.9f, 1.0f, 1.0f);
	}

	if (ContainsKeyword(materialKey, "wood"))
	{
		return XMFLOAT4(0.0f, 0.72f, 1.0f, 1.0f);
	}

	if (ContainsKeyword(materialKey, "floor") || ContainsKeyword(materialKey, "stone") || ContainsKeyword(materialKey, "column") ||
	    ContainsKeyword(materialKey, "wall") || ContainsKeyword(materialKey, "brick") || ContainsKeyword(materialKey, "ceiling"))
	{
		return XMFLOAT4(0.0f, 0.95f, 1.0f, 1.0f);
	}

	return XMFLOAT4(0.0f, 0.65f, 1.0f, 1.0f);
}
} // namespace

void SponzaScene::Initialize(DirectX12Context &context)
{
	BuildGeometry(context);
	BuildTextures(context);
	BuildDescriptorHeap(context);
}

void SponzaScene::DisposeUploaders()
{
	m_resources.DisposeUploaders();
}

void SponzaScene::BuildGeometry(DirectX12Context &context)
{
	const std::wstring modelPath = ResolveAssetPath(L"Assets\\sponza\\sponza.obj");
	std::vector<ObjModelLoader::MeshData> meshes = ObjModelLoader().Load(WStringToString(modelPath));
	if (meshes.empty())
	{
		throw std::runtime_error("No meshes were loaded from the OBJ model.");
	}

	std::vector<GeometryGenerator::Vertex> vertices;
	std::vector<std::uint32_t> indices;
	m_data.DrawItems.clear();

	XMFLOAT3 minPoint((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)());
	XMFLOAT3 maxPoint(-(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)());

	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const ObjModelLoader::MeshData &mesh = meshes[meshIndex];
		vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
		indices.insert(indices.end(), mesh.Indices32.begin(), mesh.Indices32.end());

		for (const GeometryGenerator::Vertex &vertex : mesh.Vertices)
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
		drawItem.MaterialName = mesh.MaterialName;
		drawItem.DiffuseTexturePath = mesh.DiffuseTexturePath;
		drawItem.PbrParams = BuildDefaultPbrParams(mesh);
		drawItem.HasAlphaCutout = mesh.HasAlphaCutout;
		drawItem.CastShadows = true;
		m_data.DrawItems.push_back(std::move(drawItem));
	}

	m_data.SceneCenter = XMFLOAT3(0.5f * (minPoint.x + maxPoint.x), 0.5f * (minPoint.y + maxPoint.y), 0.5f * (minPoint.z + maxPoint.z));

	const float extentX = maxPoint.x - minPoint.x;
	const float extentY = maxPoint.y - minPoint.y;
	const float extentZ = maxPoint.z - minPoint.z;
	const float maxExtent = (std::max)(extentX, (std::max)(extentY, extentZ));
	m_data.SceneScale = maxExtent > 0.0f ? 20.0f / maxExtent : 1.0f;

	const float scaledHeight = extentY * m_data.SceneScale;
	const float scaledDepth = extentZ * m_data.SceneScale;
	const float cameraDistance = (std::max)(18.0f, scaledDepth + 12.0f);
	m_data.InitialCamera.EyePos = XMFLOAT3(0.0f, (std::max)(6.0f, 0.35f * scaledHeight + 4.0f), -cameraDistance);
	m_data.InitialCamera.LookDirection =
	    XMFLOAT3(-m_data.InitialCamera.EyePos.x, -m_data.InitialCamera.EyePos.y, -m_data.InitialCamera.EyePos.z);

	const XMVECTOR initialLook = XMVector3Normalize(XMLoadFloat3(&m_data.InitialCamera.LookDirection));
	m_data.InitialCamera.Yaw = atan2f(XMVectorGetX(initialLook), XMVectorGetZ(initialLook));
	m_data.InitialCamera.Pitch = -asinf(XMVectorGetY(initialLook));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(GeometryGenerator::Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	auto geometry = std::make_unique<MeshGeometry>();
	geometry->Name = "sponzaGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geometry->VertexBufferCPU));
	CopyMemory(geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geometry->IndexBufferCPU));
	CopyMemory(geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(context.GetDevice(), context.GetCommandList(), vertices.data(), vbByteSize,
	                                                         geometry->VertexBufferUploader);
	geometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(context.GetDevice(), context.GetCommandList(), indices.data(), ibByteSize,
	                                                        geometry->IndexBufferUploader);

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
		geometry->DrawArgs[m_data.DrawItems[meshIndex].DrawName] = submesh;

		runningBaseVertex += static_cast<UINT>(meshes[meshIndex].Vertices.size());
		runningStartIndex += static_cast<UINT>(meshes[meshIndex].Indices32.size());
	}

	m_resources.Geometry = std::move(geometry);
}

void SponzaScene::BuildTextures(DirectX12Context &context)
{
	m_resources.Textures.clear();
	m_resources.OrderedTextures.clear();

	const std::array<std::uint8_t, 4> whitePixel = {255, 255, 255, 255};
	std::unordered_map<std::string, UINT> textureIndices;

	for (DeferredRenderer::ModelDrawItem &drawItem : m_data.DrawItems)
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
			ResourceUploader::UploadTexture2D(context, *texture, whitePixel.data(), 1, 1);
		}
		else
		{
			const std::wstring texturePath = AnsiToWString(drawItem.DiffuseTexturePath);
			const TextureLoader::ImageData textureData = TextureLoader::LoadUncompressedTga(texturePath);
			texture->Filename = texturePath;
			ResourceUploader::UploadTexture2D(context, *texture, textureData.Pixels.data(), textureData.Width, textureData.Height);
		}

		const UINT textureIndex = static_cast<UINT>(m_resources.OrderedTextures.size());
		drawItem.DiffuseSrvHeapIndex = textureIndex;
		textureIndices[textureKey] = textureIndex;
		m_resources.OrderedTextures.push_back(texture.get());
		m_resources.Textures[textureKey] = std::move(texture);
	}
}

void SponzaScene::BuildDescriptorHeap(DirectX12Context &context)
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = static_cast<UINT>(m_resources.OrderedTextures.size());
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_resources.SrvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_resources.SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (Texture *texture : m_resources.OrderedTextures)
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
