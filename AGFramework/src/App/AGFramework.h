#pragma once

#include "../Graphics/dx12/DirectX12App.h"
#include "../Core/InputDevice.h"
#include "../Core/GameTimer.h"
#include "../Core/Window.h"
#include <memory>

class AGFramework {
public:
    AGFramework(); 

    void Initialize();
    void Run();
    void Shutdown();

    std::shared_ptr<Window> GetWindow() const { return m_window; }
    
private:
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
    HINSTANCE m_hInstance;

    std::string m_title = "Another Graphics Framework";
    bool m_running;
    bool m_isPaused;
};
