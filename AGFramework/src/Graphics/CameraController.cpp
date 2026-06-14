#include "CameraController.h"

#include "../Core/GameTimer.h"
#include "../Core/InputDevice.h"

using namespace DirectX;

namespace
{
XMVECTOR GetSafeNormalizedDirection(const XMFLOAT3 &direction, const XMVECTOR &fallbackDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))
{
	const XMVECTOR directionVector = XMLoadFloat3(&direction);
	if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return fallbackDirection;
	}

	return XMVector3Normalize(directionVector);
}

bool IsCursorInsideClientArea(HWND windowHandle)
{
	POINT cursorPosition{};
	if (!GetCursorPos(&cursorPosition))
	{
		return false;
	}

	POINT clientPosition = cursorPosition;
	if (!ScreenToClient(windowHandle, &clientPosition))
	{
		return false;
	}

	RECT clientRect{};
	if (!GetClientRect(windowHandle, &clientRect))
	{
		return false;
	}

	return clientPosition.x >= clientRect.left && clientPosition.x < clientRect.right && clientPosition.y >= clientRect.top &&
	       clientPosition.y < clientRect.bottom;
}
} // namespace

CameraController::CameraController(HWND windowHandle, InputDevice *inputDevice) : m_windowHandle(windowHandle), m_inputDevice(inputDevice) {}

void CameraController::ApplyCameraStart(const SceneData::CameraStart &cameraStart)
{
	m_eyePos = cameraStart.EyePos;
	m_lookDirection = cameraStart.LookDirection;
	m_yaw = cameraStart.Yaw;
	m_pitch = cameraStart.Pitch;
}

void CameraController::Update(const GameTimer &gameTimer)
{
	UpdateMouseCaptureState();
	UpdateMouseLook();
	UpdateMovement(gameTimer);
}

void CameraController::NormalizeLookDirection()
{
	const XMVECTOR safeLookDirection = GetSafeNormalizedDirection(m_lookDirection);
	XMStoreFloat3(&m_lookDirection, safeLookDirection);
}

void CameraController::ReleaseMouseCapture()
{
	if (!m_isMouseCaptured)
	{
		return;
	}

	ClipCursor(nullptr);
	ShowCursor(TRUE);
	m_isMouseCaptured = false;
}

void CameraController::UpdateMouseCaptureState()
{
	const bool isWindowFocused = GetForegroundWindow() == m_windowHandle;
	const bool isRightMouseDown = m_inputDevice != nullptr && m_inputDevice->IsKeyDown(Keys::RightButton);
	const bool isCursorInsideClientArea = IsCursorInsideClientArea(m_windowHandle);
	const bool shouldCaptureMouse = isWindowFocused && isRightMouseDown && isCursorInsideClientArea;

	if (shouldCaptureMouse && !m_isMouseCaptured)
	{
		RECT clientRect{};
		GetClientRect(m_windowHandle, &clientRect);

		POINT topLeft{clientRect.left, clientRect.top};
		POINT bottomRight{clientRect.right, clientRect.bottom};
		ClientToScreen(m_windowHandle, &topLeft);
		ClientToScreen(m_windowHandle, &bottomRight);

		RECT clipRect{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
		ClipCursor(&clipRect);
		ShowCursor(FALSE);

		const int centerX = (clipRect.left + clipRect.right) / 2;
		const int centerY = (clipRect.top + clipRect.bottom) / 2;
		SetCursorPos(centerX, centerY);

		m_isMouseCaptured = true;
	}
	else if (!shouldCaptureMouse && m_isMouseCaptured)
	{
		ReleaseMouseCapture();
	}
}

void CameraController::UpdateMouseLook()
{
	if (!m_isMouseCaptured)
	{
		return;
	}

	RECT clientRect{};
	GetClientRect(m_windowHandle, &clientRect);

	POINT centerPoint{(clientRect.left + clientRect.right) / 2, (clientRect.top + clientRect.bottom) / 2};
	ClientToScreen(m_windowHandle, &centerPoint);

	POINT currentMousePosition{};
	if (!GetCursorPos(&currentMousePosition))
	{
		return;
	}

	const float deltaX = static_cast<float>(currentMousePosition.x - centerPoint.x);
	const float deltaY = static_cast<float>(currentMousePosition.y - centerPoint.y);

	m_yaw += deltaX * m_mouseSensitivity;
	m_pitch += deltaY * m_mouseSensitivity;
	m_pitch = (std::max)(-1.45f, (std::min)(1.45f, m_pitch));

	const float cosPitch = cosf(m_pitch);
	m_lookDirection.x = sinf(m_yaw) * cosPitch;
	m_lookDirection.y = -sinf(m_pitch);
	m_lookDirection.z = cosf(m_yaw) * cosPitch;

	SetCursorPos(centerPoint.x, centerPoint.y);
}

void CameraController::UpdateMovement(const GameTimer &gameTimer)
{
	const float sprintMultiplier = 3.0f;
	const bool isShiftPressed =
	    m_inputDevice != nullptr && (m_inputDevice->IsKeyDown(Keys::LeftShift) || m_inputDevice->IsKeyDown(Keys::RightShift));
	const float moveSpeed = m_moveSpeed * (isShiftPressed ? sprintMultiplier : 1.0f) * gameTimer.DeltaTime();

	XMVECTOR eyePosition = XMLoadFloat3(&m_eyePos);
	const XMVECTOR lookDirection = GetSafeNormalizedDirection(m_lookDirection);
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR rightDirection = XMVector3Normalize(XMVector3Cross(worldUp, lookDirection));

	if (m_inputDevice != nullptr && m_inputDevice->IsKeyDown(Keys::W))
	{
		eyePosition += lookDirection * moveSpeed;
	}
	if (m_inputDevice != nullptr && m_inputDevice->IsKeyDown(Keys::S))
	{
		eyePosition -= lookDirection * moveSpeed;
	}
	if (m_inputDevice != nullptr && m_inputDevice->IsKeyDown(Keys::A))
	{
		eyePosition -= rightDirection * moveSpeed;
	}
	if (m_inputDevice != nullptr && m_inputDevice->IsKeyDown(Keys::D))
	{
		eyePosition += rightDirection * moveSpeed;
	}

	XMStoreFloat3(&m_eyePos, eyePosition);
}
