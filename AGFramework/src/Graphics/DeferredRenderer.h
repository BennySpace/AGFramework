#pragma once

#include "dx12/d3dUtil.h"

#include "CascadedShadowMap.h"
#include "Gbuffer.h"
#include "LightSystem.h"
#include "MaterialSystem.h"
#include "Overlay/DebugOverlay.h"
#include "RenderSettings.h"

class DirectX12Context;

class DeferredRenderer
{
public:
	struct ModelDrawItem
	{
		std::string DrawName;
		std::string DiffuseTexturePath;
		UINT DiffuseSrvHeapIndex = 0;
		bool HasAlphaCutout = false;
		bool CastShadows = true;
	};

	struct FrameData
	{
		DirectX::XMFLOAT3 EyePos = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 LookDirection = { 0.0f, 0.0f, 1.0f };
		DirectX::XMFLOAT3 SceneCenter = { 0.0f, 0.0f, 0.0f };
		float SceneScale = 1.0f;
		DirectX::XMFLOAT4X4 Projection = MathHelper::Identity4x4();
		RenderSettings::LightingSettings LightingSettings;
		RenderSettings::ImageBasedLightingSettings ImageBasedLightingSettings;
		RenderSettings::ShadowSettings ShadowSettings;
		RenderSettings::CascadedShadowData CascadedShadowData;
		MaterialSystem::MaterialState Material;
		LightSystem::LightingState LightState;
	};

	DeferredRenderer() = default;
	~DeferredRenderer();

	void Initialize(DirectX12Context& context, bool enable4xMsaa, UINT msaaQuality);
	void Resize(DirectX12Context& context);
	void UpdateMainPassCB(const FrameData& frameData);
	void RenderShadowMapPass(
		DirectX12Context& context,
		ID3D12DescriptorHeap* srvDescriptorHeap,
		UINT cbvSrvUavDescriptorSize,
		const MeshGeometry& sceneGeometry,
		const std::vector<ModelDrawItem>& drawItems);
	void DrawGeometryPass(
		DirectX12Context& context,
		ID3D12DescriptorHeap* srvDescriptorHeap,
		UINT cbvSrvUavDescriptorSize,
		const MeshGeometry& sceneGeometry,
		const std::vector<ModelDrawItem>& drawItems);
	void DrawLightingPass(DirectX12Context& context, DebugOverlay::DebugViewMode debugViewMode, int shadowDebugCascadeIndex);
	void TransitionCascadedShadowMap(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);
	void TransitionGbuffer(DirectX12Context& context, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

	D3D12_RESOURCE_STATES GetGbufferState() const { return m_gbufferState; }
	void SetGbufferState(D3D12_RESOURCE_STATES state) { m_gbufferState = state; }
	D3D12_RESOURCE_STATES GetCascadedShadowMapState() const { return m_cascadedShadowMapState; }
	void SetCascadedShadowMapState(D3D12_RESOURCE_STATES state) { m_cascadedShadowMapState = state; }

private:
	void BuildShadersAndInputLayout();
	void BuildConstantBuffer(DirectX12Context& context);
	void BuildGbuffer(DirectX12Context& context);
	void BuildCascadedShadowMap(DirectX12Context& context);
	void BuildImageBasedLightingTextures(DirectX12Context& context);
	void BuildLightingSrvHeap(DirectX12Context& context);
	void BuildRootSignature(DirectX12Context& context);
	void BuildPSO(DirectX12Context& context, bool enable4xMsaa, UINT msaaQuality);
	void BuildShadowPSOs(DirectX12Context& context);

	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 View = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float Pad0 = 0.0f;
		RenderSettings::LightingSettings LightingSettings;
		MaterialSystem::MaterialState Material;
		DirectX::XMFLOAT4 ImageBasedLightingSettings = { 0.0f, 0.0f, 0.0f, 0.0f };
		LightSystem::DirectionalLightData DirectionalLights[LightSystem::DirectionalLightCount];
		LightSystem::PointLightData PointLights[LightSystem::PointLightCount];
		LightSystem::SpotLightData SpotLights[LightSystem::SpotLightCount];
		DirectX::XMFLOAT4X4 ShadowLightViewProj[RenderSettings::MaxShadowCascadeCount];
		DirectX::XMFLOAT4 ShadowCascadeSplits = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 ShadowMapMetrics = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 ShadowSettings0 = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 ShadowSettings1 = { 0.0f, 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT4 ShadowSettings2 = { 0.0f, 0.0f, 0.0f, 0.0f };
	};

	struct DrawSettings
	{
		float AlphaCutoff = -1.0f;
		float Padding[3] = { 0.0f, 0.0f, 0.0f };
	};

	struct ShadowPassConstants
	{
		DirectX::XMFLOAT4X4 WorldLightViewProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
	};

	struct LightingDebugSettings
	{
		float ViewMode = static_cast<float>(DebugOverlay::DebugViewMode::Final);
		float PositionVizScale = 0.05f;
		float ShadowDebugCascadeIndex = 0.0f;
		float Padding = 0.0f;
	};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_geometryRootSignature;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_lightingRootSignature;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_shadowRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_geometryPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_lightingPSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_directionalShadowOpaquePSO;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_directionalShadowAlphaCutoutPSO;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_objectCB;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_shadowPassCB;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_lightingSrvHeap;
	std::unique_ptr<CascadedShadowMap> m_cascadedShadowMap;
	std::unique_ptr<Gbuffer> m_gbuffer;
	D3D12_RESOURCE_STATES m_cascadedShadowMapState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
	D3D12_RESOURCE_STATES m_gbufferState = D3D12_RESOURCE_STATE_RENDER_TARGET;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> m_shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	RenderSettings::ShadowSettings m_shadowSettings;
	RenderSettings::ShadowSettings m_directionalShadowPsoSettings;
	RenderSettings::CascadedShadowData m_cascadedShadowData;
	DirectX::XMFLOAT3 m_sceneCenter = { 0.0f, 0.0f, 0.0f };
	float m_sceneScale = 1.0f;
	UINT8* m_mappedShadowPassCB = nullptr;
	UINT8* m_mappedObjectCB = nullptr;
	UINT m_objectCBByteSize = 0;
	UINT m_shadowPassCBStride = 0;
	UINT m_shadowPassCBByteSize = 0;
	bool m_lightingSrvHeapDirty = true;
	std::unique_ptr<Texture> m_irradianceMapTexture;
	std::unique_ptr<Texture> m_prefilterMapTexture;
	std::unique_ptr<Texture> m_brdfLutTexture;
	bool m_imageBasedLightingUsesRgbm = false;
};
