#pragma once

#include "SceneData.h"
#include "SceneResources.h"

class DirectX12Context;

class SponzaScene
{
public:
	void Initialize(DirectX12Context& context);
	void DisposeUploaders();

	const MeshGeometry& GetGeometry() const { return *m_resources.Geometry; }
	ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return m_resources.SrvDescriptorHeap.Get(); }
	const std::vector<DeferredRenderer::ModelDrawItem>& GetDrawItems() const { return m_data.DrawItems; }
	const DirectX::XMFLOAT3& GetSceneCenter() const { return m_data.SceneCenter; }
	float GetSceneScale() const { return m_data.SceneScale; }
	const SceneData::CameraStart& GetInitialCamera() const { return m_data.InitialCamera; }

private:
	void BuildGeometry(DirectX12Context& context);
	void BuildTextures(DirectX12Context& context);
	void BuildDescriptorHeap(DirectX12Context& context);

	SceneData m_data;
	SceneResources m_resources;
};
