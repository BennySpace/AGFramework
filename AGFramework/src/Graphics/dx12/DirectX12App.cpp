#include "DirectX12App.h"
#include "../../Core/GameTimer.h"
#include "../Demo/DemoLightingController.h"
#include "../FrameDataBuilder.h"
#include "../RuntimeSettingsCoordinator.h"
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
} // namespace

DirectX12App::DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd, InputDevice *inputDevice)
    : m_hAppInst(mhAppInst), m_hMainWnd(mhMainWnd), m_cameraController(mhMainWnd, inputDevice)
{
}

DirectX12App::~DirectX12App()
{
	m_cameraController.ReleaseMouseCapture();
	m_context.Shutdown();

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
	m_activeShadowSettings = RuntimeSettingsCoordinator::InitializeShadowSettings(m_deferredRenderer, m_renderSettings);
	m_demoSceneRuntime.Initialize(m_context, demoSettings);
	m_cameraController.ApplyCameraStart(m_demoSceneRuntime.GetScene().GetInitialCamera());
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
	RuntimeSettingsCoordinator::ApplyFrameState(m_demoSceneRuntime, m_renderSettings);
	m_cameraController.Update(gt);
}

void DirectX12App::Draw(const GameTimer &gt)
{
	FrameResource &frameResource = AdvanceFrameResource();
	UpdateMainPassCB(frameResource, gt);
	BeginFrameRendering(frameResource);
	RenderShadowStage(frameResource);
	RenderDeferredGeometryStage(frameResource);
	RenderLightingAndOverlay(frameResource, gt);
	EndFrameRendering(frameResource);
}

void DirectX12App::BeginFrameRendering(FrameResource &frameResource)
{
	ThrowIfFailed(frameResource.CommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(frameResource.CommandAllocator(), nullptr));

	auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(m_context.CurrentBackBuffer(),
	                                                           D3D12_RESOURCE_STATE_PRESENT,
	                                                           D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToRT);

	m_context.GetCommandList()->RSSetViewports(1, &m_context.GetViewport());
	m_context.GetCommandList()->RSSetScissorRects(1, &m_context.GetScissorRect());
}

void DirectX12App::RenderShadowStage(FrameResource &frameResource)
{
	if (m_deferredRenderer.GetCascadedShadowMapState() != D3D12_RESOURCE_STATE_DEPTH_WRITE)
	{
		m_deferredRenderer.TransitionCascadedShadowMap(m_context, m_deferredRenderer.GetCascadedShadowMapState(),
		                                               D3D12_RESOURCE_STATE_DEPTH_WRITE);
		m_deferredRenderer.SetCascadedShadowMapState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	}
	m_deferredRenderer.RenderShadowMapPass(m_context, frameResource, m_demoSceneRuntime.GetScene().GetSrvDescriptorHeap(),
	                                       m_context.GetCbvSrvUavDescriptorSize(), m_demoSceneRuntime.GetScene().GetGeometry(),
	                                       m_demoSceneRuntime.GetScene().GetDrawItems());
}

void DirectX12App::RenderDeferredGeometryStage(FrameResource &frameResource)
{
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
}

void DirectX12App::RenderLightingAndOverlay(FrameResource &frameResource, const GameTimer &gt)
{
	const float clearColor[] = {0.03f, 0.05f, 0.08f, 1.0f};
	m_context.GetCommandList()->ClearRenderTargetView(m_context.CurrentBackBufferView(), clearColor, 0, nullptr);
	DebugOverlay::DebugViewMode debugViewMode = DebugOverlay::DebugViewMode::Final;
	if (IsDebugOverlayEnabled())
	{
		debugViewMode = m_debugOverlay.ResolveDebugViewMode();
	}
	m_deferredRenderer.RenderLightingStage(m_context, frameResource, debugViewMode);
	if (IsDebugOverlayEnabled())
	{
		m_debugOverlay.Draw(m_context.GetCommandList(), BuildDebugOverlayFrameContext(gt));
	}
}

DebugOverlay::FrameContext DirectX12App::BuildDebugOverlayFrameContext(const GameTimer &gt)
{
	DebugOverlay::FrameContext frameContext;
	frameContext.Timer = &gt;
	frameContext.Camera.EyePosition = &m_cameraController.GetEyePosition();
	frameContext.Camera.LookDirection = &m_cameraController.GetLookDirection();
	frameContext.Camera.Yaw = &m_cameraController.GetYaw();
	frameContext.Camera.Pitch = &m_cameraController.GetPitch();
	frameContext.Camera.MoveSpeed = &m_cameraController.GetMoveSpeed();
	frameContext.Camera.MouseSensitivity = &m_cameraController.GetMouseSensitivity();
	frameContext.Material = &m_materialSystem;
	frameContext.Render = &m_renderSettings;
	frameContext.Light = &m_lightSystem;
	frameContext.Showcase = &m_demoSceneRuntime.GetShowcaseSession();
	frameContext.LightEdit = &m_demoSceneRuntime.GetLightEditSession();
	return frameContext;
}

void DirectX12App::EndFrameRendering(FrameResource &frameResource)
{
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
	RuntimeSettingsCoordinator::ReloadSceneIfNeeded(
	    m_context, m_demoSceneRuntime, m_renderSettings,
	    {.BeginReload = [this]() { BeginImmediateContextRecording(); },
	     .EndReload = [this]() { ExecuteAndFlushContextRecording(); }});
}

void DirectX12App::ReloadShadowSettingsIfNeeded()
{
	RuntimeSettingsCoordinator::ReloadShadowSettingsIfNeeded(
	    m_context, m_deferredRenderer, m_renderSettings, m_activeShadowSettings,
	    {.BeginReload = [this]() { BeginImmediateContextRecording(); },
	     .EndReload = [this]() { ExecuteAndFlushContextRecording(); }});
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

void DirectX12App::OnPauseChanged(bool isPaused)
{
	if (isPaused)
	{
		m_cameraController.ReleaseMouseCapture();
	}
}

float DirectX12App::AspectRatio() const
{
	return static_cast<float>(m_context.GetClientWidth()) / m_context.GetClientHeight();
}

void DirectX12App::UpdateMainPassCB(FrameResource &frameResource, const GameTimer &gt)
{
	const XMFLOAT3 normalizedLookDirection = FrameDataBuilder::NormalizeLookDirection(m_cameraController.GetLookDirection());
	m_cameraController.GetLookDirection() = normalizedLookDirection;
	m_lightSystem.Update(m_cameraController.GetEyePosition(), normalizedLookDirection, m_demoSceneRuntime.GetLightEditSession().GetState());

	FrameDataBuilder::Inputs frameDataInputs;
	frameDataInputs.EyePos = m_cameraController.GetEyePosition();
	frameDataInputs.LookDirection = normalizedLookDirection;
	frameDataInputs.SceneCenter = m_demoSceneRuntime.GetScene().GetSceneCenter();
	frameDataInputs.SceneScale = m_demoSceneRuntime.GetScene().GetSceneScale();
	frameDataInputs.CameraNearPlane = m_cameraNearPlane;
	frameDataInputs.CameraFarPlane = m_cameraFarPlane;
	frameDataInputs.Projection = m_proj;
	frameDataInputs.LightingSettings = m_renderSettings.GetLightingSettings();
	frameDataInputs.ImageBasedLightingSettings = m_renderSettings.GetImageBasedLightingSettings();
	frameDataInputs.ShadowSettings = m_renderSettings.GetShadowSettings();
	frameDataInputs.LightState = m_lightSystem.GetLightingState();
	frameDataInputs.Material = m_materialSystem.GetMaterialState();
	frameDataInputs.TotalTime = gt.TotalTime();

	const DeferredRenderer::FrameData frameData = FrameDataBuilder::BuildFrameData(frameDataInputs);
	m_deferredRenderer.UpdateMainPassCB(frameResource, frameData);
}
