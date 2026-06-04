#include "MaterialAssetContract.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
std::string Trim(const std::string &value)
{
	size_t start = 0;
	while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0)
	{
		++start;
	}

	size_t end = value.size();
	while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
	{
		--end;
	}

	return value.substr(start, end - start);
}

std::string ToLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

bool ParseBool(const std::string &value)
{
	const std::string normalized = ToLower(Trim(value));
	return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

std::string GetBasePath(const std::string &filename)
{
	const size_t lastSlash = filename.find_last_of("\\/");
	return lastSlash == std::string::npos ? std::string() : filename.substr(0, lastSlash);
}

std::string JoinPath(const std::string &basePath, const std::string &relativePath)
{
	if (relativePath.empty())
	{
		return std::string();
	}

	if (relativePath.size() > 1 && relativePath[1] == ':')
	{
		return relativePath;
	}

	if (relativePath[0] == '\\' || relativePath[0] == '/')
	{
		return relativePath;
	}

	if (basePath.empty())
	{
		return relativePath;
	}

	if (basePath.back() == '\\' || basePath.back() == '/')
	{
		return basePath + relativePath;
	}

	return basePath + "\\" + relativePath;
}

void ApplyTextureProperty(const std::string &value, const std::string &basePath, std::string &target, bool &hasTarget)
{
	hasTarget = true;
	target = value.empty() ? std::string() : JoinPath(basePath, value);
}
} // namespace

bool MaterialAssetContract::Load(const std::string &filename)
{
	m_defaultEntry = Entry();
	m_entries.clear();

	std::ifstream input(filename);
	if (!input)
	{
		return false;
	}

	const std::string basePath = GetBasePath(filename);
	Entry *currentEntry = &m_defaultEntry;
	std::string line;
	while (std::getline(input, line))
	{
		const std::string trimmed = Trim(line);
		if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
		{
			continue;
		}

		if (trimmed.front() == '[' && trimmed.back() == ']')
		{
			const std::string sectionName = Trim(trimmed.substr(1, trimmed.size() - 2));
			const std::string normalizedName = NormalizeMaterialName(sectionName);
			if (normalizedName == "default")
			{
				currentEntry = &m_defaultEntry;
			}
			else
			{
				currentEntry = &m_entries[normalizedName];
			}

			continue;
		}

		const size_t equalsPos = trimmed.find('=');
		if (equalsPos == std::string::npos)
		{
			continue;
		}

		const std::string key = ToLower(Trim(trimmed.substr(0, equalsPos)));
		const std::string value = Trim(trimmed.substr(equalsPos + 1));
		if (key == "diffuse")
		{
			ApplyTextureProperty(value, basePath, currentEntry->DiffuseTexturePath, currentEntry->HasDiffuseTexturePath);
		}
		else if (key == "normal")
		{
			ApplyTextureProperty(value, basePath, currentEntry->NormalTexturePath, currentEntry->HasNormalTexturePath);
		}
		else if (key == "orm")
		{
			ApplyTextureProperty(value, basePath, currentEntry->OrmTexturePath, currentEntry->HasOrmTexturePath);
		}
		else if (key == "opacity")
		{
			ApplyTextureProperty(value, basePath, currentEntry->OpacityTexturePath, currentEntry->HasOpacityTexturePath);
		}
		else if (key == "metallic")
		{
			currentEntry->PbrParams.x = std::stof(value);
			currentEntry->HasPbrParams = true;
		}
		else if (key == "roughness")
		{
			currentEntry->PbrParams.y = std::stof(value);
			currentEntry->HasPbrParams = true;
		}
		else if (key == "ao")
		{
			currentEntry->PbrParams.z = std::stof(value);
			currentEntry->HasPbrParams = true;
		}
		else if (key == "ibl")
		{
			currentEntry->PbrParams.w = std::stof(value);
			currentEntry->HasPbrParams = true;
		}
		else if (key == "alpha_cutout")
		{
			currentEntry->HasAlphaCutoutOverride = true;
			currentEntry->AlphaCutout = ParseBool(value);
		}
	}

	return true;
}

MaterialAssetContract::Entry MaterialAssetContract::ResolveMaterial(const std::string &materialName) const
{
	Entry resolved = m_defaultEntry;
	const auto entry = m_entries.find(NormalizeMaterialName(materialName));
	if (entry != m_entries.end())
	{
		resolved = Merge(resolved, entry->second);
	}

	return resolved;
}

MaterialAssetContract::Entry MaterialAssetContract::Merge(const Entry &baseEntry, const Entry &overrideEntry)
{
	Entry merged = baseEntry;

	if (overrideEntry.HasDiffuseTexturePath)
	{
		merged.DiffuseTexturePath = overrideEntry.DiffuseTexturePath;
		merged.HasDiffuseTexturePath = true;
	}
	if (overrideEntry.HasNormalTexturePath)
	{
		merged.NormalTexturePath = overrideEntry.NormalTexturePath;
		merged.HasNormalTexturePath = true;
	}
	if (overrideEntry.HasOrmTexturePath)
	{
		merged.OrmTexturePath = overrideEntry.OrmTexturePath;
		merged.HasOrmTexturePath = true;
	}
	if (overrideEntry.HasOpacityTexturePath)
	{
		merged.OpacityTexturePath = overrideEntry.OpacityTexturePath;
		merged.HasOpacityTexturePath = true;
	}
	if (overrideEntry.HasPbrParams)
	{
		merged.PbrParams = overrideEntry.PbrParams;
		merged.HasPbrParams = true;
	}
	if (overrideEntry.HasAlphaCutoutOverride)
	{
		merged.AlphaCutout = overrideEntry.AlphaCutout;
		merged.HasAlphaCutoutOverride = true;
	}

	return merged;
}

std::string MaterialAssetContract::NormalizeMaterialName(const std::string &materialName)
{
	return ToLower(Trim(materialName));
}
