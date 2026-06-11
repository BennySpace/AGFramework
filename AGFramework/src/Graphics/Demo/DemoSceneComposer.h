#pragma once

#include "../RenderSettings.h"
#include "../ObjModelLoader.h"
#include "../Scene/ModelDrawItem.h"

class SponzaScene;

namespace Demo
{
class DemoSceneComposer
{
 public:
	static void AppendDemoMeshes(std::vector<ObjModelLoader::MeshData> &meshes, const DirectX::XMFLOAT3 &sceneMinPoint,
	                             const DirectX::XMFLOAT3 &sceneMaxPoint, const RenderSettings::DemoSettings &demoSettings);
	static void ApplyDemoMaterialDefaults(const ObjModelLoader::MeshData &mesh, ModelDrawItem &drawItem);
	void RebuildTrackedPbrGridDrawItems(const std::vector<ModelDrawItem> &drawItems);
	void ApplyPbrGridOffset(SponzaScene &scene, const DirectX::XMFLOAT3 &positionOffset) const;

  private:
	std::vector<int> m_pbrGridDrawItemIndices;
};
} // namespace Demo
