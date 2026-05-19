#pragma once

#include "../dx12/d3dUtil.h"

struct SceneResources
{
	std::unique_ptr<MeshGeometry> Geometry;
	std::unordered_map<std::string, std::unique_ptr<Texture>> Textures;
	std::vector<Texture *> OrderedTextures;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> SrvDescriptorHeap;

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
