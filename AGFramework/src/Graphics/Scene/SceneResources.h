#pragma once

#include "MeshGeometry.h"
#include "../Resources/TextureResource.h"
#include "../dx12/DescriptorHeap.h"

#include <memory>
#include <unordered_map>
#include <vector>

struct SceneResources
{
	std::unique_ptr<MeshGeometry> Geometry;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::vector<Texture *> OrderedTextures;
	DescriptorHeap SrvDescriptorHeap;

	void DisposeUploaders()
	{
		if (Geometry)
		{
			Geometry->DisposeUploaders();
		}

		for (auto &textureEntry : Textures)
		{
			textureEntry.second->UploadHeap.Reset();
		}
	}
};
