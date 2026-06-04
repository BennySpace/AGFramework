#pragma once

#include "../DeferredRenderer.h"

#include <string>
#include <unordered_map>

class MaterialAssetContract
{
  public:
	struct Entry
	{
		std::string DiffuseTexturePath;
		std::string NormalTexturePath;
		std::string OrmTexturePath;
		std::string OpacityTexturePath;
		DirectX::XMFLOAT4 PbrParams = {0.0f, 0.58f, 1.0f, 0.95f};
		bool HasDiffuseTexturePath = false;
		bool HasNormalTexturePath = false;
		bool HasOrmTexturePath = false;
		bool HasOpacityTexturePath = false;
		bool HasPbrParams = false;
		bool HasAlphaCutoutOverride = false;
		bool AlphaCutout = false;
	};

	bool Load(const std::string &filename);
	Entry ResolveMaterial(const std::string &materialName) const;

  private:
	static Entry Merge(const Entry &baseEntry, const Entry &overrideEntry);
	static std::string NormalizeMaterialName(const std::string &materialName);

	Entry m_defaultEntry;
	std::unordered_map<std::string, Entry> m_entries;
};
