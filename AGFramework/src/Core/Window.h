#pragma once

#include "../Utils/Delegates.h"
#include "InputDevice.h"
#include <string>
#include <Windows.h>

class Window
{
  public:
	Window();
	~Window();

	bool Create(const std::string &title, int width, int height);
	void Show();
	void Hide();
	void ProcessMessages();
	bool ShouldClose() const;

	HWND GetHandle() const
	{
		return m_handle;
	}
	HINSTANCE GetInstanceHandle() const
	{
		return m_instance;
	}

	int GetWidth() const
	{
		return m_width;
	}
	int GetHeight() const
	{
		return m_height;
	}

#pragma region Delegates/Events
	MulticastDelegate<int, int> OnResize;
	MulticastDelegate<bool &> OnClose;
	MulticastDelegate<bool> OnPause;

	MulticastDelegate<InputDevice::KeyboardInputEventArgs> OnRawKey;
	MulticastDelegate<InputDevice::RawMouseEventArgs> OnRawMouse;
#pragma endregion

  private:
	HWND m_handle;
	HINSTANCE m_instance;
	std::string m_title;
	int m_width, m_height;
	bool m_shouldClose;

	bool RegisterWindowClass();
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};
