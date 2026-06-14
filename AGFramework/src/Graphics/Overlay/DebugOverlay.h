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
	struct CameraState
	{
		DirectX::XMFLOAT3 *EyePosition = nullptr;
		DirectX::XMFLOAT3 *LookDirection = nullptr;
		float *Yaw = nullptr;
		float *Pitch = nullptr;
		float *MoveSpeed = nullptr;
		float *MouseSensitivity = nullptr;
	};

	struct FrameContext
	{
		const GameTimer *Timer = nullptr;
		CameraState Camera;
		MaterialSystem *Material = nullptr;
		RenderSettings *Render = nullptr;
		LightSystem *Light = nullptr;
		Demo::DemoShowcaseSession *Showcase = nullptr;
		Demo::DemoLightEditSession *LightEdit = nullptr;
	};

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
	void Draw(ID3D12GraphicsCommandList *commandList, const FrameContext &frameContext);

	DebugViewMode ResolveDebugViewMode() const
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
