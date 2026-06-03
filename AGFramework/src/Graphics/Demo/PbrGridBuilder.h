#pragma once

#include "../ObjModelLoader.h"

namespace Demo
{
class PbrGridBuilder
{
  public:
	static void AppendGridMeshes(std::vector<ObjModelLoader::MeshData> &meshes, const DirectX::XMFLOAT3 &sponzaMinPoint,
	                             const DirectX::XMFLOAT3 &sponzaMaxPoint);
	static bool IsGridMesh(const ObjModelLoader::MeshData &mesh);
	static DirectX::XMFLOAT4 BuildDefaultPbrParams(const ObjModelLoader::MeshData &mesh);
};
} // namespace Demo
