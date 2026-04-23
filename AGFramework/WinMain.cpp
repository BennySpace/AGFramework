#include "AGFApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
	try {
		AGFApp app;
		app.Initialize();
		app.Run();
	}
	catch (const std::exception& e) {
		MessageBoxA(nullptr, e.what(), "Engine Error", MB_ICONERROR);
		return 1;
	}

	return 0;
}
