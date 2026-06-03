#include "PbrGridBuilder.h"

#include "../GeometryGenerator.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

using namespace DirectX;

namespace
{
constexpr int kGridRows = 6;
constexpr int kGridColumns = 6;
constexpr char kGridMaterialPrefix[] = "PbrGrid_r";

ObjModelLoader::MeshData BuildGridMeshData(GeometryGenerator::MeshData meshData, const std::string &materialName,
                                           const XMFLOAT3 &translation)
{
	ObjModelLoader::MeshData mesh;
	mesh.Vertices = std::move(meshData.Vertices);
	mesh.Indices32 = std::move(meshData.Indices32);
	mesh.MaterialName = materialName;
	mesh.DiffuseTexturePath.clear();
	mesh.NormalTexturePath.clear();
	mesh.OrmTexturePath.clear();
	mesh.HasAlphaCutout = false;

	for (GeometryGenerator::Vertex &vertex : mesh.Vertices)
	{
		vertex.Position.x += translation.x;
		vertex.Position.y += translation.y;
		vertex.Position.z += translation.z;
	}

	return mesh;
}

bool ParseGridMaterialName(const std::string &materialName, int &rowIndex, int &columnIndex)
{
	if (materialName.compare(0, sizeof(kGridMaterialPrefix) - 1, kGridMaterialPrefix) != 0)
	{
		return false;
	}

	const std::size_t columnMarker = materialName.find("_c", sizeof(kGridMaterialPrefix) - 1);
	if (columnMarker == std::string::npos)
	{
		return false;
	}

	try
	{
		rowIndex = std::stoi(materialName.substr(sizeof(kGridMaterialPrefix) - 1, columnMarker - (sizeof(kGridMaterialPrefix) - 1)));
		columnIndex = std::stoi(materialName.substr(columnMarker + 2));
	}
	catch (const std::exception &)
	{
		return false;
	}

	return rowIndex >= 0 && rowIndex < kGridRows && columnIndex >= 0 && columnIndex < kGridColumns;
}
} // namespace

namespace Demo
{
void PbrGridBuilder::AppendGridMeshes(std::vector<ObjModelLoader::MeshData> &meshes, const XMFLOAT3 &sponzaMinPoint,
                                      const XMFLOAT3 &sponzaMaxPoint)
{
	const float sponzaExtentX = sponzaMaxPoint.x - sponzaMinPoint.x;
	const float sponzaExtentY = sponzaMaxPoint.y - sponzaMinPoint.y;
	const float sponzaExtentZ = sponzaMaxPoint.z - sponzaMinPoint.z;
	const float sphereRadius = (std::max)(5.0f, (std::min)(sponzaExtentX, sponzaExtentY) * 0.0225f);
	const float spacing = sphereRadius * 2.45f;
	const float centerX = 0.5f * (sponzaMinPoint.x + sponzaMaxPoint.x);
	const float startX = centerX - 0.5f * static_cast<float>(kGridColumns - 1) * spacing;
	const float startY = sponzaMinPoint.y + sphereRadius;
	// Place the grid slightly in front of Sponza so it is visible from the startup camera.
	const float gridZ = sponzaMinPoint.z - sponzaExtentZ * 0.10f;

	GeometryGenerator geometryGenerator;
	for (int rowIndex = 0; rowIndex < kGridRows; ++rowIndex)
	{
		for (int columnIndex = 0; columnIndex < kGridColumns; ++columnIndex)
		{
			GeometryGenerator::MeshData sphere = geometryGenerator.CreateSphere(sphereRadius, 32, 32);
			const XMFLOAT3 translation(startX + static_cast<float>(columnIndex) * spacing,
			                           startY + static_cast<float>(rowIndex) * spacing, gridZ);
			const std::string materialName = "PbrGrid_r" + std::to_string(rowIndex) + "_c" + std::to_string(columnIndex);
			meshes.push_back(BuildGridMeshData(std::move(sphere), materialName, translation));
		}
	}
}

bool PbrGridBuilder::IsGridMesh(const ObjModelLoader::MeshData &mesh)
{
	int rowIndex = 0;
	int columnIndex = 0;
	return ParseGridMaterialName(mesh.MaterialName, rowIndex, columnIndex);
}

XMFLOAT4 PbrGridBuilder::BuildDefaultPbrParams(const ObjModelLoader::MeshData &mesh)
{
	int rowIndex = 0;
	int columnIndex = 0;
	if (!ParseGridMaterialName(mesh.MaterialName, rowIndex, columnIndex))
	{
		return XMFLOAT4(0.0f, 0.58f, 1.0f, 0.95f);
	}

	const float metallic =
	    kGridRows > 1 ? static_cast<float>(rowIndex) / static_cast<float>(kGridRows - 1) : 0.0f;
	const float roughness =
	    kGridColumns > 1 ? 0.05f + (0.95f * static_cast<float>(columnIndex) / static_cast<float>(kGridColumns - 1)) : 0.05f;
	return XMFLOAT4(metallic, roughness, 1.0f, 1.0f);
}
} // namespace Demo
