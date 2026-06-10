#include "Window.h"
#include "../Utils/StringUtils.h"
#include "../../resource.h"
#if defined(_DEBUG)
#include "../../external/imgui/backends/imgui_impl_win32.h"
#endif
#include <windowsx.h>

#if defined(_DEBUG)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace
{
constexpr wchar_t kWindowClassName[] = L"ApplicationWindowClass";
}

Window::Window() : m_handle(nullptr), m_instance(nullptr), m_width(0), m_height(0), m_shouldClose(false) {}

Window::~Window()
{
	if (m_handle)
	{
		DestroyWindow(m_handle);
	}
}

bool Window::Create(const std::string &title, int width, int height)
{
	m_title = title;
	m_width = width;
	m_height = height;
	m_instance = GetModuleHandle(nullptr);

	RegisterWindowClass();

	RECT rect = {0, 0, width, height};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

	const std::wstring wideTitle = StringUtils::Utf8ToWide(title);
	m_handle = CreateWindowEx(0, kWindowClassName, wideTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
	                          CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, m_instance, this);

	return m_handle != nullptr;
}

void Window::Show()
{
	ShowWindow(m_handle, SW_SHOW);
}
void Window::Hide()
{
	ShowWindow(m_handle, SW_HIDE);
}

bool Window::ShouldClose() const
{
	return m_shouldClose;
}

void Window::ProcessMessages()
{
	MSG msg;

	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			m_shouldClose = true;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

bool Window::RegisterWindowClass()
{
	HICON largeIcon = static_cast<HICON>(LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
	                                                GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
	HICON smallIcon = static_cast<HICON>(LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
	                                                GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_instance;
	wc.hIcon = largeIcon != nullptr ? largeIcon : LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = kWindowClassName;
	wc.hIconSm = smallIcon != nullptr ? smallIcon : wc.hIcon;

	const ATOM classAtom = RegisterClassEx(&wc);
	if (classAtom != 0)
	{
		return true;
	}

	return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#if defined(_DEBUG)
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
	{
		return true;
	}
#endif

	Window *window = nullptr;

	if (uMsg == WM_NCCREATE)
	{
		CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
		window = reinterpret_cast<Window *>(create->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
	}
	else
	{
		window = reinterpret_cast<Window *>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}

	if (!window)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
		case WM_ACTIVATE:
			window->OnPause.Broadcast(LOWORD(wParam) == WA_INACTIVE);
			return 0;

		case WM_SIZE:
			if (wParam == SIZE_MINIMIZED)
			{
				return 0;
			}

			window->m_width = LOWORD(lParam);
			window->m_height = HIWORD(lParam);
			if (window->OnResize.GetSize() > 0)
			{
				window->OnResize.Broadcast(window->m_width, window->m_height);
			}
			return 0;

		case WM_CLOSE:
			if (window->OnClose.GetSize() > 0)
			{
				bool canClose = true;
				window->OnClose.Broadcast(canClose);

				if (canClose)
					DestroyWindow(hWnd);

				return 0;
			}

			break;

		case WM_DESTROY:
			window->m_shouldClose = true;
			PostQuitMessage(0);
			return 0;

		case WM_NCDESTROY:
			window->m_handle = nullptr;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, 0);
			return DefWindowProc(hWnd, uMsg, wParam, lParam);

		case WM_INPUT:
		{
			UINT size = 0;
			GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
			if (size == 0)
				break;

			std::vector<BYTE> buffer(size);
			if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == size)
			{
				RAWINPUT *raw = reinterpret_cast<RAWINPUT *>(buffer.data());

				if (raw->header.dwType == RIM_TYPEKEYBOARD)
				{
					InputDevice::KeyboardInputEventArgs args;
					args.MakeCode = raw->data.keyboard.MakeCode;
					args.Flags = raw->data.keyboard.Flags;
					args.VKey = raw->data.keyboard.VKey;
					args.Message = raw->data.keyboard.Message;
					window->OnRawKey.Broadcast(args);
				}
				else if (raw->header.dwType == RIM_TYPEMOUSE)
				{
					InputDevice::RawMouseEventArgs args;
					args.Mode = raw->data.mouse.usFlags;
					args.ButtonFlags = raw->data.mouse.usButtonFlags;
					args.ExtraInformation = raw->data.mouse.ulExtraInformation;
					args.Buttons = raw->data.mouse.ulButtons;
					args.WheelDelta = static_cast<short>(raw->data.mouse.usButtonData);
					args.X = raw->data.mouse.lLastX;
					args.Y = raw->data.mouse.lLastY;
					window->OnRawMouse.Broadcast(args);
				}

				PRAWINPUT pRawInput = raw;
				DefRawInputProc(&pRawInput, 1, sizeof(RAWINPUTHEADER));
			}

			return 0;
		}
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
