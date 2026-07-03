#include "App/AGFramework.h"
#include "Graphics/dx12/d3dUtil.h"
#include "Utils/StringUtils.h"

#include <cstdio>
#include <string_view>

namespace
{
bool HasSmokeTestFlag()
{
	const std::wstring_view commandLine = GetCommandLineW();
	return commandLine.find(L"--smoke-test") != std::wstring_view::npos;
}

void ReportSmokeTestError(const std::wstring &message)
{
	OutputDebugStringW(message.c_str());
	OutputDebugStringW(L"\n");
	std::fwprintf(stderr, L"%ls\n", message.c_str());
}
} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	const bool isSmokeTest = HasSmokeTestFlag();

	try
	{
		AGFramework application;
		if (isSmokeTest)
		{
			application.RunSmokeTest();
		}
		else
		{
			application.Initialize();
			application.Run();
		}
	}
	catch (const DxException &e)
	{
		if (isSmokeTest)
		{
			ReportSmokeTestError(e.ToString());
			return 1;
		}
		MessageBoxW(nullptr, e.ToString().c_str(), L"AGFramework - DirectX Error", MB_ICONERROR);
		return 1;
	}
	catch (const std::exception &e)
	{
		const std::wstring errorMessage = StringUtils::Utf8ToWide(e.what());
		if (isSmokeTest)
		{
			ReportSmokeTestError(errorMessage);
			return 1;
		}
		MessageBoxW(nullptr, errorMessage.c_str(), L"AGFramework - Runtime Error", MB_ICONERROR);
		return 1;
	}
	catch (...)
	{
		if (isSmokeTest)
		{
			ReportSmokeTestError(L"Unknown fatal error.");
			return 1;
		}
		MessageBoxW(nullptr, L"Unknown fatal error.", L"AGFramework - Runtime Error", MB_ICONERROR);
		return 1;
	}

	return 0;
}
