#pragma once

#include "MaterialAssetContract.h"
#include "../ObjModelLoader.h"

#include <string>

class MaterialTextureResolver
{
  public:
	struct ResolvedMaterialTextures
	{
		std::string DiffuseTexturePath;
		std::string NormalTexturePath;
		std::string OrmTexturePath;
		std::string OpacityTexturePath;
		bool HasNormalMap = false;
		bool HasOrmMap = false;
		bool HasOpacityMap = false;
		bool HasAlphaCutout = false;
	};

	static ResolvedMaterialTextures Resolve(const ObjModelLoader::MeshData &mesh, const MaterialAssetContract::Entry &materialContract);
};
