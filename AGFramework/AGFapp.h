#pragma once

#include "DirectX12App.h"
#include "src/Common/GameTimer.h"
#include "src/Input/InputDevice.h"
#include "Window.h"

#include <memory>

class AGFApp {
public:
	AGFApp();

	void Initialize();
	void Update();
	void Render();
	void Shutdown();
	void CalculateFrameStats();

	void OnWindowResized(int width, int height);

	void Run();
	std::shared_ptr<Window> GetWindow() const { return m_window; }

private:
	std::shared_ptr<Window> m_window;
	std::unique_ptr<InputDevice> m_inputDevice;
	std::unique_ptr<DirectX12App> m_renderer;
	GameTimer m_timer;
	HWND m_hWnd;
	HINSTANCE m_hInstance;
	std::string m_title = "Another Graphic Framework";
	bool m_running;
	bool m_isPaused;
};
