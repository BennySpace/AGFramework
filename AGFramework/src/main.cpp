#include "App/AGFramework.h"
#include "Graphics/dx12/d3dUtil.h"
#include "Utils/StringUtils.h"

#include <string_view>

namespace
{
bool HasSmokeTestFlag()
{
	const std::wstring_view commandLine = GetCommandLineW();
	return commandLine.find(L"--smoke-test") != std::wstring_view::npos;
}
} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	try
	{
		AGFramework application;
		if (HasSmokeTestFlag())
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
		MessageBoxW(nullptr, e.ToString().c_str(), L"AGFramework - DirectX Error", MB_ICONERROR);
		return 1;
	}
	catch (const std::exception &e)
	{
		const std::wstring errorMessage = StringUtils::Utf8ToWide(e.what());
		MessageBoxW(nullptr, errorMessage.c_str(), L"AGFramework - Runtime Error", MB_ICONERROR);
		return 1;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"Unknown fatal error.", L"AGFramework - Runtime Error", MB_ICONERROR);
		return 1;
	}

	return 0;
}
