#include "MaterialTextureResolver.h"
#include "../Assets/AssetPathUtils.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <vector>

namespace
{
using TextureSlot = MaterialAssetContract::TextureSlot;
using ResolvedTextureSlot = MaterialTextureResolver::ResolvedTextureSlot;

std::string ReplaceExtension(const std::string &path, const std::string &newExtension)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	return path.substr(0, extensionPos) + newExtension;
}

std::string ToLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

bool HasExtension(const std::string &path, const std::string &extension)
{
	const std::string lowerPath = ToLower(path);
	const std::string lowerExtension = ToLower(extension);
	return lowerPath.size() >= lowerExtension.size() &&
	       lowerPath.compare(lowerPath.size() - lowerExtension.size(), lowerExtension.size(), lowerExtension) == 0;
}

std::string PreferDdsVariant(const std::string &path)
{
	if (path.empty())
	{
		return path;
	}

	if (HasExtension(path, ".dds"))
	{
		return path;
	}

	const std::string ddsPath = ReplaceExtension(path, ".dds");
	return AssetPathUtils::FileExists(ddsPath) ? ddsPath : path;
}

std::string PreferExistingOptionalTexturePath(const std::string &path)
{
	if (path.empty())
	{
		return std::string();
	}

	if (HasExtension(path, ".dds"))
	{
		return AssetPathUtils::FileExists(path) ? path : std::string();
	}

	const std::string ddsPath = ReplaceExtension(path, ".dds");
	if (!ddsPath.empty() && AssetPathUtils::FileExists(ddsPath))
	{
		return ddsPath;
	}

	return AssetPathUtils::FileExists(path) ? path : std::string();
}

std::string ReplaceStemSuffix(const std::string &path, const std::string &stemSuffix, const std::string &replacement)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	const std::string stem = path.substr(0, extensionPos);
	const std::string extension = path.substr(extensionPos);
	const std::string lowerStem = ToLower(stem);
	const std::string lowerSuffix = ToLower(stemSuffix);
	if (lowerStem.size() < lowerSuffix.size() ||
	    lowerStem.compare(lowerStem.size() - lowerSuffix.size(), lowerSuffix.size(), lowerSuffix) != 0)
	{
		return std::string();
	}

	return stem.substr(0, stem.size() - stemSuffix.size()) + replacement + extension;
}

std::string ReplaceStemSuffixAndExtension(const std::string &path, const std::string &stemSuffix, const std::string &replacement,
                                          const std::string &newExtension)
{
	const std::string replacedPath = ReplaceStemSuffix(path, stemSuffix, replacement);
	return replacedPath.empty() ? std::string() : ReplaceExtension(replacedPath, newExtension);
}

std::string AppendStemSuffix(const std::string &path, const std::string &suffix)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	return path.substr(0, extensionPos) + suffix + path.substr(extensionPos);
}

std::string AppendStemSuffixAndExtension(const std::string &path, const std::string &suffix, const std::string &newExtension)
{
	const std::string appendedPath = AppendStemSuffix(path, suffix);
	return appendedPath.empty() ? std::string() : ReplaceExtension(appendedPath, newExtension);
}

std::string FindCompanionTexturePath(const std::string &diffusePath, std::initializer_list<std::string> suffixes)
{
	if (diffusePath.empty())
	{
		return std::string();
	}

	std::vector<std::string> candidates;
	for (const std::string &suffix : suffixes)
	{
		const std::string fromDiff = ReplaceStemSuffix(diffusePath, "_diff", suffix);
		if (!fromDiff.empty())
		{
			candidates.push_back(fromDiff);
			candidates.push_back(ReplaceStemSuffixAndExtension(diffusePath, "_diff", suffix, ".dds"));
		}

		const std::string fromDif = ReplaceStemSuffix(diffusePath, "_dif", suffix);
		if (!fromDif.empty())
		{
			candidates.push_back(fromDif);
			candidates.push_back(ReplaceStemSuffixAndExtension(diffusePath, "_dif", suffix, ".dds"));
		}

		const std::string fromTexture = ReplaceStemSuffix(diffusePath, "_texture", "_texture" + suffix);
		if (!fromTexture.empty())
		{
			candidates.push_back(fromTexture);
			candidates.push_back(ReplaceStemSuffixAndExtension(diffusePath, "_texture", "_texture" + suffix, ".dds"));
		}

		const std::string fromBaseColor = ReplaceStemSuffix(diffusePath, "_BaseColor", suffix);
		if (!fromBaseColor.empty())
		{
			candidates.push_back(fromBaseColor);
			candidates.push_back(ReplaceStemSuffixAndExtension(diffusePath, "_BaseColor", suffix, ".dds"));
		}

		const std::string fromGeneric = AppendStemSuffix(diffusePath, suffix);
		if (!fromGeneric.empty())
		{
			candidates.push_back(fromGeneric);
			candidates.push_back(AppendStemSuffixAndExtension(diffusePath, suffix, ".dds"));
		}
	}

	for (const std::string &candidate : candidates)
	{
		if (AssetPathUtils::FileExists(candidate))
		{
			return candidate;
		}
	}

	return std::string();
}

ResolvedTextureSlot ResolveTextureSlot(const TextureSlot &contractSlot, const std::string &meshPath, bool allowLegacyFallback,
                                       const std::string &legacyFallbackPath, bool preferDds)
{
	ResolvedTextureSlot resolved;

	if (contractSlot.IsExplicit())
	{
		resolved.Path = preferDds ? PreferDdsVariant(contractSlot.Path) : PreferExistingOptionalTexturePath(contractSlot.Path);
		resolved.Exists = !resolved.Path.empty();
		return resolved;
	}

	if (!meshPath.empty())
	{
		resolved.Path = preferDds ? PreferDdsVariant(meshPath) : PreferExistingOptionalTexturePath(meshPath);
		resolved.Exists = !resolved.Path.empty();
		if (resolved.Exists || !allowLegacyFallback)
		{
			return resolved;
		}
	}

	if (allowLegacyFallback)
	{
		resolved.Path = legacyFallbackPath;
		resolved.Exists = !resolved.Path.empty();
	}

	return resolved;
}
} // namespace

MaterialTextureResolver::ResolvedMaterialTextures MaterialTextureResolver::Resolve(const ObjModelLoader::MeshData &mesh,
                                                                                  const MaterialAssetContract::Entry &materialContract)
{
	ResolvedMaterialTextures resolved;

	resolved.Diffuse = ResolveTextureSlot(materialContract.Diffuse, mesh.DiffuseTexturePath, false, std::string(), true);

	const std::string diffuseReferencePath = resolved.Diffuse.Path;
	resolved.Normal = ResolveTextureSlot(materialContract.Normal, mesh.NormalTexturePath, true,
	                                     FindCompanionTexturePath(diffuseReferencePath, {"_norm", "_normal"}), false);
	resolved.Orm = ResolveTextureSlot(materialContract.Orm, mesh.OrmTexturePath, false, std::string(), false);
	resolved.Opacity = ResolveTextureSlot(materialContract.Opacity, mesh.OpacityTexturePath, true,
	                                      FindCompanionTexturePath(diffuseReferencePath, {"_alpha", "_opacity"}), false);

	resolved.HasNormalMap = resolved.Normal.Exists;
	resolved.HasOrmMap = resolved.Orm.Exists;
	resolved.HasOpacityMap = resolved.Opacity.Exists;
	resolved.HasAlphaCutout = materialContract.HasAlphaCutoutOverride ? materialContract.AlphaCutout
	                                                                 : (mesh.HasAlphaCutout || resolved.HasOpacityMap);
	return resolved;
}
