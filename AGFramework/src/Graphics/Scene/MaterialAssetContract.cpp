#include "MaterialAssetContract.h"
#include "../../Utils/StringUtils.h"
#include "../Assets/AssetPathUtils.h"

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

std::string FormatConfigError(const std::string &filename, std::size_t lineNumber, const std::string &message)
{
	std::ostringstream errorStream;
	errorStream << "Failed to load material contract '" << filename << "'";
	if (lineNumber > 0)
	{
		errorStream << " at line " << lineNumber;
	}
	errorStream << ": " << message;
	return errorStream.str();
}

std::string GetBasePath(const std::string &filename)
{
	const size_t lastSlash = filename.find_last_of("\\/");
	return lastSlash == std::string::npos ? std::string() : filename.substr(0, lastSlash);
}

void ApplyTextureProperty(const std::string &value, const std::string &basePath, MaterialAssetContract::TextureSlot &target)
{
	target.StateValue = MaterialAssetContract::TextureSlot::State::ExplicitPath;
	target.Path = value.empty() ? std::string() : AssetPathUtils::JoinPath(basePath, value);

	if (!target.Path.empty() && !AssetPathUtils::FileExists(target.Path))
	{
		throw std::runtime_error("explicit texture path not found: " + target.Path);
	}
}
} // namespace

void MaterialAssetContract::Load(const std::string &filename)
{
	m_defaultEntry = Entry();
	m_entries.clear();

	std::ifstream input(StringUtils::Utf8ToWide(filename));
	if (!input)
	{
		throw std::runtime_error(FormatConfigError(filename, 0, "file not found or cannot be opened"));
	}

	const std::string basePath = GetBasePath(filename);
	Entry *currentEntry = &m_defaultEntry;
	std::string line;
	std::size_t lineNumber = 0;
	while (std::getline(input, line))
	{
		++lineNumber;
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
			throw std::runtime_error(FormatConfigError(filename, lineNumber, "expected 'key=value' entry"));
		}

		const std::string key = ToLower(Trim(trimmed.substr(0, equalsPos)));
		const std::string value = Trim(trimmed.substr(equalsPos + 1));
		try
		{
			if (key == "diffuse")
			{
				ApplyTextureProperty(value, basePath, currentEntry->Diffuse);
			}
			else if (key == "basecolor")
			{
				ApplyTextureProperty(value, basePath, currentEntry->Diffuse);
			}
			else if (key == "normal")
			{
				ApplyTextureProperty(value, basePath, currentEntry->Normal);
			}
			else if (key == "orm")
			{
				ApplyTextureProperty(value, basePath, currentEntry->Orm);
			}
			else if (key == "opacity")
			{
				ApplyTextureProperty(value, basePath, currentEntry->Opacity);
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
			else
			{
				throw std::runtime_error("unknown property '" + key + "'");
			}
		}
		catch (const std::exception &error)
		{
			throw std::runtime_error(FormatConfigError(filename, lineNumber, error.what()));
		}
	}
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

	if (overrideEntry.Diffuse.IsExplicit())
	{
		merged.Diffuse = overrideEntry.Diffuse;
	}
	if (overrideEntry.Normal.IsExplicit())
	{
		merged.Normal = overrideEntry.Normal;
	}
	if (overrideEntry.Orm.IsExplicit())
	{
		merged.Orm = overrideEntry.Orm;
	}
	if (overrideEntry.Opacity.IsExplicit())
	{
		merged.Opacity = overrideEntry.Opacity;
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
