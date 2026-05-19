#pragma once

#include <memory>
#include <Windows.h>

#include "dx12/DirectX12App.h"

class GameTimer;

class RenderingSystem
{
  public:
	RenderingSystem() = default;
	~RenderingSystem();

	bool Initialize(HINSTANCE hInstance, HWND hWnd);
	void Update(const GameTimer &gameTimer);
	void Render(const GameTimer &gameTimer);
	void OnWindowResize(int width, int height);
	void Shutdown();

  private:
	std::unique_ptr<DirectX12App> m_renderer;
};
