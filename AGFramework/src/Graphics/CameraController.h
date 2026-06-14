#pragma once

#include "Scene/SceneData.h"
#include "dx12/d3dUtil.h"

class GameTimer;
class InputDevice;

class CameraController
{
  public:
	CameraController(HWND windowHandle, InputDevice *inputDevice);

	void ApplyCameraStart(const SceneData::CameraStart &cameraStart);
	void Update(const GameTimer &gameTimer);
	void NormalizeLookDirection();
	void ReleaseMouseCapture();

	const DirectX::XMFLOAT3 &GetEyePosition() const
	{
		return m_eyePos;
	}

	DirectX::XMFLOAT3 &GetEyePosition()
	{
		return m_eyePos;
	}

	const DirectX::XMFLOAT3 &GetLookDirection() const
	{
		return m_lookDirection;
	}

	DirectX::XMFLOAT3 &GetLookDirection()
	{
		return m_lookDirection;
	}

	float GetYaw() const
	{
		return m_yaw;
	}

	float &GetYaw()
	{
		return m_yaw;
	}

	float GetPitch() const
	{
		return m_pitch;
	}

	float &GetPitch()
	{
		return m_pitch;
	}

	float &GetMoveSpeed()
	{
		return m_moveSpeed;
	}

	float &GetMouseSensitivity()
	{
		return m_mouseSensitivity;
	}

  private:
	void UpdateMouseCaptureState();
	void UpdateMouseLook();
	void UpdateMovement(const GameTimer &gameTimer);

	HWND m_windowHandle = nullptr;
	InputDevice *m_inputDevice = nullptr;
	DirectX::XMFLOAT3 m_eyePos = {0.0f, 8.0f, -30.0f};
	DirectX::XMFLOAT3 m_lookDirection = {0.0f, 0.0f, 1.0f};
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;
	float m_moveSpeed = 10.0f;
	float m_mouseSensitivity = 0.0035f;
	bool m_isMouseCaptured = false;
};
