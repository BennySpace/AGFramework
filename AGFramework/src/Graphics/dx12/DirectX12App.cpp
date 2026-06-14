#include "DirectX12App.h"
#include "../../Core/GameTimer.h"
#include "../Demo/DemoLightingController.h"
#include <cfloat>
using namespace DirectX;

namespace
{
bool IsDebugOverlayEnabled()
{
#if defined(_DEBUG)
	return true;
#else
	return false;
#endif
}

bool NearlyEqualFloat(float a, float b, float epsilon = 0.0001f)
{
	return fabsf(a - b) <= epsilon;
}

bool RequiresShadowResourceReload(const RenderSettings::ShadowSettings &currentSettings,
                                  const RenderSettings::ShadowSettings &requestedSettings)
{
	return currentSettings.CascadeCount != requestedSettings.CascadeCount ||
	       currentSettings.ShadowMapSize != requestedSettings.ShadowMapSize ||
	       !NearlyEqualFloat(currentSettings.DepthBias, requestedSettings.DepthBias) ||
	       !NearlyEqualFloat(currentSettings.SlopeScaledDepthBias, requestedSettings.SlopeScaledDepthBias) ||
	       !NearlyEqualFloat(currentSettings.DepthBiasClamp, requestedSettings.DepthBiasClamp);
}

XMVECTOR GetSafeNormalizedDirection(const XMFLOAT3 &direction, const XMVECTOR &fallbackDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))
{
	const XMVECTOR directionVector = XMLoadFloat3(&direction);
	if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return fallbackDirection;
	}

	return XMVector3Normalize(directionVector);
}

RenderSettings::CascadedShadowData BuildCascadedShadowData(const RenderSettings::ShadowSettings &shadowSettings,
                                                           const XMFLOAT3 &eyePosition, const XMFLOAT3 &lookDirection,
                                                           float cameraNearPlane, float cameraFarPlane, const XMFLOAT4X4 &projection,
                                                           const LightSystem::DirectionalLightData &directionalLight)
{
	RenderSettings::CascadedShadowData shadowData;

	const float farPlane = (std::max)(cameraNearPlane + 0.001f, shadowSettings.MaxShadowDistance);
	const std::uint32_t cascadeCount = (std::min)(shadowSettings.CascadeCount, RenderSettings::MaxShadowCascadeCount);
	const float cascadeCountF = static_cast<float>((std::max)(1u, cascadeCount));
	const float lambda = (std::max)(0.0f, (std::min)(1.0f, shadowSettings.CascadeSplitLambda));

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < RenderSettings::MaxShadowCascadeCount; ++cascadeIndex)
	{
		if (cascadeIndex >= cascadeCount)
		{
			shadowData.SplitDistances[cascadeIndex] = farPlane;
			continue;
		}

		const float splitFactor = static_cast<float>(cascadeIndex + 1) / cascadeCountF;
		const float logarithmicSplit = cameraNearPlane * powf(farPlane / cameraNearPlane, splitFactor);
		const float uniformSplit = cameraNearPlane + (farPlane - cameraNearPlane) * splitFactor;
		shadowData.SplitDistances[cascadeIndex] = lambda * logarithmicSplit + (1.0f - lambda) * uniformSplit;
	}

	const float shadowMapSize = static_cast<float>((std::max)(1u, shadowSettings.ShadowMapSize));
	shadowData.ShadowMapMetrics = XMFLOAT4(shadowMapSize, shadowMapSize, 1.0f / shadowMapSize, 1.0f / shadowMapSize);

	const XMVECTOR eye = XMLoadFloat3(&eyePosition);
	XMVECTOR safeLookDirection = XMLoadFloat3(&lookDirection);
	if (XMVector3NearEqual(safeLookDirection, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		safeLookDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}
	else
	{
		safeLookDirection = XMVector3Normalize(safeLookDirection);
	}

	const XMVECTOR target = eye + safeLookDirection;
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	const XMMATRIX proj = XMLoadFloat4x4(&projection);
	const XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

	const XMVECTOR fullFrustumCornersNdc[8] = {XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f), XMVectorSet(-1.0f, 1.0f, 0.0f, 1.0f),
	                                           XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f),   XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f),
	                                           XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f), XMVectorSet(-1.0f, 1.0f, 1.0f, 1.0f),
	                                           XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),   XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f)};

	XMVECTOR fullFrustumCornersWorld[8];
	for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		XMVECTOR cornerWorld = XMVector4Transform(fullFrustumCornersNdc[cornerIndex], invViewProj);
		cornerWorld = XMVectorScale(cornerWorld, 1.0f / XMVectorGetW(cornerWorld));
		fullFrustumCornersWorld[cornerIndex] = cornerWorld;
	}

	float cascadeNearDistance = cameraNearPlane;
	for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		const float cascadeFarDistance = shadowData.SplitDistances[cascadeIndex];
		const float nearT = (cascadeNearDistance - cameraNearPlane) / (cameraFarPlane - cameraNearPlane);
		const float farT = (cascadeFarDistance - cameraNearPlane) / (cameraFarPlane - cameraNearPlane);

		for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
		{
			const XMVECTOR nearCorner = fullFrustumCornersWorld[cornerIndex];
			const XMVECTOR farCorner = fullFrustumCornersWorld[cornerIndex + 4];
			const XMVECTOR cornerRay = farCorner - nearCorner;

			XMFLOAT3 cascadeNearCorner;
			XMFLOAT3 cascadeFarCorner;
			XMStoreFloat3(&cascadeNearCorner, nearCorner + cornerRay * nearT);
			XMStoreFloat3(&cascadeFarCorner, nearCorner + cornerRay * farT);

			shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex] = cascadeNearCorner;
			shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex + 4] = cascadeFarCorner;
		}

		cascadeNearDistance = cascadeFarDistance;
	}

	XMVECTOR lightDirection = XMLoadFloat4(&directionalLight.Direction);
	lightDirection = XMVectorSetW(lightDirection, 0.0f);
	if (XMVector3NearEqual(lightDirection, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		lightDirection = XMVectorSet(0.45f, -0.82f, 0.35f, 0.0f);
	}
	lightDirection = XMVector3Normalize(lightDirection);

	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR lightUp = worldUp;
	if (fabsf(XMVectorGetX(XMVector3Dot(lightDirection, worldUp))) > 0.99f)
	{
		lightUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		XMVECTOR cascadeCenter = XMVectorZero();
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			cascadeCenter += XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
		}
		cascadeCenter = XMVectorScale(cascadeCenter, 1.0f / 8.0f);

		float cascadeRadius = 0.0f;
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			const XMVECTOR corner = XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
			const float cornerDistance = XMVectorGetX(XMVector3Length(corner - cascadeCenter));
			cascadeRadius = (std::max)(cascadeRadius, cornerDistance);
		}
		cascadeRadius = ceilf(cascadeRadius * 16.0f) / 16.0f;

		const XMVECTOR lightPosition = cascadeCenter - lightDirection * (cascadeRadius * 2.0f + 50.0f);
		const XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, cascadeCenter, lightUp);

		const XMVECTOR cascadeCenterLightSpace = XMVector3TransformCoord(cascadeCenter, lightView);
		XMFLOAT3 cascadeCenterLight;
		XMStoreFloat3(&cascadeCenterLight, cascadeCenterLightSpace);

		XMFLOAT3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			const XMVECTOR corner = XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
			const XMVECTOR cornerLightSpace = XMVector3TransformCoord(corner, lightView);
			XMFLOAT3 cornerLight;
			XMStoreFloat3(&cornerLight, cornerLightSpace);

			minBounds.x = (std::min)(minBounds.x, cornerLight.x);
			minBounds.y = (std::min)(minBounds.y, cornerLight.y);
			minBounds.z = (std::min)(minBounds.z, cornerLight.z);
			maxBounds.x = (std::max)(maxBounds.x, cornerLight.x);
			maxBounds.y = (std::max)(maxBounds.y, cornerLight.y);
			maxBounds.z = (std::max)(maxBounds.z, cornerLight.z);
		}

		const float depthPadding = cascadeRadius * 3.0f + 100.0f;
		float left = cascadeCenterLight.x - cascadeRadius;
		float right = cascadeCenterLight.x + cascadeRadius;
		float bottom = cascadeCenterLight.y - cascadeRadius;
		float top = cascadeCenterLight.y + cascadeRadius;
		const float nearZ = (std::max)(0.1f, cascadeCenterLight.z - depthPadding);
		const float farZ = (std::max)(nearZ + 0.1f, cascadeCenterLight.z + depthPadding);

		const float shadowMapResolution = static_cast<float>((std::max)(1u, shadowSettings.ShadowMapSize));
		const float projectionWidth = right - left;
		const float projectionHeight = top - bottom;
		const float texelSizeX = projectionWidth / shadowMapResolution;
		const float texelSizeY = projectionHeight / shadowMapResolution;

		if (texelSizeX > 0.0f && texelSizeY > 0.0f)
		{
			const float centerX = 0.5f * (left + right);
			const float centerY = 0.5f * (bottom + top);
			const float snappedCenterX = floorf(centerX / texelSizeX + 0.5f) * texelSizeX;
			const float snappedCenterY = floorf(centerY / texelSizeY + 0.5f) * texelSizeY;
			const float offsetX = snappedCenterX - centerX;
			const float offsetY = snappedCenterY - centerY;

			left += offsetX;
			right += offsetX;
			bottom += offsetY;
			top += offsetY;
		}

		const XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);
		const XMMATRIX lightViewProj = lightView * lightProj;
		XMStoreFloat4x4(&shadowData.LightViewProjMatrices[cascadeIndex], lightViewProj);
	}

	return shadowData;
}

} // namespace

DirectX12App::DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd, InputDevice *inputDevice)
    : m_hAppInst(mhAppInst), m_hMainWnd(mhMainWnd), m_cameraController(mhMainWnd, inputDevice)
{
}

DirectX12App::~DirectX12App()
{
	if (IsDebugOverlayEnabled())
	{
		m_debugOverlay.Shutdown();
	}
}

bool DirectX12App::Initialize()
{
	RECT clientRect{};
	GetClientRect(m_hMainWnd, &clientRect);
	const int clientWidth = clientRect.right - clientRect.left;
	const int clientHeight = clientRect.bottom - clientRect.top;

	// Start in the curated demo look so the scene reads well before any overlay tweaks.
	Demo::DemoLightingController::ApplyRecommendedLook(m_materialSystem, m_renderSettings, m_demoSceneRuntime.GetLightEditSession());

	m_context.Initialize(m_hMainWnd, clientWidth, clientHeight, m4xMsaaState, m4xMsaaQuality, SwapChainBufferCount,
	                     DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT);

	ApplyResize(clientWidth, clientHeight);

	BeginContextRecording();

	const RenderSettings::DemoSettings demoSettings = m_renderSettings.GetDemoSettings();
	m_activeShadowSettings = m_renderSettings.GetShadowSettings();
	m_demoSceneRuntime.Initialize(m_context, demoSettings);
	m_cameraController.ApplyCameraStart(m_demoSceneRuntime.GetScene().GetInitialCamera());
	m_deferredRenderer.SetShadowSettings(m_activeShadowSettings);
	m_deferredRenderer.Initialize(m_context, m4xMsaaState, m4xMsaaQuality);
	BuildFrameResources();
	m_deferredRendererInitialized = true;
	if (IsDebugOverlayEnabled())
	{
		m_debugOverlay.Initialize(m_hMainWnd, m_context.GetDevice(), m_context.GetCommandQueue(), m_context.GetBackBufferFormat(),
		                          SwapChainBufferCount);
	}

	ExecuteAndFlushContextRecording();

	m_demoSceneRuntime.DisposeUploaders();

	return true;
}

void DirectX12App::BeginContextRecording()
{
	ThrowIfFailed(m_context.GetCommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(m_context.GetCommandAllocator(), nullptr));
}

void DirectX12App::BeginImmediateContextRecording()
{
	m_context.FlushCommandQueue();
	BeginContextRecording();
}

void DirectX12App::Update(const GameTimer &gt)
{
	ReloadSceneIfNeeded();
	ReloadShadowSettingsIfNeeded();
	m_demoSceneRuntime.ApplyFrameState(m_renderSettings.GetDemoSettings());
	m_cameraController.Update(gt);
}

void DirectX12App::Draw(const GameTimer &gt)
{
	FrameResource &frameResource = AdvanceFrameResource();
	UpdateMainPassCB(frameResource, gt);

	ThrowIfFailed(frameResource.CommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(frameResource.CommandAllocator(), nullptr));

	auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT,
	                                                           D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToRT);

	m_context.GetCommandList()->RSSetViewports(1, &m_context.GetViewport());
	m_context.GetCommandList()->RSSetScissorRects(1, &m_context.GetScissorRect());

	if (m_deferredRenderer.GetCascadedShadowMapState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		m_deferredRenderer.TransitionCascadedShadowMap(m_context, m_deferredRenderer.GetCascadedShadowMapState(),
		                                               D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_deferredRenderer.SetCascadedShadowMapState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	m_deferredRenderer.RenderShadowMapPass(m_context, frameResource, m_demoSceneRuntime.GetScene().GetSrvDescriptorHeap(),
	                                       m_context.GetCbvSrvUavDescriptorSize(), m_demoSceneRuntime.GetScene().GetGeometry(),
	                                       m_demoSceneRuntime.GetScene().GetDrawItems());

	if (m_deferredRenderer.GetGbufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		m_deferredRenderer.TransitionGbuffer(m_context, m_deferredRenderer.GetGbufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	m_deferredRenderer.RenderOpaqueGeometryStage(m_context, frameResource, m_demoSceneRuntime.GetScene().GetSrvDescriptorHeap(),
	                                             m_context.GetCbvSrvUavDescriptorSize(), m_demoSceneRuntime.GetScene().GetGeometry(),
	                                             m_demoSceneRuntime.GetScene().GetDrawItems());
	m_deferredRenderer.TransitionGbuffer(m_context, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (m_deferredRenderer.GetCascadedShadowMapState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		m_deferredRenderer.TransitionCascadedShadowMap(m_context, m_deferredRenderer.GetCascadedShadowMapState(),
		                                               D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_deferredRenderer.SetCascadedShadowMapState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	const float clearColor[] = {0.03f, 0.05f, 0.08f, 1.0f};
	m_context.GetCommandList()->ClearRenderTargetView(m_context.CurrentBackBufferView(), clearColor, 0, nullptr);
	DebugOverlay::DebugViewMode debugViewMode = DebugOverlay::DebugViewMode::Final;
	if (IsDebugOverlayEnabled())
	{
		debugViewMode = m_debugOverlay.GetDebugViewMode();
	}
	m_deferredRenderer.RenderLightingStage(m_context, frameResource, debugViewMode);
	if (IsDebugOverlayEnabled())
	{
		m_debugOverlay.Draw(m_context.GetCommandList(), gt, m_cameraController.GetEyePosition(), m_cameraController.GetLookDirection(),
		                    m_cameraController.GetYaw(), m_cameraController.GetPitch(), m_cameraController.GetMoveSpeed(),
		                    m_cameraController.GetMouseSensitivity(), m_materialSystem, m_renderSettings, m_lightSystem,
		                    m_demoSceneRuntime.GetShowcaseSession(), m_demoSceneRuntime.GetLightEditSession());
	}

	auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET,
	                                                                D3D12_RESOURCE_STATE_PRESENT);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToPresent);

	ThrowIfFailed(m_context.GetCommandList()->Close());

	ID3D12CommandList *cmds[] = {m_context.GetCommandList()};
	m_context.ExecuteCommandLists(_countof(cmds), cmds);

	m_context.Present();
	frameResource.FenceValue = m_context.SignalCommandQueue();
}

void DirectX12App::ExecuteContextRecording()
{
	ThrowIfFailed(m_context.GetCommandList()->Close());
	ID3D12CommandList *commandLists[] = {m_context.GetCommandList()};
	m_context.ExecuteCommandLists(_countof(commandLists), commandLists);
}

void DirectX12App::ExecuteAndFlushContextRecording()
{
	ExecuteContextRecording();
	m_context.FlushCommandQueue();
}

void DirectX12App::BuildFrameResources()
{
	for (auto &frameResource : m_frameResources)
	{
		frameResource = std::make_unique<FrameResource>();
		frameResource->Initialize(m_context.GetDevice(), m_deferredRenderer.GetObjectCBByteSize(),
		                          m_deferredRenderer.GetShadowPassCBByteSize());
	}
}

FrameResource &DirectX12App::AdvanceFrameResource()
{
	m_currentFrameResourceIndex = (m_currentFrameResourceIndex + 1) % SwapChainBufferCount;
	m_currentFrameResource = m_frameResources[m_currentFrameResourceIndex].get();

	if (m_currentFrameResource->FenceValue != 0 &&
	    m_context.GetCompletedFenceValue() < m_currentFrameResource->FenceValue)
	{
		m_context.WaitForFenceValue(m_currentFrameResource->FenceValue);
	}

	return *m_currentFrameResource;
}

void DirectX12App::ReloadSceneIfNeeded()
{
	const RenderSettings::DemoSettings requestedDemoSettings = m_renderSettings.GetDemoSettings();
	if (!m_demoSceneRuntime.RequiresReload(requestedDemoSettings))
	{
		return;
	}

	BeginImmediateContextRecording();

	m_demoSceneRuntime.Reload(m_context, requestedDemoSettings);

	ExecuteAndFlushContextRecording();
	m_demoSceneRuntime.DisposeUploaders();
}

void DirectX12App::ReloadShadowSettingsIfNeeded()
{
	const RenderSettings::ShadowSettings requestedShadowSettings = m_renderSettings.GetShadowSettings();
	if (!RequiresShadowResourceReload(m_activeShadowSettings, requestedShadowSettings))
	{
		return;
	}

	BeginImmediateContextRecording();

	m_deferredRenderer.SetShadowSettings(requestedShadowSettings);
	m_deferredRenderer.ReloadShadowDependentResources(m_context);

	ExecuteAndFlushContextRecording();

	m_activeShadowSettings = requestedShadowSettings;
}

void DirectX12App::ApplyResize(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		return;
	}

	m_context.Resize(width, height);
	if (m_deferredRendererInitialized)
	{
		BeginContextRecording();
		m_deferredRenderer.Resize(m_context);
		ExecuteAndFlushContextRecording();
	}

	XMMATRIX P = XMMatrixPerspectiveFovLH(m_cameraFieldOfViewY, AspectRatio(), m_cameraNearPlane, m_cameraFarPlane);
	XMStoreFloat4x4(&m_proj, P);
}

void DirectX12App::OnWindowResize(int width, int height)
{
	if (width <= 0 || height <= 0)
	{
		return;
	}

	if (width == m_context.GetClientWidth() && height == m_context.GetClientHeight())
		return;

	if (m_context.IsInitialized())
	{
		ApplyResize(width, height);
	}
}

float DirectX12App::AspectRatio() const
{
	return static_cast<float>(m_context.GetClientWidth()) / m_context.GetClientHeight();
}

void DirectX12App::UpdateMainPassCB(FrameResource &frameResource, const GameTimer &gt)
{
	m_cameraController.NormalizeLookDirection();
	m_lightSystem.Update(m_cameraController.GetEyePosition(), m_cameraController.GetLookDirection(),
	                     m_demoSceneRuntime.GetLightEditSession().GetState());

	DeferredRenderer::FrameData frameData;
	frameData.EyePos = m_cameraController.GetEyePosition();
	frameData.LookDirection = m_cameraController.GetLookDirection();
	frameData.SceneCenter = m_demoSceneRuntime.GetScene().GetSceneCenter();
	frameData.SceneScale = m_demoSceneRuntime.GetScene().GetSceneScale();
	frameData.CameraNearPlane = m_cameraNearPlane;
	frameData.Projection = m_proj;
	frameData.LightingSettings = m_renderSettings.GetLightingSettings();
	frameData.ImageBasedLightingSettings = m_renderSettings.GetImageBasedLightingSettings();
	frameData.ShadowSettings = m_renderSettings.GetShadowSettings();
	frameData.LightState = m_lightSystem.GetLightingState();
	frameData.TotalTime = gt.TotalTime();
	frameData.CascadedShadowData =
	    BuildCascadedShadowData(frameData.ShadowSettings, frameData.EyePos, frameData.LookDirection, m_cameraNearPlane, m_cameraFarPlane,
	                            frameData.Projection, m_lightSystem.GetShadowCastingDirectionalLight());
	frameData.Material = m_materialSystem.GetMaterialState();

	m_deferredRenderer.UpdateMainPassCB(frameResource, frameData);
}
