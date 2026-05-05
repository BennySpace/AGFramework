#pragma once

#include "../DeferredRenderer.h"
#include "../dx12/d3dUtil.h"

class DirectX12Context;

class SponzaScene
{
public:
	struct CameraStart
	{
		DirectX::XMFLOAT3 EyePos = { 0.0f, 8.0f, -30.0f };
		DirectX::XMFLOAT3 LookDirection = { 0.0f, 0.0f, 1.0f };
		float Yaw = 0.0f;
		float Pitch = 0.0f;
	};

	void Initialize(DirectX12Context& context);
	void DisposeUploaders();

	const MeshGeometry& GetGeometry() const { return *m_geometry; }
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return m_srvDescriptorHeap.Get(); }
	const std::vector<DeferredRenderer::ModelDrawItem>& GetDrawItems() const { return m_drawItems; }
	const DirectX::XMFLOAT3& GetSceneCenter() const { return m_sceneCenter; }
	float GetSceneScale() const { return m_sceneScale; }
	const CameraStart& GetInitialCamera() const { return m_initialCamera; }

private:
	void BuildGeometry(DirectX12Context& context);
	void BuildTextures(DirectX12Context& context);
	void BuildDescriptorHeap(DirectX12Context& context);

	std::unique_ptr<MeshGeometry> m_geometry;
	std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
	std::vector<Texture*> m_orderedTextures;
	std::vector<DeferredRenderer::ModelDrawItem> m_drawItems;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescriptorHeap;
	DirectX::XMFLOAT3 m_sceneCenter = { 0.0f, 0.0f, 0.0f };
	float m_sceneScale = 1.0f;
	CameraStart m_initialCamera;
};
