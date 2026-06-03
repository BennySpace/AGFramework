#include "SponzaScene.h"

#include "../Demo/DemoSceneComposer.h"
#include "../ObjModelLoader.h"
#include "../Resources/ResourceUploader.h"
#include "../Resources/TextureLoader.h"
#include "../dx12/DirectX12Context.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <unordered_map>

using namespace DirectX;

namespace
{
constexpr wchar_t kSharedWhiteTexturePath[] = L"Assets\\shared\\textures\\white1x1.dds";
constexpr wchar_t kSharedErrorTexturePath[] = L"Assets\\shared\\textures\\texture_error.dds";

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

std::string ResolveAssetPathUtf8(const std::wstring &assetRelativePath)
{
	return WStringToString(ResolveAssetPath(assetRelativePath));
}

bool FileExists(const std::string &path)
{
	if (path.empty())
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(AnsiToWString(path).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string ReplaceExtension(const std::string &path, const std::string &newExtension)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	return path.substr(0, extensionPos) + newExtension;
}

std::string ToLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

bool HasExtension(const std::string &path, const std::string &extension)
{
	const std::string lowerPath = ToLower(path);
	const std::string lowerExtension = ToLower(extension);
	return lowerPath.size() >= lowerExtension.size() &&
	       lowerPath.compare(lowerPath.size() - lowerExtension.size(), lowerExtension.size(), lowerExtension) == 0;
}

std::string PreferDdsVariant(const std::string &path)
{
	if (path.empty())
	{
		return path;
	}

	if (HasExtension(path, ".dds"))
	{
		return path;
	}

	const std::string ddsPath = ReplaceExtension(path, ".dds");
	return FileExists(ddsPath) ? ddsPath : path;
}

std::string ReplaceStemSuffix(const std::string &path, const std::string &stemSuffix, const std::string &replacement)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	const std::string stem = path.substr(0, extensionPos);
	const std::string extension = path.substr(extensionPos);
	const std::string lowerStem = ToLower(stem);
	const std::string lowerSuffix = ToLower(stemSuffix);
	if (lowerStem.size() < lowerSuffix.size() ||
	    lowerStem.compare(lowerStem.size() - lowerSuffix.size(), lowerSuffix.size(), lowerSuffix) != 0)
	{
		return std::string();
	}

	return stem.substr(0, stem.size() - stemSuffix.size()) + replacement + extension;
}

std::string AppendStemSuffix(const std::string &path, const std::string &suffix)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	return path.substr(0, extensionPos) + suffix + path.substr(extensionPos);
}

std::string FindCompanionTexturePath(const std::string &diffusePath, std::initializer_list<std::string> suffixes)
{
	if (diffusePath.empty())
	{
		return std::string();
	}

	std::vector<std::string> candidates;
	for (const std::string &suffix : suffixes)
	{
		const std::string fromDiff = ReplaceStemSuffix(diffusePath, "_diff", suffix);
		if (!fromDiff.empty())
		{
			candidates.push_back(fromDiff);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_diff", suffix + ".dds"));
		}

		const std::string fromDif = ReplaceStemSuffix(diffusePath, "_dif", suffix);
		if (!fromDif.empty())
		{
			candidates.push_back(fromDif);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_dif", suffix + ".dds"));
		}

		const std::string fromTexture = ReplaceStemSuffix(diffusePath, "_texture", "_texture" + suffix);
		if (!fromTexture.empty())
		{
			candidates.push_back(fromTexture);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_texture", "_texture" + suffix + ".dds"));
		}

		const std::string fromBaseColor = ReplaceStemSuffix(diffusePath, "_BaseColor", suffix);
		if (!fromBaseColor.empty())
		{
			candidates.push_back(fromBaseColor);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_BaseColor", suffix + ".dds"));
		}

		const std::string fromGeneric = AppendStemSuffix(diffusePath, suffix);
		if (!fromGeneric.empty())
		{
			candidates.push_back(fromGeneric);
			candidates.push_back(AppendStemSuffix(diffusePath, suffix + ".dds"));
		}
	}

	for (const std::string &candidate : candidates)
	{
		if (FileExists(candidate))
		{
			return candidate;
		}
	}

	return std::string();
}

void UpdateBounds(const GeometryGenerator::Vertex &vertex, XMFLOAT3 &minPoint, XMFLOAT3 &maxPoint)
{
	minPoint.x = (std::min)(minPoint.x, vertex.Position.x);
	minPoint.y = (std::min)(minPoint.y, vertex.Position.y);
	minPoint.z = (std::min)(minPoint.z, vertex.Position.z);
	maxPoint.x = (std::max)(maxPoint.x, vertex.Position.x);
	maxPoint.y = (std::max)(maxPoint.y, vertex.Position.y);
	maxPoint.z = (std::max)(maxPoint.z, vertex.Position.z);
}

} // namespace

void SponzaScene::Initialize(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings)
{
	BuildGeometry(context, demoSettings);
	BuildTextures(context);
	BuildDescriptorHeap(context);
}

void SponzaScene::DisposeUploaders()
{
	m_resources.DisposeUploaders();
}

void SponzaScene::BuildGeometry(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings)
{
	const std::wstring modelPath = ResolveAssetPath(L"Assets\\sponza\\sponza.obj");
	std::vector<ObjModelLoader::MeshData> meshes = ObjModelLoader().Load(WStringToString(modelPath));
	if (meshes.empty())
	{
		throw std::runtime_error("No meshes were loaded from the OBJ model.");
	}

	XMFLOAT3 sponzaMinPoint((std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)());
	XMFLOAT3 sponzaMaxPoint(-(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)());
	for (const ObjModelLoader::MeshData &mesh : meshes)
	{
		for (const GeometryGenerator::Vertex &vertex : mesh.Vertices)
		{
			UpdateBounds(vertex, sponzaMinPoint, sponzaMaxPoint);
		}
	}

	if (demoSettings.EnablePbrGrid)
	{
		Demo::DemoSceneComposer::AppendDemoMeshes(meshes, sponzaMinPoint, sponzaMaxPoint, demoSettings);
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
			UpdateBounds(vertex, minPoint, maxPoint);
		}

		DeferredRenderer::ModelDrawItem drawItem;
		drawItem.DrawName = "mesh_" + std::to_string(meshIndex);
		drawItem.MaterialName = mesh.MaterialName;
		drawItem.DiffuseTexturePath = PreferDdsVariant(mesh.DiffuseTexturePath);
		drawItem.NormalTexturePath = mesh.NormalTexturePath.empty()
		                                 ? FindCompanionTexturePath(drawItem.DiffuseTexturePath, {"_norm", "_ddn", "_normal", "_nmap"})
		                                 : PreferDdsVariant(mesh.NormalTexturePath);
		drawItem.OrmTexturePath = mesh.OrmTexturePath.empty()
		                              ? FindCompanionTexturePath(drawItem.DiffuseTexturePath, {"_orm", "_rma", "_metallicRoughness"})
		                              : PreferDdsVariant(mesh.OrmTexturePath);
		Demo::DemoSceneComposer::ApplyDemoMaterialDefaults(mesh, drawItem);
		drawItem.TextureFlags.x = drawItem.NormalTexturePath.empty() ? 0.0f : 1.0f;
		drawItem.TextureFlags.y = drawItem.OrmTexturePath.empty() ? 0.0f : 1.0f;
		drawItem.HasAlphaCutout = mesh.HasAlphaCutout;
		drawItem.CastShadows = !drawItem.IsDemoPbrGrid;
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

void SponzaScene::ApplyPositionOffsetToDrawItems(const std::vector<int> &drawItemIndices, const XMFLOAT3 &positionOffset)
{
	for (const int drawItemIndex : drawItemIndices)
	{
		if (drawItemIndex < 0 || drawItemIndex >= static_cast<int>(m_data.DrawItems.size()))
		{
			continue;
		}

		m_data.DrawItems[drawItemIndex].PositionOffset = XMFLOAT4(positionOffset.x, positionOffset.y, positionOffset.z, 0.0f);
	}
}

void SponzaScene::BuildTextures(DirectX12Context &context)
{
	m_resources.Textures.clear();
	m_resources.OrderedTextures.clear();

	const std::string defaultWhiteTexturePath = ResolveAssetPathUtf8(kSharedWhiteTexturePath);
	const std::string errorTexturePath = ResolveAssetPathUtf8(kSharedErrorTexturePath);
	const std::array<std::uint8_t, 4> whitePixel = {255, 255, 255, 255};
	const std::array<std::uint8_t, 4> defaultNormalPixel = {128, 128, 255, 255};
	const std::array<std::uint8_t, 4> defaultOrmPixel = {255, 128, 0, 255};
	std::unordered_map<std::string, UINT> textureIndices;

	auto loadTexture = [&](const std::string &texturePath, const std::string &fallbackKey, const std::array<std::uint8_t, 4> &fallbackPixel,
	                       bool useSharedDiffuseFallbacks = false) {
		bool useProceduralFallback = false;
		std::string resolvedTexturePath = texturePath;
		if (resolvedTexturePath.empty())
		{
			if (useSharedDiffuseFallbacks && FileExists(defaultWhiteTexturePath))
			{
				resolvedTexturePath = defaultWhiteTexturePath;
			}
			else
			{
				useProceduralFallback = true;
			}
		}
		else if (useSharedDiffuseFallbacks && !FileExists(resolvedTexturePath) && FileExists(errorTexturePath))
		{
			resolvedTexturePath = errorTexturePath;
		}

		const std::string textureKey = useProceduralFallback ? fallbackKey : resolvedTexturePath;
		const auto existing = textureIndices.find(textureKey);
		if (existing != textureIndices.end())
		{
			return existing->second;
		}

		auto texture = std::make_unique<Texture>();
		texture->Name = textureKey;

		if (useProceduralFallback)
		{
			texture->Filename = AnsiToWString(fallbackKey);
			ResourceUploader::UploadTexture2D(context, *texture, fallbackPixel.data(), 1, 1);
		}
		else
		{
			try
			{
				const std::wstring wideTexturePath = AnsiToWString(resolvedTexturePath);
				const TextureLoader::SceneTextureSource textureSource = TextureLoader::LoadSceneTexture(wideTexturePath);
				ResourceUploader::UploadSceneTexture(context, *texture, textureSource);
			}
			catch (const std::exception &)
			{
				if (useSharedDiffuseFallbacks && resolvedTexturePath != errorTexturePath && FileExists(errorTexturePath))
				{
					const std::wstring wideErrorTexturePath = AnsiToWString(errorTexturePath);
					const TextureLoader::SceneTextureSource textureSource = TextureLoader::LoadSceneTexture(wideErrorTexturePath);
					ResourceUploader::UploadSceneTexture(context, *texture, textureSource);
				}
				else
				{
					throw;
				}
			}
		}

		const UINT textureIndex = static_cast<UINT>(m_resources.OrderedTextures.size());
		textureIndices[textureKey] = textureIndex;
		m_resources.OrderedTextures.push_back(texture.get());
		m_resources.Textures[textureKey] = std::move(texture);
		return textureIndex;
	};

	for (DeferredRenderer::ModelDrawItem &drawItem : m_data.DrawItems)
	{
		drawItem.DiffuseSrvHeapIndex = loadTexture(drawItem.DiffuseTexturePath, "__default_white__", whitePixel, true);
		drawItem.NormalSrvHeapIndex = loadTexture(drawItem.NormalTexturePath, "__default_normal__", defaultNormalPixel);
		drawItem.OrmSrvHeapIndex = loadTexture(drawItem.OrmTexturePath, "__default_orm__", defaultOrmPixel);
	}
}

void SponzaScene::BuildDescriptorHeap(DirectX12Context &context)
{
	m_resources.SrvDescriptorHeap.Initialize(context.GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
	                                         static_cast<UINT>(m_resources.OrderedTextures.size()),
	                                         D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
	for (Texture *texture : m_resources.OrderedTextures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture->Resource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = static_cast<UINT>(texture->Resource->GetDesc().MipLevels);
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		context.GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc,
		                                              m_resources.SrvDescriptorHeap.Allocate().CpuHandle);
	}
}
