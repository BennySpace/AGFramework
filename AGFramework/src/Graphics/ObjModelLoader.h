#pragma once

#include "GeometryGenerator.h"
#include <string>
#include <vector>

class ObjModelLoader
{
  public:
	struct MeshData
	{
		std::vector<GeometryGenerator::Vertex> Vertices;
		std::vector<std::uint32_t> Indices32;
		std::string MaterialName;
		std::string DiffuseTexturePath;
		std::string NormalTexturePath;
		std::string OrmTexturePath;
		std::string OpacityTexturePath;
		DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
		bool HasAlphaCutout = false;
	};

	std::vector<MeshData> Load(const std::string &filename) const;
};
