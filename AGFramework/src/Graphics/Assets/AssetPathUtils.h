#pragma once

#include <initializer_list>
#include <string>

namespace AssetPathUtils
{
bool FileExists(const std::string &path);
bool FileExists(const std::wstring &path);

std::string JoinPath(const std::string &basePath, const std::string &relativePath);
std::wstring JoinPath(const std::wstring &basePath, const std::wstring &relativePath);

std::wstring AnsiToWide(const std::string &value);
std::string WideToUtf8(const std::wstring &value);

std::wstring ResolveExistingPath(const std::wstring &relativePath);
std::wstring ResolveExistingPath(std::initializer_list<std::wstring> candidateRelativePaths);
std::wstring ResolveRequiredPath(const std::wstring &relativePath);
std::wstring ResolveRequiredPath(const char *assetLabel, std::initializer_list<std::wstring> candidateRelativePaths);
} // namespace AssetPathUtils
