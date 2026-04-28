#pragma once

#include "../Utils/Delegates.h"
#include "Keys.h"
#include "SimpleMath.h"
#include <unordered_set>

class InputDevice
{
	friend class Window;
	friend class AGFramework;
	std::unordered_set<Keys> m_keys;
	
public:
	struct MouseMoveEventArgs
	{
		DirectX::SimpleMath::Vector2 Position;
		DirectX::SimpleMath::Vector2 Offset;
		int WheelDelta;
	};

	DirectX::SimpleMath::Vector2 MousePosition{};
	DirectX::SimpleMath::Vector2 MouseOffset{};
	int MouseWheelDelta = 0;

	MulticastDelegate<const MouseMoveEventArgs&> MouseMove;

public:
	InputDevice(HWND hWnd);
	~InputDevice() = default;


	void AddPressedKey(Keys key);
	void RemovePressedKey(Keys key);
	bool IsKeyDown(Keys key);

protected:
	struct KeyboardInputEventArgs {
		/*
			* The "make" scan code (key depression).
			*/
		USHORT MakeCode;

		/*
			* The flags field indicates a "break" (key release) and other
			* miscellaneous scan code information defined in ntddkbd.h.
			*/
		USHORT Flags;

		USHORT VKey;
		UINT   Message;
	};

	enum class MouseButtonFlags
	{
		LeftButtonDown = 1,
		LeftButtonUp = 2,
		RightButtonDown = 4,
		RightButtonUp = 8,
		MiddleButtonDown = 16,
		MiddleButtonUp = 32,
		Button1Down = LeftButtonDown,
		Button1Up = LeftButtonUp,
		Button2Down = RightButtonDown,
		Button2Up = RightButtonUp,
		Button3Down = MiddleButtonDown,
		Button3Up = MiddleButtonUp,
		Button4Down = 64,
		Button4Up = 128,
		Button5Down = 256,
		Button5Up = 512,
		MouseWheel = 1024,
		Hwheel = 2048,

		None = 0,
	};
	struct RawMouseEventArgs
	{
		int Mode;
		int ButtonFlags;
		int ExtraInformation;
		int Buttons;
		int WheelDelta;
		int X;
		int Y;
	};

	void HandleKeyboardInput(KeyboardInputEventArgs args);
	void HandleMouseInput(RawMouseEventArgs args);

	HWND m_hWnd;
};
