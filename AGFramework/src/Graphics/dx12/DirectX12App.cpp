#include "DirectX12App.h"
#include "../../Core/GameTimer.h"
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

	const float clearColor[] = { 0.03f, 0.05f, 0.08f, 1.0f };
	m_context.GetCommandList()->ClearRenderTargetView(m_context.CurrentBackBufferView(), clearColor, 0, nullptr);
	m_deferredRenderer.DrawLightingPass(m_context, m_debugOverlay.GetDebugViewMode());
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
	frameData.Material = m_materialSystem.GetMaterialState();
	frameData.LightState = m_lightSystem.GetLightingState();

	m_deferredRenderer.UpdateMainPassCB(frameData);
}
