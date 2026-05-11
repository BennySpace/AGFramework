#include "DirectX12App.h"
#include "../../Core/GameTimer.h"
#include <cfloat>
using namespace DirectX;

namespace
{
	XMVECTOR GetSafeNormalizedDirection(const XMFLOAT3& direction, const XMVECTOR& fallbackDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))
	{
		const XMVECTOR directionVector = XMLoadFloat3(&direction);
		if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
		{
			return fallbackDirection;
		}

		return XMVector3Normalize(directionVector);
	}

	RenderSettings::CascadedShadowData BuildCascadedShadowData(
		const RenderSettings::ShadowSettings& shadowSettings,
		const XMFLOAT3& eyePosition,
		const XMFLOAT3& lookDirection,
		const XMFLOAT4X4& projection,
		const LightSystem::DirectionalLightData& directionalLight)
	{
		RenderSettings::CascadedShadowData shadowData;

		const float cameraNearPlane = 1.0f;
		const float cameraFarPlane = 1000.0f;
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
		shadowData.ShadowMapMetrics = XMFLOAT4(
			shadowMapSize,
			shadowMapSize,
			1.0f / shadowMapSize,
			1.0f / shadowMapSize);

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

		const XMVECTOR fullFrustumCornersNdc[8] =
		{
			XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 0.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 0.0f, 1.0f),
			XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
			XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f,  1.0f, 1.0f, 1.0f),
			XMVectorSet( 1.0f, -1.0f, 1.0f, 1.0f)
		};

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

	RenderSettings::SpotShadowData BuildSpotShadowData(
		const RenderSettings::SpotShadowSettings& shadowSettings,
		const LightSystem::SpotLightData& spotLight)
	{
		RenderSettings::SpotShadowData shadowData;

		const float shadowMapSize = static_cast<float>((std::max)(1u, shadowSettings.ShadowMapSize));
		shadowData.ShadowMapMetrics = XMFLOAT4(
			shadowMapSize,
			shadowMapSize,
			1.0f / shadowMapSize,
			1.0f / shadowMapSize);

		shadowData.LightPosition = XMFLOAT3(
			spotLight.Position.x,
			spotLight.Position.y,
			spotLight.Position.z);

		const XMFLOAT3 rawDirection(
			spotLight.Direction.x,
			spotLight.Direction.y,
			spotLight.Direction.z);
		const XMVECTOR safeDirectionVector = GetSafeNormalizedDirection(rawDirection);
		XMStoreFloat3(&shadowData.LightDirection, safeDirectionVector);

		const float nearZ = 0.1f;
		const float farZ = (std::max)(spotLight.Params.x, nearZ + 0.1f);
		shadowData.NearZ = nearZ;
		shadowData.FarZ = farZ;

		const float outerConeCosine = (std::max)(-0.999f, (std::min)(0.999f, spotLight.Params.z));
		const float fieldOfViewY = 2.0f * acosf(outerConeCosine);
		const XMVECTOR lightPosition = XMLoadFloat3(&shadowData.LightPosition);

		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if (fabsf(XMVectorGetX(XMVector3Dot(safeDirectionVector, up))) > 0.99f)
		{
			up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		}

		const XMMATRIX lightView = XMMatrixLookToLH(lightPosition, safeDirectionVector, up);
		const XMMATRIX lightProj = XMMatrixPerspectiveFovLH(fieldOfViewY, 1.0f, nearZ, farZ);
		XMStoreFloat4x4(&shadowData.LightViewProjMatrix, lightView * lightProj);

		return shadowData;
	}
}

DirectX12App::DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd) : m_hAppInst(mhAppInst), m_hMainWnd(mhMainWnd)
{
}

DirectX12App::~DirectX12App()
{
	m_debugOverlay.Shutdown();
}

bool DirectX12App::Initialize()
{
	RECT clientRect{};
	GetClientRect(m_hMainWnd, &clientRect);
	const int clientWidth = clientRect.right - clientRect.left;
	const int clientHeight = clientRect.bottom - clientRect.top;

	m_context.Initialize(
		m_hMainWnd,
		clientWidth,
		clientHeight,
		m4xMsaaState,
		m4xMsaaQuality,
		SwapChainBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_D24_UNORM_S8_UINT);

	ApplyResize(clientWidth, clientHeight);

	ThrowIfFailed(m_context.GetCommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(m_context.GetCommandAllocator(), nullptr));

	m_scene.Initialize(m_context);
	m_eyePos = m_scene.GetInitialCamera().EyePos;
	m_lookDirection = m_scene.GetInitialCamera().LookDirection;
	m_yaw = m_scene.GetInitialCamera().Yaw;
	m_pitch = m_scene.GetInitialCamera().Pitch;
	m_deferredRenderer.Initialize(m_context, m4xMsaaState, m4xMsaaQuality);
	m_debugOverlay.Initialize(
		m_hMainWnd,
		m_context.GetDevice(),
		m_context.GetCommandQueue(),
		m_context.GetBackBufferFormat(),
		SwapChainBufferCount);

	ThrowIfFailed(m_context.GetCommandList()->Close());
	ID3D12CommandList* initCmdsLists[] = { m_context.GetCommandList() };
	m_context.ExecuteCommandLists(_countof(initCmdsLists), initCmdsLists);
	m_context.FlushCommandQueue();

	m_scene.DisposeUploaders();

	return true;
}

void DirectX12App::Update(const GameTimer& gt)
{
	UpdateMouseCaptureState();
	UpdateMouseLook();
	UpdateCamera(gt);
	UpdateMainPassCB(gt);
}

void DirectX12App::Draw(const GameTimer& gt)
{
	(void)gt;
	ThrowIfFailed(m_context.GetCommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(m_context.GetCommandAllocator(), nullptr));

	auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(
		m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToRT);

	m_context.GetCommandList()->RSSetViewports(1, &m_context.GetViewport());
	m_context.GetCommandList()->RSSetScissorRects(1, &m_context.GetScissorRect());

	if (m_deferredRenderer.GetCascadedShadowMapState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		m_deferredRenderer.TransitionCascadedShadowMap(
			m_context,
			m_deferredRenderer.GetCascadedShadowMapState(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_deferredRenderer.SetCascadedShadowMapState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	m_deferredRenderer.RenderShadowMapPass(
		m_context,
		m_scene.GetSrvDescriptorHeap(),
		m_context.GetCbvSrvUavDescriptorSize(),
		m_scene.GetGeometry(),
		m_scene.GetDrawItems());
	if (m_deferredRenderer.GetSpotShadowMapState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		m_deferredRenderer.TransitionSpotShadowMap(
			m_context,
			m_deferredRenderer.GetSpotShadowMapState(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_deferredRenderer.SetSpotShadowMapState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	m_deferredRenderer.RenderSpotShadowMapPass(
		m_context,
		m_scene.GetSrvDescriptorHeap(),
		m_context.GetCbvSrvUavDescriptorSize(),
		m_scene.GetGeometry(),
		m_scene.GetDrawItems());

	if (m_deferredRenderer.GetGbufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		m_deferredRenderer.TransitionGbuffer(m_context, m_deferredRenderer.GetGbufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	m_deferredRenderer.DrawGeometryPass(
		m_context,
		m_scene.GetSrvDescriptorHeap(),
		m_context.GetCbvSrvUavDescriptorSize(),
		m_scene.GetGeometry(),
		m_scene.GetDrawItems());
	m_deferredRenderer.TransitionGbuffer(m_context, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	if (m_deferredRenderer.GetCascadedShadowMapState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		m_deferredRenderer.TransitionCascadedShadowMap(
			m_context,
			m_deferredRenderer.GetCascadedShadowMapState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_deferredRenderer.SetCascadedShadowMapState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}
	if (m_deferredRenderer.GetSpotShadowMapState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
	{
		m_deferredRenderer.TransitionSpotShadowMap(
			m_context,
			m_deferredRenderer.GetSpotShadowMapState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_deferredRenderer.SetSpotShadowMapState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	const float clearColor[] = { 0.03f, 0.05f, 0.08f, 1.0f };
	m_context.GetCommandList()->ClearRenderTargetView(m_context.CurrentBackBufferView(), clearColor, 0, nullptr);
	m_deferredRenderer.DrawLightingPass(
		m_context,
		m_debugOverlay.GetDebugViewMode(),
		m_debugOverlay.GetShadowDebugCascadeIndex());
	m_debugOverlay.Draw(
		m_context.GetCommandList(),
		gt,
		m_eyePos,
		m_lookDirection,
		m_yaw,
		m_pitch,
		m_cameraMoveSpeed,
		m_cameraMouseSensitivity,
		m_materialSystem,
		m_renderSettings,
		m_lightSystem);

	auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToPresent);

	ThrowIfFailed(m_context.GetCommandList()->Close());

	ID3D12CommandList* cmds[] = { m_context.GetCommandList() };
	m_context.ExecuteCommandLists(_countof(cmds), cmds);

	m_context.Present();
	m_context.FlushCommandQueue();
}

void DirectX12App::ApplyResize(int width, int height)
{
	m_context.Resize(width, height);
	m_deferredRenderer.Resize(m_context);

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_proj, P);
}

void DirectX12App::OnWindowResize(int width, int height)
{
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

void DirectX12App::UpdateCamera(const GameTimer& gt)
{
	const float sprintMultiplier = 3.0f;
	const float moveSpeed = m_cameraMoveSpeed *
		(d3dUtil::IsKeyDown(VK_SHIFT) ? sprintMultiplier : 1.0f) *
		gt.DeltaTime();

	XMVECTOR eyePosition = XMLoadFloat3(&m_eyePos);
	XMVECTOR lookDirection = GetSafeNormalizedDirection(m_lookDirection);
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR rightDirection = XMVector3Normalize(XMVector3Cross(worldUp, lookDirection));

	if (d3dUtil::IsKeyDown('W'))
	{
		eyePosition += lookDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('S'))
	{
		eyePosition -= lookDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('A'))
	{
		eyePosition -= rightDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('D'))
	{
		eyePosition += rightDirection * moveSpeed;
	}

	XMStoreFloat3(&m_eyePos, eyePosition);
}

void DirectX12App::UpdateMouseCaptureState()
{
	const bool isWindowFocused = GetForegroundWindow() == m_hMainWnd;
	const bool isRightMouseDown = d3dUtil::IsKeyDown(VK_RBUTTON);
	const bool shouldCaptureMouse = isWindowFocused && isRightMouseDown;

	if (shouldCaptureMouse && !m_isMouseCaptured)
	{
		RECT clientRect{};
		GetClientRect(m_hMainWnd, &clientRect);

		POINT topLeft{ clientRect.left, clientRect.top };
		POINT bottomRight{ clientRect.right, clientRect.bottom };
		ClientToScreen(m_hMainWnd, &topLeft);
		ClientToScreen(m_hMainWnd, &bottomRight);

		RECT clipRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
		ClipCursor(&clipRect);
		ShowCursor(FALSE);

		const int centerX = (clipRect.left + clipRect.right) / 2;
		const int centerY = (clipRect.top + clipRect.bottom) / 2;
		SetCursorPos(centerX, centerY);

		m_isMouseCaptured = true;
	}
	else if (!shouldCaptureMouse && m_isMouseCaptured)
	{
		ClipCursor(nullptr);
		ShowCursor(TRUE);
		m_isMouseCaptured = false;
	}
}

void DirectX12App::UpdateMouseLook()
{
	if (!m_isMouseCaptured)
	{
		return;
	}

	RECT clientRect{};
	GetClientRect(m_hMainWnd, &clientRect);

	POINT centerPoint{
		(clientRect.left + clientRect.right) / 2,
		(clientRect.top + clientRect.bottom) / 2
	};
	ClientToScreen(m_hMainWnd, &centerPoint);

	POINT currentMousePosition{};
	if (!GetCursorPos(&currentMousePosition))
	{
		return;
	}

	const float deltaX = static_cast<float>(currentMousePosition.x - centerPoint.x);
	const float deltaY = static_cast<float>(currentMousePosition.y - centerPoint.y);

	m_yaw += deltaX * m_cameraMouseSensitivity;
	m_pitch += deltaY * m_cameraMouseSensitivity;
	m_pitch = (std::max)(-1.45f, (std::min)(1.45f, m_pitch));

	const float cosPitch = cosf(m_pitch);
	m_lookDirection.x = sinf(m_yaw) * cosPitch;
	m_lookDirection.y = -sinf(m_pitch);
	m_lookDirection.z = cosf(m_yaw) * cosPitch;

	SetCursorPos(centerPoint.x, centerPoint.y);
}

void DirectX12App::UpdateMainPassCB(const GameTimer& gt)
{
	(void)gt;

	const XMVECTOR safeLookDirection = GetSafeNormalizedDirection(m_lookDirection);
	XMStoreFloat3(&m_lookDirection, safeLookDirection);
	m_lightSystem.Update(m_eyePos, m_lookDirection);

	DeferredRenderer::FrameData frameData;
	frameData.EyePos = m_eyePos;
	frameData.LookDirection = m_lookDirection;
	frameData.SceneCenter = m_scene.GetSceneCenter();
	frameData.SceneScale = m_scene.GetSceneScale();
	frameData.Projection = m_proj;
	frameData.LightingSettings = m_renderSettings.GetLightingSettings();
	frameData.ShadowSettings = m_renderSettings.GetShadowSettings();
	frameData.SpotShadowSettings = m_renderSettings.GetSpotShadowSettings();
	frameData.LightState = m_lightSystem.GetLightingState();
	frameData.CascadedShadowData = BuildCascadedShadowData(
		frameData.ShadowSettings,
		frameData.EyePos,
		frameData.LookDirection,
		frameData.Projection,
		m_lightSystem.GetShadowCastingDirectionalLight());
	frameData.SpotShadowData = BuildSpotShadowData(
		frameData.SpotShadowSettings,
		frameData.LightState.SpotLights[0]);
	frameData.Material = m_materialSystem.GetMaterialState();

	m_deferredRenderer.UpdateMainPassCB(frameData);
}
