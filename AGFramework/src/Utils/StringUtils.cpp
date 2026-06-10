#include "StringUtils.h"

#include <windows.h>

namespace
{
std::wstring MultiByteToWide(const std::string &value, UINT codePage)
{
	if (value.empty())
	{
		return std::wstring();
	}

	const int sizeRequired = MultiByteToWideChar(codePage, 0, value.c_str(), -1, nullptr, 0);
	std::wstring result(sizeRequired > 0 ? sizeRequired - 1 : 0, L'\0');
	if (sizeRequired > 1)
	{
		MultiByteToWideChar(codePage, 0, value.c_str(), -1, &result[0], sizeRequired - 1);
	}

	return result;
}
} // namespace

namespace StringUtils
{
std::wstring Utf8ToWide(const std::string &value)
{
	return MultiByteToWide(value, CP_UTF8);
}

std::wstring AnsiToWide(const std::string &value)
{
	return MultiByteToWide(value, CP_ACP);
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
} // namespace StringUtils
