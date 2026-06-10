#pragma once

#include <string>

namespace StringUtils
{
std::wstring Utf8ToWide(const std::string &value);
std::wstring SystemToWide(const std::string &value);
std::string WideToUtf8(const std::wstring &value);
} // namespace StringUtils
