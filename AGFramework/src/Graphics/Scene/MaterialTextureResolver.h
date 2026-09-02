#pragma once

#include "MaterialAssetContract.h"
#include "../ObjModelLoader.h"

#include <string>

class MaterialTextureResolver
{
  public:
	struct ResolvedTextureSlot
	{
		std::string Path;
		bool Exists = false;
	};

	struct ResolvedMaterialTextures
	{
		ResolvedTextureSlot Diffuse;
		ResolvedTextureSlot Normal;
		ResolvedTextureSlot Orm;
		ResolvedTextureSlot Opacity;
		bool HasNormalMap = false;
		bool HasOrmMap = false;
		bool HasOpacityMap = false;
		bool HasAlphaCutout = false;
	};

	static ResolvedMaterialTextures Resolve(const ObjModelLoader::MeshData &mesh, const MaterialAssetContract::Entry &materialContract);
};
