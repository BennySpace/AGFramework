#include "DeferredRenderer.h"

#include "Resources/ResourceUploader.h"
#include "dx12/DirectX12Context.h"
#include <algorithm>
#include <cwctype>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
	std::wstring ResolveShaderPath(const std::wstring& shaderRelativePath)
	{
		const std::wstring candidates[] =
		{
			shaderRelativePath,
			L"..\\" + shaderRelativePath,
			L"..\\..\\" + shaderRelativePath,
			L"AGFramework\\" + shaderRelativePath
		};

		for (const std::wstring& candidate : candidates)
		{
			const DWORD attributes = GetFileAttributesW(candidate.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return candidate;
			}
		}

		return shaderRelativePath;
	}

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

		return L"";
	}

	std::wstring ResolveFirstExistingAssetPath(std::initializer_list<std::wstring> candidatePaths)
	{
		for (const std::wstring& candidatePath : candidatePaths)
		{
			const std::wstring resolvedPath = ResolveAssetPath(candidatePath);
			if (!resolvedPath.empty())
			{
				return resolvedPath;
			}
		}

		return L"";
	}

	bool ContainsCaseInsensitive(const std::wstring& text, const std::wstring& needle)
	{
		if (needle.empty() || text.size() < needle.size())
		{
			return false;
		}

		auto toLower = [](wchar_t value)
			{
				return static_cast<wchar_t>(towlower(value));
			};

		std::wstring lowerText(text.size(), L'\0');
		std::transform(text.begin(), text.end(), lowerText.begin(), toLower);

		std::wstring lowerNeedle(needle.size(), L'\0');
		std::transform(needle.begin(), needle.end(), lowerNeedle.begin(), toLower);

		return lowerText.find(lowerNeedle) != std::wstring::npos;
	}

	void LoadDdsTextureOrFallback(
		DirectX12Context& context,
		Texture& texture,
		const std::wstring& path,
		bool isCubeTexture,
		const std::array<std::uint8_t, 4>& fallbackPixel)
	{
		if (!path.empty())
		{
			texture.Filename = path;
			ThrowIfFailed(CreateDDSTextureFromFile12(
				context.GetDevice(),
				context.GetCommandList(),
				path.c_str(),
				texture.Resource,
				texture.UploadHeap));
			return;
		}

		texture.Filename = isCubeTexture ? L"generated-cube-fallback" : L"generated-2d-fallback";
		if (isCubeTexture)
		{
			ResourceUploader::UploadTextureCube(context, texture, fallbackPixel.data(), 1, 1);
		}
		else
		{
			ResourceUploader::UploadTexture2D(context, texture, fallbackPixel.data(), 1, 1);
		}
	}
}

DeferredRenderer::~DeferredRenderer()
{
	if (m_objectCB != nullptr && m_mappedObjectCB != nullptr)
	{
		m_objectCB->Unmap(0, nullptr);
		m_mappedObjectCB = nullptr;
	}

	if (m_shadowPassCB != nullptr && m_mappedShadowPassCB != nullptr)
	{
		m_shadowPassCB->Unmap(0, nullptr);
		m_mappedShadowPassCB = nullptr;
	}
}

void DeferredRenderer::Initialize(DirectX12Context& context, bool enable4xMsaa, UINT msaaQuality)
{
	BuildShadersAndInputLayout();
	BuildConstantBuffer(context);
	BuildGbuffer(context);
	BuildCascadedShadowMap(context);
	BuildImageBasedLightingTextures(context);
	BuildLightingSrvHeap(context);
	BuildRootSignature(context);
	BuildPSO(context, enable4xMsaa, msaaQuality);
}

void DeferredRenderer::Resize(DirectX12Context& context)
{
	BuildGbuffer(context);
	BuildCascadedShadowMap(context);
	BuildLightingSrvHeap(context);
}

void DeferredRenderer::UpdateMainPassCB(const FrameData& frameData)
{
	m_shadowSettings = frameData.ShadowSettings;
	m_cascadedShadowData = frameData.CascadedShadowData;
	m_sceneCenter = frameData.SceneCenter;
	m_sceneScale = frameData.SceneScale;

	XMMATRIX world =
		XMMatrixTranslation(-frameData.SceneCenter.x, -frameData.SceneCenter.y, -frameData.SceneCenter.z) *
		XMMatrixScaling(frameData.SceneScale, frameData.SceneScale, frameData.SceneScale);
	XMMATRIX texTransform = XMMatrixIdentity();

	const XMVECTOR eyePos = XMLoadFloat3(&frameData.EyePos);
	const XMVECTOR lookDirection = XMLoadFloat3(&frameData.LookDirection);
	const XMVECTOR target = eyePos + lookDirection;
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eyePos, target, up);
	XMMATRIX proj = XMLoadFloat4x4(&frameData.Projection);
	XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
	XMMATRIX worldViewProj = world * view * proj;

	ObjectConstants objectConstants;
	XMStoreFloat4x4(&objectConstants.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objectConstants.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
	XMStoreFloat4x4(&objectConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	XMStoreFloat4x4(&objectConstants.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&objectConstants.TexTransform, XMMatrixTranspose(texTransform));
	objectConstants.EyePosW = frameData.EyePos;
	objectConstants.LightingSettings = frameData.LightingSettings;
	objectConstants.Material = frameData.Material;
	const bool decodeImageBasedLightingAsRgbm =
		frameData.ImageBasedLightingSettings.UseAutoDecoding
			? m_imageBasedLightingUsesRgbm
			: frameData.ImageBasedLightingSettings.DecodeAsRgbm;
	objectConstants.ImageBasedLightingSettings = XMFLOAT4(
		m_prefilterMapTexture && m_prefilterMapTexture->Resource
			? static_cast<float>(m_prefilterMapTexture->Resource->GetDesc().MipLevels > 0
				? m_prefilterMapTexture->Resource->GetDesc().MipLevels - 1
				: 0)
			: 0.0f,
		decodeImageBasedLightingAsRgbm ? 1.0f : 0.0f,
		decodeImageBasedLightingAsRgbm ? frameData.ImageBasedLightingSettings.RgbmScale : 1.0f,
		0.0f);

	for (size_t lightIndex = 0; lightIndex < LightSystem::DirectionalLightCount; ++lightIndex)
	{
		objectConstants.DirectionalLights[lightIndex] = frameData.LightState.DirectionalLights[lightIndex];
	}
	for (size_t lightIndex = 0; lightIndex < LightSystem::PointLightCount; ++lightIndex)
	{
		objectConstants.PointLights[lightIndex] = frameData.LightState.PointLights[lightIndex];
	}
	for (size_t lightIndex = 0; lightIndex < LightSystem::SpotLightCount; ++lightIndex)
	{
		objectConstants.SpotLights[lightIndex] = frameData.LightState.SpotLights[lightIndex];
	}

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < RenderSettings::MaxShadowCascadeCount; ++cascadeIndex)
	{
		const XMMATRIX shadowLightViewProj = XMLoadFloat4x4(&frameData.CascadedShadowData.LightViewProjMatrices[cascadeIndex]);
		XMStoreFloat4x4(&objectConstants.ShadowLightViewProj[cascadeIndex], XMMatrixTranspose(shadowLightViewProj));
	}

	objectConstants.ShadowCascadeSplits = XMFLOAT4(
		frameData.CascadedShadowData.SplitDistances[0],
		frameData.CascadedShadowData.SplitDistances[1],
		frameData.CascadedShadowData.SplitDistances[2],
		frameData.CascadedShadowData.SplitDistances[3]);
	objectConstants.ShadowMapMetrics = frameData.CascadedShadowData.ShadowMapMetrics;
	const std::uint32_t cascadeCount = (std::min)(frameData.ShadowSettings.CascadeCount, RenderSettings::MaxShadowCascadeCount);
	objectConstants.ShadowSettings0 = XMFLOAT4(
		static_cast<float>(cascadeCount),
		frameData.ShadowSettings.PcfRadius,
		frameData.ShadowSettings.ShadowStrength,
		frameData.ShadowSettings.EnableDirectionalShadows ? 1.0f : 0.0f);
	objectConstants.ShadowSettings1 = XMFLOAT4(
		frameData.ShadowSettings.DepthBias,
		frameData.ShadowSettings.SlopeScaledDepthBias,
		frameData.ShadowSettings.DepthBiasClamp,
		frameData.ShadowSettings.CascadeSplitLambda);
	objectConstants.ShadowSettings2 = XMFLOAT4(
		frameData.ShadowSettings.ReceiverBiasMin,
		frameData.ShadowSettings.ReceiverBiasSlopeScale,
		frameData.ShadowSettings.ReceiverBiasTexelFactor,
		0.0f);

	memcpy(m_mappedObjectCB, &objectConstants, sizeof(objectConstants));
}

void DeferredRenderer::BuildCascadedShadowMap(DirectX12Context& context)
{
	CascadedShadowMap::Desc shadowMapDesc;
	shadowMapDesc.Width = m_shadowSettings.ShadowMapSize;
	shadowMapDesc.Height = m_shadowSettings.ShadowMapSize;
	shadowMapDesc.CascadeCount = (std::min)(m_shadowSettings.CascadeCount, RenderSettings::MaxShadowCascadeCount);

	if (m_cascadedShadowMap != nullptr)
	{
		const CascadedShadowMap::Desc& currentDesc = m_cascadedShadowMap->GetDesc();
		if (currentDesc.Width == shadowMapDesc.Width &&
			currentDesc.Height == shadowMapDesc.Height &&
			currentDesc.CascadeCount == shadowMapDesc.CascadeCount &&
			currentDesc.ResourceFormat == shadowMapDesc.ResourceFormat &&
			currentDesc.DsvFormat == shadowMapDesc.DsvFormat &&
			currentDesc.SrvFormat == shadowMapDesc.SrvFormat)
		{
			return;
		}
	}

	if (!m_cascadedShadowMap)
	{
		m_cascadedShadowMap = std::make_unique<CascadedShadowMap>();
	}

	if (!m_cascadedShadowMap->Initialize(context.GetDevice(), shadowMapDesc))
	{
		throw std::runtime_error("Failed to initialize cascaded shadow map array.");
	}

	m_cascadedShadowMapState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	m_lightingSrvHeapDirty = true;
}

void DeferredRenderer::BuildImageBasedLightingTextures(DirectX12Context& context)
{
	if (m_irradianceMapTexture != nullptr &&
		m_prefilterMapTexture != nullptr &&
		m_brdfLutTexture != nullptr)
	{
		return;
	}

	m_irradianceMapTexture = std::make_unique<Texture>();
	m_irradianceMapTexture->Name = "ibl_irradiance";
	m_prefilterMapTexture = std::make_unique<Texture>();
	m_prefilterMapTexture->Name = "ibl_prefilter";
	m_brdfLutTexture = std::make_unique<Texture>();
	m_brdfLutTexture->Name = "ibl_brdf_lut";

	const std::wstring irradiancePath = ResolveFirstExistingAssetPath({
		L"Assets\\ibl\\irradiance.dds"
		});
	const std::wstring prefilterPath = ResolveFirstExistingAssetPath({
		L"Assets\\ibl\\prefilter.dds",
		L"Assets\\ibl\\prefiltered_environment.dds",
		L"Assets\\ibl\\prefilteredEnv.dds"
		});
	const std::wstring brdfLutPath = ResolveFirstExistingAssetPath({
		L"Assets\\ibl\\brdfLUT.dds",
		L"Assets\\ibl\\brdf_lut.dds",
		L"Assets\\ibl\\brdf_integration.dds"
		});

	m_imageBasedLightingUsesRgbm =
		ContainsCaseInsensitive(irradiancePath, L"mdr") ||
		ContainsCaseInsensitive(prefilterPath, L"mdr");

	const std::array<std::uint8_t, 4> blackPixel = { 0, 0, 0, 255 };
	const std::array<std::uint8_t, 4> whitePixel = { 255, 255, 255, 255 };
	LoadDdsTextureOrFallback(context, *m_irradianceMapTexture, irradiancePath, true, blackPixel);
	LoadDdsTextureOrFallback(context, *m_prefilterMapTexture, prefilterPath, true, blackPixel);
	LoadDdsTextureOrFallback(context, *m_brdfLutTexture, brdfLutPath, false, whitePixel);

	m_lightingSrvHeapDirty = true;
}

void DeferredRenderer::BuildLightingSrvHeap(DirectX12Context& context)
{
	if (m_gbuffer == nullptr || m_cascadedShadowMap == nullptr)
	{
		return;
	}

	BuildImageBasedLightingTextures(context);

	if (m_lightingSrvHeap != nullptr && !m_lightingSrvHeapDirty)
	{
		return;
	}

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 8;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_lightingSrvHeap.ReleaseAndGetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_lightingSrvHeap->GetCPUDescriptorHandleForHeapStart());
	const DXGI_FORMAT gbufferFormats[] =
	{
		m_gbuffer->GetFormat(Gbuffer::Target::Albedo),
		m_gbuffer->GetFormat(Gbuffer::Target::Normal),
		m_gbuffer->GetFormat(Gbuffer::Target::Position),
		m_gbuffer->GetFormat(Gbuffer::Target::Material)
	};
	ID3D12Resource* gbufferResources[] =
	{
		m_gbuffer->GetResource(Gbuffer::Target::Albedo),
		m_gbuffer->GetResource(Gbuffer::Target::Normal),
		m_gbuffer->GetResource(Gbuffer::Target::Position),
		m_gbuffer->GetResource(Gbuffer::Target::Material)
	};

	for (int targetIndex = 0; targetIndex < 4; ++targetIndex)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = gbufferFormats[targetIndex];
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
		context.GetDevice()->CreateShaderResourceView(gbufferResources[targetIndex], &srvDesc, handle);
		handle.Offset(1, context.GetCbvSrvUavDescriptorSize());
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC shadowSrvDesc = {};
	shadowSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shadowSrvDesc.Format = m_cascadedShadowMap->GetDesc().SrvFormat;
	shadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	shadowSrvDesc.Texture2DArray.MostDetailedMip = 0;
	shadowSrvDesc.Texture2DArray.MipLevels = 1;
	shadowSrvDesc.Texture2DArray.FirstArraySlice = 0;
	shadowSrvDesc.Texture2DArray.ArraySize = m_cascadedShadowMap->GetDesc().CascadeCount;
	shadowSrvDesc.Texture2DArray.PlaneSlice = 0;
	shadowSrvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	context.GetDevice()->CreateShaderResourceView(m_cascadedShadowMap->GetResource(), &shadowSrvDesc, handle);
	handle.Offset(1, context.GetCbvSrvUavDescriptorSize());

	D3D12_SHADER_RESOURCE_VIEW_DESC irradianceSrvDesc = {};
	irradianceSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	irradianceSrvDesc.Format = m_irradianceMapTexture->Resource->GetDesc().Format;
	irradianceSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	irradianceSrvDesc.TextureCube.MostDetailedMip = 0;
	irradianceSrvDesc.TextureCube.MipLevels = m_irradianceMapTexture->Resource->GetDesc().MipLevels;
	irradianceSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	context.GetDevice()->CreateShaderResourceView(m_irradianceMapTexture->Resource.Get(), &irradianceSrvDesc, handle);
	handle.Offset(1, context.GetCbvSrvUavDescriptorSize());

	D3D12_SHADER_RESOURCE_VIEW_DESC prefilterSrvDesc = {};
	prefilterSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	prefilterSrvDesc.Format = m_prefilterMapTexture->Resource->GetDesc().Format;
	prefilterSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	prefilterSrvDesc.TextureCube.MostDetailedMip = 0;
	prefilterSrvDesc.TextureCube.MipLevels = m_prefilterMapTexture->Resource->GetDesc().MipLevels;
	prefilterSrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	context.GetDevice()->CreateShaderResourceView(m_prefilterMapTexture->Resource.Get(), &prefilterSrvDesc, handle);
	handle.Offset(1, context.GetCbvSrvUavDescriptorSize());

	D3D12_SHADER_RESOURCE_VIEW_DESC brdfLutSrvDesc = {};
	brdfLutSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	brdfLutSrvDesc.Format = m_brdfLutTexture->Resource->GetDesc().Format;
	brdfLutSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	brdfLutSrvDesc.Texture2D.MostDetailedMip = 0;
	brdfLutSrvDesc.Texture2D.MipLevels = m_brdfLutTexture->Resource->GetDesc().MipLevels;
	brdfLutSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	context.GetDevice()->CreateShaderResourceView(m_brdfLutTexture->Resource.Get(), &brdfLutSrvDesc, handle);
	m_lightingSrvHeapDirty = false;
}

void DeferredRenderer::RenderShadowMapPass(
	DirectX12Context& context,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	UINT cbvSrvUavDescriptorSize,
	const MeshGeometry& sceneGeometry,
	const std::vector<ModelDrawItem>& drawItems)
{
	BuildCascadedShadowMap(context);
	BuildShadowPSOs(context);

	if (!m_shadowSettings.EnableDirectionalShadows || m_cascadedShadowMap == nullptr)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = context.GetCommandList();
	commandList->SetGraphicsRootSignature(m_shadowRootSignature.Get());
	commandList->RSSetViewports(1, &m_cascadedShadowMap->GetViewport());
	commandList->RSSetScissorRects(1, &m_cascadedShadowMap->GetScissorRect());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = sceneGeometry.VertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW indexBufferView = sceneGeometry.IndexBufferView();
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	commandList->IASetIndexBuffer(&indexBufferView);

	const XMMATRIX world =
		XMMatrixTranslation(-m_sceneCenter.x, -m_sceneCenter.y, -m_sceneCenter.z) *
		XMMatrixScaling(m_sceneScale, m_sceneScale, m_sceneScale);
	const XMMATRIX texTransform = XMMatrixIdentity();

	const std::uint32_t cascadeCount = (std::min)(m_shadowSettings.CascadeCount, RenderSettings::MaxShadowCascadeCount);
	for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		const D3D12_CPU_DESCRIPTOR_HANDLE cascadeDsv = m_cascadedShadowMap->GetDsv(cascadeIndex);
		commandList->OMSetRenderTargets(0, nullptr, FALSE, &cascadeDsv);
		m_cascadedShadowMap->ClearCascade(commandList, cascadeIndex);

		const XMMATRIX lightViewProj = XMLoadFloat4x4(&m_cascadedShadowData.LightViewProjMatrices[cascadeIndex]);
		const XMMATRIX worldLightViewProj = world * lightViewProj;
		ShadowPassConstants shadowConstants;
		XMStoreFloat4x4(&shadowConstants.WorldLightViewProj, XMMatrixTranspose(worldLightViewProj));
		XMStoreFloat4x4(&shadowConstants.TexTransform, XMMatrixTranspose(texTransform));

		const UINT cascadeCbOffset = cascadeIndex * m_shadowPassCBStride;
		memcpy(m_mappedShadowPassCB + cascadeCbOffset, &shadowConstants, sizeof(shadowConstants));
		commandList->SetGraphicsRootConstantBufferView(0, m_shadowPassCB->GetGPUVirtualAddress() + cascadeCbOffset);

		for (const ModelDrawItem& drawItem : drawItems)
		{
			if (!drawItem.CastShadows)
			{
				continue;
			}

			DrawSettings drawSettings;
			drawSettings.AlphaCutoff = drawItem.HasAlphaCutout ? 0.5f : -1.0f;
			commandList->SetGraphicsRoot32BitConstants(2, 4, &drawSettings, 0);

			if (drawItem.HasAlphaCutout)
			{
				CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
				textureHandle.Offset(static_cast<INT>(drawItem.DiffuseSrvHeapIndex), cbvSrvUavDescriptorSize);
				commandList->SetPipelineState(m_directionalShadowAlphaCutoutPSO.Get());
				commandList->SetGraphicsRootDescriptorTable(1, textureHandle);
			}
			else
			{
				commandList->SetPipelineState(m_directionalShadowOpaquePSO.Get());
			}

			const auto& submesh = sceneGeometry.DrawArgs.at(drawItem.DrawName);
			commandList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
		}
	}

	commandList->RSSetViewports(1, &context.GetViewport());
	commandList->RSSetScissorRects(1, &context.GetScissorRect());
}

void DeferredRenderer::DrawGeometryPass(
	DirectX12Context& context,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	UINT cbvSrvUavDescriptorSize,
	const MeshGeometry& sceneGeometry,
	const std::vector<ModelDrawItem>& drawItems)
{
	BuildCascadedShadowMap(context);
	m_gbuffer->Clear(context.GetCommandList());

	D3D12_CPU_DESCRIPTOR_HANDLE gbufferRtvs[4] =
	{
		m_gbuffer->GetRtv(Gbuffer::Target::Albedo),
		m_gbuffer->GetRtv(Gbuffer::Target::Normal),
		m_gbuffer->GetRtv(Gbuffer::Target::Position),
		m_gbuffer->GetRtv(Gbuffer::Target::Material)
	};
	const D3D12_CPU_DESCRIPTOR_HANDLE gbufferDsv = m_gbuffer->GetDsv();
	context.GetCommandList()->OMSetRenderTargets(_countof(gbufferRtvs), gbufferRtvs, false, &gbufferDsv);
	context.GetCommandList()->SetPipelineState(m_geometryPSO.Get());
	context.GetCommandList()->SetGraphicsRootSignature(m_geometryRootSignature.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
	context.GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	context.GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = sceneGeometry.VertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW indexBufferView = sceneGeometry.IndexBufferView();
	context.GetCommandList()->IASetVertexBuffers(0, 1, &vertexBufferView);
	context.GetCommandList()->IASetIndexBuffer(&indexBufferView);
	context.GetCommandList()->SetGraphicsRootConstantBufferView(0, m_objectCB->GetGPUVirtualAddress());

	for (const ModelDrawItem& drawItem : drawItems)
	{
		CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		textureHandle.Offset(static_cast<INT>(drawItem.DiffuseSrvHeapIndex), cbvSrvUavDescriptorSize);
		context.GetCommandList()->SetGraphicsRootDescriptorTable(1, textureHandle);
		DrawSettings drawSettings;
		drawSettings.AlphaCutoff = drawItem.HasAlphaCutout ? 0.5f : -1.0f;
		context.GetCommandList()->SetGraphicsRoot32BitConstants(2, 4, &drawSettings, 0);
		context.GetCommandList()->SetGraphicsRoot32BitConstants(3, 4, &drawItem.PbrParams, 0);

		const auto& submesh = sceneGeometry.DrawArgs.at(drawItem.DrawName);
		context.GetCommandList()->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
	}
}

void DeferredRenderer::DrawLightingPass(DirectX12Context& context, DebugOverlay::DebugViewMode debugViewMode, int shadowDebugCascadeIndex)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = context.CurrentBackBufferView();
	context.GetCommandList()->OMSetRenderTargets(1, &currentBackBufferView, true, nullptr);
	context.GetCommandList()->SetPipelineState(m_lightingPSO.Get());
	context.GetCommandList()->SetGraphicsRootSignature(m_lightingRootSignature.Get());

	BuildLightingSrvHeap(context);

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_lightingSrvHeap.Get() };
	context.GetCommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	context.GetCommandList()->SetGraphicsRootConstantBufferView(0, m_objectCB->GetGPUVirtualAddress());
	context.GetCommandList()->SetGraphicsRootDescriptorTable(1, m_lightingSrvHeap->GetGPUDescriptorHandleForHeapStart());
	LightingDebugSettings debugSettings;
	debugSettings.ViewMode = static_cast<float>(debugViewMode);
	debugSettings.PositionVizScale = 0.05f;
	debugSettings.ShadowDebugCascadeIndex = static_cast<float>(shadowDebugCascadeIndex);
	context.GetCommandList()->SetGraphicsRoot32BitConstants(2, 4, &debugSettings, 0);
	context.GetCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context.GetCommandList()->DrawInstanced(3, 1, 0, 0);
}

void DeferredRenderer::TransitionCascadedShadowMap(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	if (m_cascadedShadowMap == nullptr || beforeState == afterState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_cascadedShadowMap->GetResource(),
		beforeState,
		afterState);
	context.GetCommandList()->ResourceBarrier(1, &barrier);
}

void DeferredRenderer::TransitionGbuffer(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	D3D12_RESOURCE_BARRIER barriers[4] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Albedo), beforeState, afterState),
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Normal), beforeState, afterState),
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Position), beforeState, afterState),
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Material), beforeState, afterState)
	};
	context.GetCommandList()->ResourceBarrier(_countof(barriers), barriers);
}

void DeferredRenderer::BuildShadersAndInputLayout()
{
	m_shaders["standardVS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\GeometryPass.hlsl"), nullptr, "GeometryVS", "vs_5_1");
	m_shaders["gbufferPS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\GeometryPass.hlsl"), nullptr, "GeometryPS", "ps_5_1");
	m_shaders["fullscreenVS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\DeferredLighting.hlsl"), nullptr, "FullscreenVS", "vs_5_1");
	m_shaders["deferredLightingPS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\DeferredLighting.hlsl"), nullptr, "DeferredLightingPS", "ps_5_1");
	m_shaders["shadowVS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\ShadowMap.hlsl"), nullptr, "ShadowVS", "vs_5_1");
	m_shaders["shadowAlphaCutoutPS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\ShadowMap.hlsl"), nullptr, "ShadowAlphaCutoutPS", "ps_5_1");

	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DeferredRenderer::BuildConstantBuffer(DirectX12Context& context)
{
	if (m_objectCB != nullptr && m_mappedObjectCB != nullptr)
	{
		m_objectCB->Unmap(0, nullptr);
		m_mappedObjectCB = nullptr;
		m_objectCB.Reset();
	}

	if (m_shadowPassCB != nullptr && m_mappedShadowPassCB != nullptr)
	{
		m_shadowPassCB->Unmap(0, nullptr);
		m_mappedShadowPassCB = nullptr;
		m_shadowPassCB.Reset();
	}

	m_objectCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	m_shadowPassCBStride = d3dUtil::CalcConstantBufferByteSize(sizeof(ShadowPassConstants));
	m_shadowPassCBByteSize = m_shadowPassCBStride * RenderSettings::MaxShadowCascadeCount;

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(m_objectCBByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_objectCB)));

	ThrowIfFailed(m_objectCB->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedObjectCB)));

	ThrowIfFailed(context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(m_shadowPassCBByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_shadowPassCB)));

	ThrowIfFailed(m_shadowPassCB->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedShadowPassCB)));
}

void DeferredRenderer::BuildGbuffer(DirectX12Context& context)
{
	if (!m_gbuffer)
	{
		m_gbuffer = std::make_unique<Gbuffer>();
	}

	Gbuffer::Desc desc;
	desc.Width = static_cast<UINT>(context.GetClientWidth());
	desc.Height = static_cast<UINT>(context.GetClientHeight());
	desc.AlbedoFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.NormalFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.PositionFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.MaterialFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	desc.DepthFormat = context.GetDepthStencilFormat();

	if (!m_gbuffer->Initialize(context.GetDevice(), desc))
	{
		throw std::runtime_error("Failed to initialize gbuffer.");
	}
	m_gbufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_lightingSrvHeapDirty = true;
}

void DeferredRenderer::BuildRootSignature(DirectX12Context& context)
{
	CD3DX12_STATIC_SAMPLER_DESC linearWrapSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);
	CD3DX12_STATIC_SAMPLER_DESC shadowComparisonSampler(
		1,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);
	CD3DX12_STATIC_SAMPLER_DESC linearClampSampler(
		2,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	CD3DX12_STATIC_SAMPLER_DESC lightingStaticSamplers[] = { linearWrapSampler, shadowComparisonSampler, linearClampSampler };

	CD3DX12_DESCRIPTOR_RANGE geometryTexTable;
	geometryTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER geometryRootParameters[4];
	geometryRootParameters[0].InitAsConstantBufferView(0);
	geometryRootParameters[1].InitAsDescriptorTable(1, &geometryTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	geometryRootParameters[2].InitAsConstants(4, 1);
	geometryRootParameters[3].InitAsConstants(4, 2);

	CD3DX12_ROOT_SIGNATURE_DESC geometryRootSigDesc(
		4,
		geometryRootParameters,
		1,
		&linearWrapSampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(
		&geometryRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()));

	ThrowIfFailed(context.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_geometryRootSignature.GetAddressOf())));

	CD3DX12_DESCRIPTOR_RANGE lightingTexTable;
	lightingTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8, 0);

	CD3DX12_ROOT_PARAMETER lightingRootParameters[3];
	lightingRootParameters[0].InitAsConstantBufferView(0);
	lightingRootParameters[1].InitAsDescriptorTable(1, &lightingTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	lightingRootParameters[2].InitAsConstants(4, 1);

	CD3DX12_ROOT_SIGNATURE_DESC lightingRootSigDesc(
		3,
		lightingRootParameters,
		_countof(lightingStaticSamplers),
		lightingStaticSamplers,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	serializedRootSig.Reset();
	errorBlob.Reset();
	ThrowIfFailed(D3D12SerializeRootSignature(
		&lightingRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()));

	ThrowIfFailed(context.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_lightingRootSignature.GetAddressOf())));

	CD3DX12_DESCRIPTOR_RANGE shadowTexTable;
	shadowTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER shadowRootParameters[3];
	shadowRootParameters[0].InitAsConstantBufferView(0);
	shadowRootParameters[1].InitAsDescriptorTable(1, &shadowTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	shadowRootParameters[2].InitAsConstants(4, 1);

	CD3DX12_ROOT_SIGNATURE_DESC shadowRootSigDesc(
		3,
		shadowRootParameters,
		1,
		&linearWrapSampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	serializedRootSig.Reset();
	errorBlob.Reset();
	ThrowIfFailed(D3D12SerializeRootSignature(
		&shadowRootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()));

	ThrowIfFailed(context.GetDevice()->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_shadowRootSignature.GetAddressOf())));
}

void DeferredRenderer::BuildPSO(DirectX12Context& context, bool enable4xMsaa, UINT msaaQuality)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC geometryPsoDesc = {};
	geometryPsoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
	geometryPsoDesc.pRootSignature = m_geometryRootSignature.Get();
	geometryPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()),
		m_shaders["standardVS"]->GetBufferSize()
	};
	geometryPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["gbufferPS"]->GetBufferPointer()),
		m_shaders["gbufferPS"]->GetBufferSize()
	};
	geometryPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	geometryPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	geometryPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	geometryPsoDesc.SampleMask = UINT_MAX;
	geometryPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	geometryPsoDesc.NumRenderTargets = 4;
	geometryPsoDesc.RTVFormats[0] = m_gbuffer->GetFormat(Gbuffer::Target::Albedo);
	geometryPsoDesc.RTVFormats[1] = m_gbuffer->GetFormat(Gbuffer::Target::Normal);
	geometryPsoDesc.RTVFormats[2] = m_gbuffer->GetFormat(Gbuffer::Target::Position);
	geometryPsoDesc.RTVFormats[3] = m_gbuffer->GetFormat(Gbuffer::Target::Material);
	geometryPsoDesc.SampleDesc.Count = enable4xMsaa ? 4 : 1;
	geometryPsoDesc.SampleDesc.Quality = enable4xMsaa ? (msaaQuality - 1) : 0;
	geometryPsoDesc.DSVFormat = context.GetDepthStencilFormat();

	ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&geometryPsoDesc, IID_PPV_ARGS(&m_geometryPSO)));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC lightingPsoDesc = {};
	lightingPsoDesc.InputLayout = { nullptr, 0 };
	lightingPsoDesc.pRootSignature = m_lightingRootSignature.Get();
	lightingPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["fullscreenVS"]->GetBufferPointer()),
		m_shaders["fullscreenVS"]->GetBufferSize()
	};
	lightingPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["deferredLightingPS"]->GetBufferPointer()),
		m_shaders["deferredLightingPS"]->GetBufferSize()
	};
	lightingPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	lightingPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	lightingPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	lightingPsoDesc.DepthStencilState.DepthEnable = FALSE;
	lightingPsoDesc.DepthStencilState.StencilEnable = FALSE;
	lightingPsoDesc.SampleMask = UINT_MAX;
	lightingPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	lightingPsoDesc.NumRenderTargets = 1;
	lightingPsoDesc.RTVFormats[0] = context.GetBackBufferFormat();
	lightingPsoDesc.SampleDesc.Count = 1;
	lightingPsoDesc.SampleDesc.Quality = 0;

	ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&lightingPsoDesc, IID_PPV_ARGS(&m_lightingPSO)));
	BuildShadowPSOs(context);
}

void DeferredRenderer::BuildShadowPSOs(DirectX12Context& context)
{
	if (m_directionalShadowOpaquePSO != nullptr &&
		m_directionalShadowAlphaCutoutPSO != nullptr &&
		m_directionalShadowPsoSettings.DepthBias == m_shadowSettings.DepthBias &&
		m_directionalShadowPsoSettings.SlopeScaledDepthBias == m_shadowSettings.SlopeScaledDepthBias &&
		m_directionalShadowPsoSettings.DepthBiasClamp == m_shadowSettings.DepthBiasClamp)
	{
		return;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = {};
	shadowPsoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
	shadowPsoDesc.pRootSignature = m_shadowRootSignature.Get();
	shadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["shadowVS"]->GetBufferPointer()),
		m_shaders["shadowVS"]->GetBufferSize()
	};
	shadowPsoDesc.PS = { nullptr, 0 };
	shadowPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	shadowPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	shadowPsoDesc.RasterizerState.DepthBias = static_cast<INT>(m_shadowSettings.DepthBias);
	shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = m_shadowSettings.SlopeScaledDepthBias;
	shadowPsoDesc.RasterizerState.DepthBiasClamp = m_shadowSettings.DepthBiasClamp;
	shadowPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	shadowPsoDesc.SampleMask = UINT_MAX;
	shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	shadowPsoDesc.NumRenderTargets = 0;
	shadowPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	shadowPsoDesc.SampleDesc.Count = 1;
	shadowPsoDesc.SampleDesc.Quality = 0;

	ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&m_directionalShadowOpaquePSO)));

	shadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["shadowAlphaCutoutPS"]->GetBufferPointer()),
		m_shaders["shadowAlphaCutoutPS"]->GetBufferSize()
	};
	ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&m_directionalShadowAlphaCutoutPSO)));
	m_directionalShadowPsoSettings = m_shadowSettings;
}
