#pragma once

#include "DeferredRenderer.h"

class FrameDataBuilder
{
  public:
	struct Inputs
	{
		DirectX::XMFLOAT3 EyePos = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 LookDirection = {0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT3 SceneCenter = {0.0f, 0.0f, 0.0f};
		float SceneScale = 1.0f;
		float CameraNearPlane = 1.0f;
		float CameraFarPlane = 1000.0f;
		DirectX::XMFLOAT4X4 Projection = MathHelper::Identity4x4();
		RenderSettings::LightingSettings LightingSettings;
		RenderSettings::ImageBasedLightingSettings ImageBasedLightingSettings;
		RenderSettings::ShadowSettings ShadowSettings;
		LightSystem::LightingState LightState;
		MaterialSystem::MaterialState Material;
		float TotalTime = 0.0f;
	};

	static DirectX::XMFLOAT3 NormalizeLookDirection(const DirectX::XMFLOAT3 &lookDirection);
	static DeferredRenderer::FrameData BuildFrameData(const Inputs &inputs);

  private:
	static RenderSettings::CascadedShadowData BuildCascadedShadowData(const Inputs &inputs);
};
