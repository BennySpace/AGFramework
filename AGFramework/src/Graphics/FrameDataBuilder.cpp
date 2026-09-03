#include "FrameDataBuilder.h"

#include <cfloat>

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
} // namespace

XMFLOAT3 FrameDataBuilder::NormalizeLookDirection(const XMFLOAT3 &lookDirection)
{
	XMFLOAT3 normalizedLookDirection;
	XMStoreFloat3(&normalizedLookDirection, GetSafeNormalizedDirection(lookDirection));
	return normalizedLookDirection;
}

DeferredRenderer::FrameData FrameDataBuilder::BuildFrameData(const Inputs &inputs)
{
	DeferredRenderer::FrameData frameData;
	frameData.EyePos = inputs.EyePos;
	frameData.LookDirection = inputs.LookDirection;
	frameData.SceneCenter = inputs.SceneCenter;
	frameData.SceneScale = inputs.SceneScale;
	frameData.CameraNearPlane = inputs.CameraNearPlane;
	frameData.Projection = inputs.Projection;
	frameData.LightingSettings = inputs.LightingSettings;
	frameData.ImageBasedLightingSettings = inputs.ImageBasedLightingSettings;
	frameData.ShadowSettings = inputs.ShadowSettings;
	frameData.CascadedShadowData = BuildCascadedShadowData(inputs);
	frameData.Material = inputs.Material;
	frameData.LightState = inputs.LightState;
	return frameData;
}

RenderSettings::CascadedShadowData FrameDataBuilder::BuildCascadedShadowData(const Inputs &inputs)
{
	RenderSettings::CascadedShadowData shadowData;

	const float minimumFarPlane = inputs.CameraNearPlane + 0.001f;
	const float cameraFarPlane = (std::max)(minimumFarPlane, inputs.CameraFarPlane);
	const float farPlane = (std::clamp)(inputs.ShadowSettings.MaxShadowDistance, minimumFarPlane, cameraFarPlane);
	const float cameraDepthRange = cameraFarPlane - inputs.CameraNearPlane;
	const std::uint32_t cascadeCount = (std::min)(inputs.ShadowSettings.CascadeCount, RenderSettings::MaxShadowCascadeCount);
	const float cascadeCountF = static_cast<float>((std::max)(1u, cascadeCount));
	const float lambda = (std::max)(0.0f, (std::min)(1.0f, inputs.ShadowSettings.CascadeSplitLambda));

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < RenderSettings::MaxShadowCascadeCount; ++cascadeIndex)
	{
		if (cascadeIndex >= cascadeCount)
		{
			shadowData.SplitDistances[cascadeIndex] = farPlane;
			continue;
		}

		const float splitFactor = static_cast<float>(cascadeIndex + 1) / cascadeCountF;
		const float logarithmicSplit = inputs.CameraNearPlane * powf(farPlane / inputs.CameraNearPlane, splitFactor);
		const float uniformSplit = inputs.CameraNearPlane + (farPlane - inputs.CameraNearPlane) * splitFactor;
		shadowData.SplitDistances[cascadeIndex] = lambda * logarithmicSplit + (1.0f - lambda) * uniformSplit;
	}

	const float shadowMapSize = static_cast<float>((std::max)(1u, inputs.ShadowSettings.ShadowMapSize));
	shadowData.ShadowMapMetrics = XMFLOAT4(shadowMapSize, shadowMapSize, 1.0f / shadowMapSize, 1.0f / shadowMapSize);

	const XMVECTOR eye = XMLoadFloat3(&inputs.EyePos);
	const XMVECTOR safeLookDirection = GetSafeNormalizedDirection(inputs.LookDirection);
	const XMVECTOR target = eye + safeLookDirection;
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	const XMMATRIX proj = XMLoadFloat4x4(&inputs.Projection);
	const XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

	const XMVECTOR fullFrustumCornersNdc[8] = {XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f), XMVectorSet(-1.0f, 1.0f, 0.0f, 1.0f),
	                                           XMVectorSet(1.0f, 1.0f, 0.0f, 1.0f),   XMVectorSet(1.0f, -1.0f, 0.0f, 1.0f),
	                                           XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f), XMVectorSet(-1.0f, 1.0f, 1.0f, 1.0f),
	                                           XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f),   XMVectorSet(1.0f, -1.0f, 1.0f, 1.0f)};

	XMVECTOR fullFrustumCornersWorld[8];
	for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
	{
		XMVECTOR cornerWorld = XMVector4Transform(fullFrustumCornersNdc[cornerIndex], invViewProj);
		cornerWorld = XMVectorScale(cornerWorld, 1.0f / XMVectorGetW(cornerWorld));
		fullFrustumCornersWorld[cornerIndex] = cornerWorld;
	}

	float cascadeNearDistance = inputs.CameraNearPlane;
	for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		const float cascadeFarDistance = shadowData.SplitDistances[cascadeIndex];
		const float nearT = (cascadeNearDistance - inputs.CameraNearPlane) / cameraDepthRange;
		const float farT = (cascadeFarDistance - inputs.CameraNearPlane) / cameraDepthRange;

		for (int cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
		{
			const XMVECTOR nearCorner = fullFrustumCornersWorld[cornerIndex];
			const XMVECTOR farCorner = fullFrustumCornersWorld[cornerIndex + 4];
			const XMVECTOR cornerRay = farCorner - nearCorner;

			XMFLOAT3 cascadeNearCorner;
			XMFLOAT3 cascadeFarCorner;
			XMStoreFloat3(&cascadeNearCorner, nearCorner + cornerRay * nearT);
			XMStoreFloat3(&cascadeFarCorner, nearCorner + cornerRay * farT);

			shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex] = cascadeNearCorner;
			shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex + 4] = cascadeFarCorner;
		}

		cascadeNearDistance = cascadeFarDistance;
	}

	XMVECTOR lightDirection = XMLoadFloat4(&inputs.LightState.DirectionalLights[LightSystem::ShadowCastingDirectionalLightIndex].Direction);
	lightDirection = XMVectorSetW(lightDirection, 0.0f);
	if (XMVector3NearEqual(lightDirection, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		lightDirection = XMVectorSet(0.45f, -0.82f, 0.35f, 0.0f);
	}
	lightDirection = XMVector3Normalize(lightDirection);

	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR lightUp = worldUp;
	if (fabsf(XMVectorGetX(XMVector3Dot(lightDirection, worldUp))) > 0.99f)
	{
		lightUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}

	for (std::uint32_t cascadeIndex = 0; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		XMVECTOR cascadeCenter = XMVectorZero();
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			cascadeCenter += XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
		}
		cascadeCenter = XMVectorScale(cascadeCenter, 1.0f / 8.0f);

		float cascadeRadius = 0.0f;
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			const XMVECTOR corner = XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
			const float cornerDistance = XMVectorGetX(XMVector3Length(corner - cascadeCenter));
			cascadeRadius = (std::max)(cascadeRadius, cornerDistance);
		}
		cascadeRadius = ceilf(cascadeRadius * 16.0f) / 16.0f;

		const XMVECTOR lightPosition = cascadeCenter - lightDirection * (cascadeRadius * 2.0f + 50.0f);
		const XMMATRIX lightView = XMMatrixLookAtLH(lightPosition, cascadeCenter, lightUp);

		const XMVECTOR cascadeCenterLightSpace = XMVector3TransformCoord(cascadeCenter, lightView);
		XMFLOAT3 cascadeCenterLight;
		XMStoreFloat3(&cascadeCenterLight, cascadeCenterLightSpace);

		XMFLOAT3 minBounds(FLT_MAX, FLT_MAX, FLT_MAX);
		XMFLOAT3 maxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);
		for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
		{
			const XMVECTOR corner = XMLoadFloat3(&shadowData.FrustumCornersWorldSpace[cascadeIndex][cornerIndex]);
			const XMVECTOR cornerLightSpace = XMVector3TransformCoord(corner, lightView);
			XMFLOAT3 cornerLight;
			XMStoreFloat3(&cornerLight, cornerLightSpace);

			minBounds.x = (std::min)(minBounds.x, cornerLight.x);
			minBounds.y = (std::min)(minBounds.y, cornerLight.y);
			minBounds.z = (std::min)(minBounds.z, cornerLight.z);
			maxBounds.x = (std::max)(maxBounds.x, cornerLight.x);
			maxBounds.y = (std::max)(maxBounds.y, cornerLight.y);
			maxBounds.z = (std::max)(maxBounds.z, cornerLight.z);
		}

		const float depthPadding = cascadeRadius * 3.0f + 100.0f;
		float left = cascadeCenterLight.x - cascadeRadius;
		float right = cascadeCenterLight.x + cascadeRadius;
		float bottom = cascadeCenterLight.y - cascadeRadius;
		float top = cascadeCenterLight.y + cascadeRadius;
		const float nearZ = (std::max)(0.1f, cascadeCenterLight.z - depthPadding);
		const float farZ = (std::max)(nearZ + 0.1f, cascadeCenterLight.z + depthPadding);

		const float shadowMapResolution = static_cast<float>((std::max)(1u, inputs.ShadowSettings.ShadowMapSize));
		const float projectionWidth = right - left;
		const float projectionHeight = top - bottom;
		const float texelSizeX = projectionWidth / shadowMapResolution;
		const float texelSizeY = projectionHeight / shadowMapResolution;

		if (texelSizeX > 0.0f && texelSizeY > 0.0f)
		{
			const float centerX = 0.5f * (left + right);
			const float centerY = 0.5f * (bottom + top);
			const float snappedCenterX = floorf(centerX / texelSizeX + 0.5f) * texelSizeX;
			const float snappedCenterY = floorf(centerY / texelSizeY + 0.5f) * texelSizeY;
			const float offsetX = snappedCenterX - centerX;
			const float offsetY = snappedCenterY - centerY;

			left += offsetX;
			right += offsetX;
			bottom += offsetY;
			top += offsetY;
		}

		const XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(left, right, bottom, top, nearZ, farZ);
		const XMMATRIX lightViewProj = lightView * lightProj;
		XMStoreFloat4x4(&shadowData.LightViewProjMatrices[cascadeIndex], lightViewProj);
	}

	return shadowData;
}
