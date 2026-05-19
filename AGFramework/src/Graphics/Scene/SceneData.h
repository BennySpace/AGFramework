#pragma once

#include "../DeferredRenderer.h"

struct SceneData
{
	struct CameraStart
	{
		DirectX::XMFLOAT3 EyePos = {0.0f, 8.0f, -30.0f};
		DirectX::XMFLOAT3 LookDirection = {0.0f, 0.0f, 1.0f};
		float Yaw = 0.0f;
		float Pitch = 0.0f;
	};

	std::vector<DeferredRenderer::ModelDrawItem> DrawItems;
	DirectX::XMFLOAT3 SceneCenter = {0.0f, 0.0f, 0.0f};
	float SceneScale = 1.0f;
	CameraStart InitialCamera;
};
