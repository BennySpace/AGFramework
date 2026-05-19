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
		bool HasAlphaCutout = false;
	};

	std::vector<MeshData> Load(const std::string &filename) const;
};
