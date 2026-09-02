#pragma once

#include "../Core/InputDevice.h"
#include "../Core/GameTimer.h"
#include "../Core/Window.h"
#include "../Graphics/dx12/DirectX12App.h"
#include <memory>

class AGFramework
{
  public:
	AGFramework();

	void Initialize(bool showWindow = true);
	void Run();
	void RunSmokeTest();

  private:
	void Shutdown();
	void Update();
	void Draw();
	void CalculateFrameStats();
	void OnWindowResized(int width, int height);

  private:
	std::shared_ptr<Window> m_window;
	std::unique_ptr<InputDevice> m_inputDevice;
	std::unique_ptr<DirectX12App> m_renderer;

	GameTimer m_timer;
	HWND m_hWnd;

	std::string m_title = "AGFramework";
	bool m_isPaused;
};
