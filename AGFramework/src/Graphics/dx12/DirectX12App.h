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

#include "DirectX12Context.h"
#include "FrameResource.h"
#include "../../Core/InputDevice.h"
#include "../Demo/DemoLightEditSession.h"
#include "../Demo/DemoShowcaseSession.h"
#include "../Demo/DemoSceneComposer.h"
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

	void UpdateCamera(const GameTimer &gt);
	void UpdateMouseCaptureState();
	void UpdateMouseLook();
	void UpdateMainPassCB(FrameResource &frameResource, const GameTimer &gt);

  private:
	void ApplyResize(int width, int height);
	void BuildFrameResources();
	FrameResource &AdvanceFrameResource();
	void ReloadSceneIfNeeded();
	void ReloadShadowSettingsIfNeeded();

  protected:
	HINSTANCE m_hAppInst = nullptr;
	HWND m_hMainWnd = nullptr;
	InputDevice *m_inputDevice = nullptr;

	bool m4xMsaaState = false;
	UINT m4xMsaaQuality = 0;

	static const int SwapChainBufferCount = 2;

	LightSystem m_lightSystem;
	Demo::DemoSceneComposer m_demoSceneComposer;
	Demo::DemoShowcaseSession m_demoShowcaseSession;
	Demo::DemoLightEditSession m_demoLightEditSession;
	MaterialSystem m_materialSystem;
	RenderSettings m_renderSettings;

	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 m_eyePos = {0.0f, 8.0f, -30.0f};
	DirectX::XMFLOAT3 m_lookDirection = {0.0f, 0.0f, 1.0f};
	float m_cameraFieldOfViewY = 0.25f * DirectX::XM_PI;
	float m_cameraNearPlane = 1.0f;
	float m_cameraFarPlane = 1000.0f;
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	float m_cameraMoveSpeed = 10.0f;
	float m_cameraMouseSensitivity = 0.0035f;
	bool m_isMouseCaptured = false;
	bool m_deferredRendererInitialized = false;
	RenderSettings::DemoSettings m_activeDemoSettings;
	RenderSettings::ShadowSettings m_activeShadowSettings;
	DirectX12Context m_context;
	SponzaScene m_scene;
	DeferredRenderer m_deferredRenderer;
	std::array<std::unique_ptr<FrameResource>, SwapChainBufferCount> m_frameResources;
	FrameResource *m_currentFrameResource = nullptr;
	int m_currentFrameResourceIndex = SwapChainBufferCount - 1;
	DebugOverlay m_debugOverlay;
};
