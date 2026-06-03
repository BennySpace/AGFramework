#pragma once

#include "../dx12/d3dUtil.h"

class GameTimer;
namespace Demo
{
class DemoLightEditSession;
class DemoShowcaseSession;
}
class MaterialSystem;
class RenderSettings;
class LightSystem;

class DebugOverlay
{
  public:
	enum class DebugViewMode
	{
		Final = 0,
		Albedo = 1,
		Normal = 2,
		ShadowCascade = 4,
		ShadowFactor = 5,
		Metallic = 8,
		Roughness = 9,
		AmbientOcclusion = 10,
		DirectLighting = 12,
		AmbientLighting = 13
	};

	DebugOverlay() = default;

	void Initialize(HWND windowHandle, ID3D12Device *device, ID3D12CommandQueue *commandQueue, DXGI_FORMAT backBufferFormat,
	                UINT framesInFlight);
	void Shutdown();
	void Draw(ID3D12GraphicsCommandList *commandList, const GameTimer &gameTimer, DirectX::XMFLOAT3 &eyePosition,
	          DirectX::XMFLOAT3 &lookDirection, float &yaw, float &pitch, float &cameraMoveSpeed, float &cameraMouseSensitivity,
	          MaterialSystem &materialSystem, RenderSettings &renderSettings, LightSystem &lightSystem, Demo::DemoShowcaseSession &showcaseSession,
	          Demo::DemoLightEditSession &lightEditSession);

	DebugViewMode GetDebugViewMode() const
	{
		return m_debugViewMode;
	}

  private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap;
	bool m_isInitialized = false;
	bool m_isResizingSidebar = false;
	DebugViewMode m_debugViewMode = DebugViewMode::Final;
	bool m_showLightMarkers = false;
	bool m_showLightBounds = false;
	float m_lightMarkerScale = 1.0f;
	float m_sidebarWidth = 348.0f;
};
