#pragma once

#include "../Graphics/RenderingSystem.h"
#include "../Core/InputDevice.h"
#include "../Core/GameTimer.h"
#include "../Core/Window.h"
#include <memory>

class AGFramework
{
  public:
	AGFramework();

	void Initialize();
	void Run();

  private:
	void Shutdown();
	void Update();
	void Draw();
	void CalculateFrameStats();
	void OnWindowResized(int width, int height);

  private:
	std::shared_ptr<Window> m_window;
	std::unique_ptr<InputDevice> m_inputDevice;
	std::unique_ptr<RenderingSystem> m_renderingSystem;

	GameTimer m_timer;
	HWND m_hWnd;
	HINSTANCE m_hInstance;

	std::string m_title = "Another Graphics Framework";
	bool m_isPaused;
};
