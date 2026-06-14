#pragma once

#include "d3dUtil.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include "../CameraController.h"
#include "../Demo/DemoSceneRuntime.h"
#include "DirectX12Context.h"
#include "FrameResource.h"
#include "../../Core/InputDevice.h"
#include "../DeferredRenderer.h"
#include "../Overlay/DebugOverlay.h"
#include "../LightSystem.h"
#include "../MaterialSystem.h"
#include "../RenderSettings.h"
#include "../Scene/SponzaScene.h"
#include <array>
#include <memory>

class GameTimer;

class DirectX12App
{
  public:
	DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd, InputDevice *inputDevice);
	virtual ~DirectX12App();

	virtual bool Initialize();
	virtual void Update(const GameTimer &gt);
	virtual void Draw(const GameTimer &gt);

	void OnWindowResize(int width, int height);

	float AspectRatio() const;
	void UpdateMainPassCB(FrameResource &frameResource, const GameTimer &gt);

  private:
	void ApplyResize(int width, int height);
	void BeginContextRecording();
	void BeginImmediateContextRecording();
	void BuildFrameResources();
	void ExecuteContextRecording();
	void ExecuteAndFlushContextRecording();
	FrameResource &AdvanceFrameResource();
	void ReloadSceneIfNeeded();
	void ReloadShadowSettingsIfNeeded();

  protected:
	HINSTANCE m_hAppInst = nullptr;
	HWND m_hMainWnd = nullptr;

	bool m4xMsaaState = false;
	UINT m4xMsaaQuality = 0;

	static const int SwapChainBufferCount = 2;

	LightSystem m_lightSystem;
	Demo::DemoSceneRuntime m_demoSceneRuntime;
	MaterialSystem m_materialSystem;
	RenderSettings m_renderSettings;

	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	float m_cameraFieldOfViewY = 0.25f * DirectX::XM_PI;
	float m_cameraNearPlane = 1.0f;
	float m_cameraFarPlane = 1000.0f;
	bool m_deferredRendererInitialized = false;
	RenderSettings::ShadowSettings m_activeShadowSettings;
	DirectX12Context m_context;
	DeferredRenderer m_deferredRenderer;
	std::array<std::unique_ptr<FrameResource>, SwapChainBufferCount> m_frameResources;
	FrameResource *m_currentFrameResource = nullptr;
	int m_currentFrameResourceIndex = SwapChainBufferCount - 1;
	DebugOverlay m_debugOverlay;
	CameraController m_cameraController;
};
