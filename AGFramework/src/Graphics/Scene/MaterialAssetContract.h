#pragma once

#include "../DeferredRenderer.h"

#include <string>
#include <unordered_map>

class MaterialAssetContract
{
  public:
	struct TextureSlot
	{
		enum class State
		{
			Unset,
			ExplicitPath
		};

		State StateValue = State::Unset;
		std::string Path;

		bool IsExplicit() const
		{
			return StateValue == State::ExplicitPath;
		}
	};

	struct Entry
	{
		TextureSlot Diffuse;
		TextureSlot Normal;
		TextureSlot Orm;
		TextureSlot Opacity;
		DirectX::XMFLOAT4 PbrParams = {0.0f, 0.58f, 1.0f, 0.95f};
		bool HasPbrParams = false;
		bool HasMetallic = false;
		bool HasRoughness = false;
		bool HasAmbientOcclusion = false;
		bool HasIblIntensity = false;
		bool HasAlphaCutoutOverride = false;
		bool AlphaCutout = false;
	};

	void Load(const std::string &filename);
	Entry ResolveMaterial(const std::string &materialName) const;

  private:
	static Entry Merge(const Entry &baseEntry, const Entry &overrideEntry);
	static std::string NormalizeMaterialName(const std::string &materialName);

	Entry m_defaultEntry;
	std::unordered_map<std::string, Entry> m_entries;
};
