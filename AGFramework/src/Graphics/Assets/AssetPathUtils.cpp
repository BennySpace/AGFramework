#include "AssetPathUtils.h"

#include <stdexcept>
#include <windows.h>

namespace
{
constexpr const wchar_t *kAssetPrefixes[] = {L"", L"..\\", L"..\\..\\", L"AGFramework\\"};

std::string BuildMissingAssetError(const char *assetLabel, std::initializer_list<std::wstring> candidateRelativePaths)
{
	std::string errorMessage = "Required runtime asset is missing: ";
	errorMessage += assetLabel;
	errorMessage += ". Expected one of: ";

	bool isFirstPath = true;
	for (const std::wstring &candidateRelativePath : candidateRelativePaths)
	{
		if (!isFirstPath)
		{
			errorMessage += ", ";
		}

		errorMessage += AssetPathUtils::WideToUtf8(candidateRelativePath);
		isFirstPath = false;
	}

	return errorMessage;
}
} // namespace

namespace AssetPathUtils
{
bool FileExists(const std::string &path)
{
	if (path.empty())
	{
		return false;
	}

	return FileExists(AnsiToWide(path));
}

bool FileExists(const std::wstring &path)
{
	if (path.empty())
	{
		return false;
	}

	const DWORD attributes = GetFileAttributesW(path.c_str());
	return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
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

std::wstring JoinPath(const std::wstring &basePath, const std::wstring &relativePath)
{
	if (relativePath.empty())
	{
		return std::wstring();
	}

	if (relativePath.size() > 1 && relativePath[1] == L':')
	{
		return relativePath;
	}

	if (relativePath[0] == L'\\' || relativePath[0] == L'/')
	{
		return relativePath;
	}

	if (basePath.empty())
	{
		return relativePath;
	}

	if (basePath.back() == L'\\' || basePath.back() == L'/')
	{
		return basePath + relativePath;
	}

	return basePath + L"\\" + relativePath;
}

std::wstring Utf8ToWide(const std::string &value)
{
	if (value.empty())
	{
		return std::wstring();
	}

	const int sizeRequired = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
	std::wstring result(sizeRequired > 0 ? sizeRequired - 1 : 0, L'\0');
	if (sizeRequired > 1)
	{
		MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, &result[0], sizeRequired - 1);
	}

	return result;
}

std::wstring AnsiToWide(const std::string &value)
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

std::string WideToUtf8(const std::wstring &value)
{
	if (value.empty())
	{
		return std::string();
	}

	const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
	std::string result(sizeRequired > 0 ? sizeRequired - 1 : 0, '\0');
	if (sizeRequired > 1)
	{
		WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &result[0], sizeRequired - 1, nullptr, nullptr);
	}

	return result;
}

std::wstring ResolveExistingPath(const std::wstring &relativePath)
{
	if (relativePath.empty())
	{
		return std::wstring();
	}

	for (const wchar_t *prefix : kAssetPrefixes)
	{
		const std::wstring candidate = JoinPath(std::wstring(prefix), relativePath);
		if (FileExists(candidate))
		{
			return candidate;
		}
	}

	return std::wstring();
}

std::wstring ResolveExistingPath(std::initializer_list<std::wstring> candidateRelativePaths)
{
	for (const std::wstring &candidateRelativePath : candidateRelativePaths)
	{
		const std::wstring resolvedPath = ResolveExistingPath(candidateRelativePath);
		if (!resolvedPath.empty())
		{
			return resolvedPath;
		}
	}

	return std::wstring();
}

std::wstring ResolveRequiredPath(const std::wstring &relativePath)
{
	const std::wstring resolvedPath = ResolveExistingPath(relativePath);
	if (!resolvedPath.empty())
	{
		return resolvedPath;
	}

	throw std::runtime_error("Required runtime asset not found: " + WideToUtf8(relativePath));
}

std::wstring ResolveRequiredPath(const char *assetLabel, std::initializer_list<std::wstring> candidateRelativePaths)
{
	const std::wstring resolvedPath = ResolveExistingPath(candidateRelativePaths);
	if (!resolvedPath.empty())
	{
		return resolvedPath;
	}

	throw std::runtime_error(BuildMissingAssetError(assetLabel, candidateRelativePaths));
}
} // namespace AssetPathUtils
