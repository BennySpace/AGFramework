#include "App/AGFramework.h"
#include "Graphics/dx12/d3dUtil.h"
#include "Utils/StringUtils.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd)
{
	try
	{
		AGFramework application;
		application.Initialize();
		application.Run();
	}
	catch (const DxException &e)
	{
		MessageBoxW(nullptr, e.ToString().c_str(), L"DirectX Error", MB_ICONERROR);
		return 1;
	}
	catch (const std::exception &e)
	{
		const std::wstring errorMessage = StringUtils::Utf8ToWide(e.what());
		MessageBoxW(nullptr, errorMessage.c_str(), L"Engine Error", MB_ICONERROR);
		return 1;
	}
	catch (...)
	{
		MessageBoxW(nullptr, L"Unknown fatal error.", L"Engine Error", MB_ICONERROR);
		return 1;
	}

	return 0;
}
