#pragma once

#include "../dx12/DescriptorHeap.h"

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
