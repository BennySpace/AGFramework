#include "App/AGFramework.h"
#include "Graphics/dx12/d3dUtil.h"

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
		MessageBoxA(nullptr, e.what(), "Engine Error", MB_ICONERROR);
		return 1;
	}
	catch (...)
	{
		MessageBoxA(nullptr, "Unknown fatal error.", "Engine Error", MB_ICONERROR);
		return 1;
	}

	return 0;
}
