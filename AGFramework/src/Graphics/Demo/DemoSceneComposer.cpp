#include "DemoSceneComposer.h"

#include "PbrGridBuilder.h"
#include "../Scene/SponzaScene.h"

using namespace DirectX;

namespace Demo
{
void DemoSceneComposer::AppendDemoMeshes(std::vector<ObjModelLoader::MeshData> &meshes, const XMFLOAT3 &sceneMinPoint,
                                         const XMFLOAT3 &sceneMaxPoint, const RenderSettings::DemoSettings &demoSettings)
{
	if (demoSettings.EnablePbrGrid)
	{
		PbrGridBuilder::AppendGridMeshes(meshes, sceneMinPoint, sceneMaxPoint);
	}
}

void DemoSceneComposer::ApplyDemoMaterialDefaults(const ObjModelLoader::MeshData &mesh, ModelDrawItem &drawItem)
{
	drawItem.IsDemoPbrGrid = PbrGridBuilder::IsGridMesh(mesh);

	if (drawItem.IsDemoPbrGrid)
	{
		drawItem.PbrParams = PbrGridBuilder::BuildDefaultPbrParams(mesh);
		return;
	}

	drawItem.PbrParams = XMFLOAT4(0.0f, 0.58f, 1.0f, 0.95f);
}

void DemoSceneComposer::RebuildTrackedPbrGridDrawItems(const std::vector<ModelDrawItem> &drawItems)
{
	m_pbrGridDrawItemIndices.clear();

	for (std::size_t drawItemIndex = 0; drawItemIndex < drawItems.size(); ++drawItemIndex)
	{
		if (drawItems[drawItemIndex].IsDemoPbrGrid)
		{
			m_pbrGridDrawItemIndices.push_back(static_cast<int>(drawItemIndex));
		}
	}
}

void DemoSceneComposer::ApplyPbrGridOffset(SponzaScene &scene, const XMFLOAT3 &positionOffset) const
{
	scene.ApplyPositionOffsetToDrawItems(m_pbrGridDrawItemIndices, positionOffset);
}
} // namespace Demo
