#include "DeferredRenderer.h"

#include "dx12/DirectX12Context.h"

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
	BuildSpotShadowMap(context);
	BuildLightingSrvHeap(context);
	BuildRootSignature(context);
	BuildPSO(context, enable4xMsaa, msaaQuality);
}

void DeferredRenderer::Resize(DirectX12Context& context)
{
	BuildGbuffer(context);
	BuildCascadedShadowMap(context);
	BuildSpotShadowMap(context);
	BuildLightingSrvHeap(context);
}

void DeferredRenderer::UpdateMainPassCB(const FrameData& frameData)
{
	m_shadowSettings = frameData.ShadowSettings;
	m_spotShadowSettings = frameData.SpotShadowSettings;
	m_cascadedShadowData = frameData.CascadedShadowData;
	m_spotShadowData = frameData.SpotShadowData;
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
	const XMMATRIX spotShadowLightViewProj = XMLoadFloat4x4(&frameData.SpotShadowData.LightViewProjMatrix);
	XMStoreFloat4x4(&objectConstants.SpotShadowLightViewProj, XMMatrixTranspose(spotShadowLightViewProj));

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
	objectConstants.SpotShadowMapMetrics = frameData.SpotShadowData.ShadowMapMetrics;
	objectConstants.SpotShadowSettings0 = XMFLOAT4(
		frameData.SpotShadowSettings.EnableSpotShadows ? 1.0f : 0.0f,
		frameData.SpotShadowSettings.PcfRadius,
		frameData.SpotShadowSettings.ShadowStrength,
		0.0f);
	objectConstants.SpotShadowSettings1 = XMFLOAT4(
		frameData.SpotShadowData.NearZ,
		frameData.SpotShadowData.FarZ,
		0.0f,
		0.0f);
	objectConstants.SpotShadowSettings2 = XMFLOAT4(
		frameData.SpotShadowSettings.ReceiverBiasMin,
		frameData.SpotShadowSettings.ReceiverBiasSlopeScale,
		frameData.SpotShadowSettings.ReceiverBiasTexelFactor,
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

void DeferredRenderer::BuildSpotShadowMap(DirectX12Context& context)
{
	SpotShadowMap::Desc shadowMapDesc;
	shadowMapDesc.Width = m_spotShadowSettings.ShadowMapSize;
	shadowMapDesc.Height = m_spotShadowSettings.ShadowMapSize;

	if (m_spotShadowMap != nullptr)
	{
		const SpotShadowMap::Desc& currentDesc = m_spotShadowMap->GetDesc();
		if (currentDesc.Width == shadowMapDesc.Width &&
			currentDesc.Height == shadowMapDesc.Height &&
			currentDesc.ResourceFormat == shadowMapDesc.ResourceFormat &&
			currentDesc.DsvFormat == shadowMapDesc.DsvFormat &&
			currentDesc.SrvFormat == shadowMapDesc.SrvFormat)
		{
			return;
		}
	}

	if (!m_spotShadowMap)
	{
		m_spotShadowMap = std::make_unique<SpotShadowMap>();
	}

	if (!m_spotShadowMap->Initialize(context.GetDevice(), shadowMapDesc))
	{
		throw std::runtime_error("Failed to initialize spot shadow map.");
	}

	m_spotShadowMapState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	m_lightingSrvHeapDirty = true;
}

void DeferredRenderer::BuildLightingSrvHeap(DirectX12Context& context)
{
	if (m_gbuffer == nullptr || m_cascadedShadowMap == nullptr || m_spotShadowMap == nullptr)
	{
		return;
	}

	if (m_lightingSrvHeap != nullptr && !m_lightingSrvHeapDirty)
	{
		return;
	}

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 5;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(m_lightingSrvHeap.ReleaseAndGetAddressOf())));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_lightingSrvHeap->GetCPUDescriptorHandleForHeapStart());
	const DXGI_FORMAT gbufferFormats[] =
	{
		m_gbuffer->GetFormat(Gbuffer::Target::Albedo),
		m_gbuffer->GetFormat(Gbuffer::Target::Normal),
		m_gbuffer->GetFormat(Gbuffer::Target::Position)
	};
	ID3D12Resource* gbufferResources[] =
	{
		m_gbuffer->GetResource(Gbuffer::Target::Albedo),
		m_gbuffer->GetResource(Gbuffer::Target::Normal),
		m_gbuffer->GetResource(Gbuffer::Target::Position)
	};

	for (int targetIndex = 0; targetIndex < 3; ++targetIndex)
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

	D3D12_SHADER_RESOURCE_VIEW_DESC spotShadowSrvDesc = {};
	spotShadowSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	spotShadowSrvDesc.Format = m_spotShadowMap->GetDesc().SrvFormat;
	spotShadowSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	spotShadowSrvDesc.Texture2D.MostDetailedMip = 0;
	spotShadowSrvDesc.Texture2D.MipLevels = 1;
	spotShadowSrvDesc.Texture2D.PlaneSlice = 0;
	spotShadowSrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	context.GetDevice()->CreateShaderResourceView(m_spotShadowMap->GetResource(), &spotShadowSrvDesc, handle);
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

void DeferredRenderer::RenderSpotShadowMapPass(
	DirectX12Context& context,
	ID3D12DescriptorHeap* srvDescriptorHeap,
	UINT cbvSrvUavDescriptorSize,
	const MeshGeometry& sceneGeometry,
	const std::vector<ModelDrawItem>& drawItems)
{
	BuildSpotShadowMap(context);
	BuildShadowPSOs(context);

	if (!m_spotShadowSettings.EnableSpotShadows || m_spotShadowMap == nullptr)
	{
		return;
	}

	ID3D12GraphicsCommandList* commandList = context.GetCommandList();
	commandList->SetGraphicsRootSignature(m_shadowRootSignature.Get());
	commandList->RSSetViewports(1, &m_spotShadowMap->GetViewport());
	commandList->RSSetScissorRects(1, &m_spotShadowMap->GetScissorRect());
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
	const XMMATRIX lightViewProj = XMLoadFloat4x4(&m_spotShadowData.LightViewProjMatrix);
	const XMMATRIX worldLightViewProj = world * lightViewProj;

	ShadowPassConstants shadowConstants;
	XMStoreFloat4x4(&shadowConstants.WorldLightViewProj, XMMatrixTranspose(worldLightViewProj));
	XMStoreFloat4x4(&shadowConstants.TexTransform, XMMatrixTranspose(texTransform));

	memcpy(m_mappedShadowPassCB + m_spotShadowPassCBOffset, &shadowConstants, sizeof(shadowConstants));
	commandList->SetGraphicsRootConstantBufferView(0, m_shadowPassCB->GetGPUVirtualAddress() + m_spotShadowPassCBOffset);

	const D3D12_CPU_DESCRIPTOR_HANDLE spotDsv = m_spotShadowMap->GetDsv();
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &spotDsv);
	m_spotShadowMap->Clear(commandList);

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
			commandList->SetPipelineState(m_spotShadowAlphaCutoutPSO.Get());
			commandList->SetGraphicsRootDescriptorTable(1, textureHandle);
		}
		else
		{
			commandList->SetPipelineState(m_spotShadowOpaquePSO.Get());
		}

		const auto& submesh = sceneGeometry.DrawArgs.at(drawItem.DrawName);
		commandList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, submesh.BaseVertexLocation, 0);
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

	D3D12_CPU_DESCRIPTOR_HANDLE gbufferRtvs[3] =
	{
		m_gbuffer->GetRtv(Gbuffer::Target::Albedo),
		m_gbuffer->GetRtv(Gbuffer::Target::Normal),
		m_gbuffer->GetRtv(Gbuffer::Target::Position)
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

void DeferredRenderer::TransitionSpotShadowMap(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	if (m_spotShadowMap == nullptr || beforeState == afterState)
	{
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		m_spotShadowMap->GetResource(),
		beforeState,
		afterState);
	context.GetCommandList()->ResourceBarrier(1, &barrier);
}

void DeferredRenderer::TransitionGbuffer(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
	D3D12_RESOURCE_BARRIER barriers[3] =
	{
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Albedo), beforeState, afterState),
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Normal), beforeState, afterState),
		CD3DX12_RESOURCE_BARRIER::Transition(m_gbuffer->GetResource(Gbuffer::Target::Position), beforeState, afterState)
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
	m_spotShadowPassCBOffset = m_shadowPassCBStride * RenderSettings::MaxShadowCascadeCount;
	m_shadowPassCBByteSize = m_shadowPassCBStride * (RenderSettings::MaxShadowCascadeCount + 1);

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
	CD3DX12_STATIC_SAMPLER_DESC lightingStaticSamplers[] = { linearWrapSampler, shadowComparisonSampler };

	CD3DX12_DESCRIPTOR_RANGE geometryTexTable;
	geometryTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER geometryRootParameters[3];
	geometryRootParameters[0].InitAsConstantBufferView(0);
	geometryRootParameters[1].InitAsDescriptorTable(1, &geometryTexTable, D3D12_SHADER_VISIBILITY_PIXEL);
	geometryRootParameters[2].InitAsConstants(4, 1);

	CD3DX12_ROOT_SIGNATURE_DESC geometryRootSigDesc(
		3,
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
	lightingTexTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);

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
	geometryPsoDesc.NumRenderTargets = 3;
	geometryPsoDesc.RTVFormats[0] = m_gbuffer->GetFormat(Gbuffer::Target::Albedo);
	geometryPsoDesc.RTVFormats[1] = m_gbuffer->GetFormat(Gbuffer::Target::Normal);
	geometryPsoDesc.RTVFormats[2] = m_gbuffer->GetFormat(Gbuffer::Target::Position);
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
	const auto buildShadowPsoPair =
		[&context, this](
			float depthBias,
			float slopeScaledDepthBias,
			float depthBiasClamp,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>& opaquePso,
			Microsoft::WRL::ComPtr<ID3D12PipelineState>& alphaCutoutPso)
		{
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
			shadowPsoDesc.RasterizerState.DepthBias = static_cast<INT>(depthBias);
			shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = slopeScaledDepthBias;
			shadowPsoDesc.RasterizerState.DepthBiasClamp = depthBiasClamp;
			shadowPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			shadowPsoDesc.SampleMask = UINT_MAX;
			shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			shadowPsoDesc.NumRenderTargets = 0;
			shadowPsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
			shadowPsoDesc.SampleDesc.Count = 1;
			shadowPsoDesc.SampleDesc.Quality = 0;

			ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&opaquePso)));

			shadowPsoDesc.PS =
			{
				reinterpret_cast<BYTE*>(m_shaders["shadowAlphaCutoutPS"]->GetBufferPointer()),
				m_shaders["shadowAlphaCutoutPS"]->GetBufferSize()
			};
			ThrowIfFailed(context.GetDevice()->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&alphaCutoutPso)));
		};

	if (m_directionalShadowOpaquePSO == nullptr ||
		m_directionalShadowAlphaCutoutPSO == nullptr ||
		m_directionalShadowPsoSettings.DepthBias != m_shadowSettings.DepthBias ||
		m_directionalShadowPsoSettings.SlopeScaledDepthBias != m_shadowSettings.SlopeScaledDepthBias ||
		m_directionalShadowPsoSettings.DepthBiasClamp != m_shadowSettings.DepthBiasClamp)
	{
		buildShadowPsoPair(
			m_shadowSettings.DepthBias,
			m_shadowSettings.SlopeScaledDepthBias,
			m_shadowSettings.DepthBiasClamp,
			m_directionalShadowOpaquePSO,
			m_directionalShadowAlphaCutoutPSO);
		m_directionalShadowPsoSettings = m_shadowSettings;
	}

	if (m_spotShadowOpaquePSO == nullptr ||
		m_spotShadowAlphaCutoutPSO == nullptr ||
		m_spotShadowPsoSettings.DepthBias != m_spotShadowSettings.DepthBias ||
		m_spotShadowPsoSettings.SlopeScaledDepthBias != m_spotShadowSettings.SlopeScaledDepthBias ||
		m_spotShadowPsoSettings.DepthBiasClamp != m_spotShadowSettings.DepthBiasClamp)
	{
		buildShadowPsoPair(
			m_spotShadowSettings.DepthBias,
			m_spotShadowSettings.SlopeScaledDepthBias,
			m_spotShadowSettings.DepthBiasClamp,
			m_spotShadowOpaquePSO,
			m_spotShadowAlphaCutoutPSO);
		m_spotShadowPsoSettings = m_spotShadowSettings;
	}
}
