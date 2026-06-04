#include "MaterialTextureResolver.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <vector>
#include <windows.h>

namespace
{
std::wstring AnsiToWStringLocal(const std::string &value)
{
	if (value.empty())
	{
		return std::wstring();
	}

	const int sizeRequired = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
	std::wstring result(sizeRequired > 0 ? sizeRequired - 1 : 0, L'\0');
	if (sizeRequired > 1)
	{
		MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, &result[0], sizeRequired - 1);
	}

	return result;
}

bool FileExists(const std::string &path)
{
	if (path.empty())
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(AnsiToWStringLocal(path).c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

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
	return FileExists(ddsPath) ? ddsPath : path;
}

std::string PreferExistingOptionalTexturePath(const std::string &path)
{
	if (path.empty())
	{
		return std::string();
	}

	if (HasExtension(path, ".dds"))
	{
		return FileExists(path) ? path : std::string();
	}

	const std::string ddsPath = ReplaceExtension(path, ".dds");
	if (!ddsPath.empty() && FileExists(ddsPath))
	{
		return ddsPath;
	}

	return FileExists(path) ? path : std::string();
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

std::string AppendStemSuffix(const std::string &path, const std::string &suffix)
{
	const size_t extensionPos = path.find_last_of('.');
	if (extensionPos == std::string::npos)
	{
		return std::string();
	}

	return path.substr(0, extensionPos) + suffix + path.substr(extensionPos);
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
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_diff", suffix + ".dds"));
		}

		const std::string fromDif = ReplaceStemSuffix(diffusePath, "_dif", suffix);
		if (!fromDif.empty())
		{
			candidates.push_back(fromDif);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_dif", suffix + ".dds"));
		}

		const std::string fromTexture = ReplaceStemSuffix(diffusePath, "_texture", "_texture" + suffix);
		if (!fromTexture.empty())
		{
			candidates.push_back(fromTexture);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_texture", "_texture" + suffix + ".dds"));
		}

		const std::string fromBaseColor = ReplaceStemSuffix(diffusePath, "_BaseColor", suffix);
		if (!fromBaseColor.empty())
		{
			candidates.push_back(fromBaseColor);
			candidates.push_back(ReplaceStemSuffix(diffusePath, "_BaseColor", suffix + ".dds"));
		}

		const std::string fromGeneric = AppendStemSuffix(diffusePath, suffix);
		if (!fromGeneric.empty())
		{
			candidates.push_back(fromGeneric);
			candidates.push_back(AppendStemSuffix(diffusePath, suffix + ".dds"));
		}
	}

	for (const std::string &candidate : candidates)
	{
		if (FileExists(candidate))
		{
			return candidate;
		}
	}

	return std::string();
}
} // namespace

MaterialTextureResolver::ResolvedMaterialTextures MaterialTextureResolver::Resolve(const ObjModelLoader::MeshData &mesh,
                                                                                  const MaterialAssetContract::Entry &materialContract)
{
	ResolvedMaterialTextures resolved;
	const std::string diffuseTexturePath =
	    materialContract.HasDiffuseTexturePath ? materialContract.DiffuseTexturePath : mesh.DiffuseTexturePath;
	const std::string normalTexturePath =
	    materialContract.HasNormalTexturePath ? materialContract.NormalTexturePath : mesh.NormalTexturePath;
	const std::string ormTexturePath = materialContract.HasOrmTexturePath ? materialContract.OrmTexturePath : mesh.OrmTexturePath;
	const std::string opacityTexturePath =
	    materialContract.HasOpacityTexturePath ? materialContract.OpacityTexturePath : mesh.OpacityTexturePath;

	resolved.DiffuseTexturePath = PreferDdsVariant(diffuseTexturePath);
	resolved.NormalTexturePath = normalTexturePath.empty()
	                                 ? FindCompanionTexturePath(resolved.DiffuseTexturePath, {"_norm", "_ddn", "_normal", "_nmap"})
	                                 : PreferExistingOptionalTexturePath(normalTexturePath);
	resolved.OrmTexturePath = PreferExistingOptionalTexturePath(ormTexturePath);
	resolved.OpacityTexturePath = opacityTexturePath.empty()
	                                  ? FindCompanionTexturePath(resolved.DiffuseTexturePath, {"_mask", "_alpha", "_opacity"})
	                                  : PreferExistingOptionalTexturePath(opacityTexturePath);

	resolved.HasNormalMap = !resolved.NormalTexturePath.empty();
	resolved.HasOrmMap = !resolved.OrmTexturePath.empty();
	resolved.HasOpacityMap = !resolved.OpacityTexturePath.empty();
	resolved.HasAlphaCutout = materialContract.HasAlphaCutoutOverride ? materialContract.AlphaCutout
	                                                                 : (mesh.HasAlphaCutout || resolved.HasOpacityMap);
	return resolved;
}
