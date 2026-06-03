#include "LightSystem.h"

using namespace DirectX;

namespace
{
constexpr XMFLOAT3 kDefaultDirectionalDirection = XMFLOAT3(-0.577f, -0.577f, 0.577f);
constexpr XMFLOAT4 kPointLightParams = XMFLOAT4(12.0f, 1.35f, 0.0f, 1.0f);
constexpr XMFLOAT4 kPrimarySpotLightParams = XMFLOAT4(48.0f, 0.96f, 0.88f, 3.25f);
constexpr XMFLOAT3 kSecondarySpotLightDirection = XMFLOAT3(0.0f, -0.45f, 1.0f);
constexpr XMFLOAT4 kSecondarySpotLightParams = XMFLOAT4(24.0f, 0.95f, 0.82f, 2.2f);

XMFLOAT4 ScaleLightColor(const XMFLOAT4 &color, float intensity)
{
	return XMFLOAT4(color.x * intensity, color.y * intensity, color.z * intensity, color.w);
}

XMFLOAT3 NormalizeOrFallback(const XMFLOAT3 &value, const XMFLOAT3 &fallback)
{
	const XMVECTOR vector = XMLoadFloat3(&value);
	if (XMVector3NearEqual(vector, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return fallback;
	}

	XMFLOAT3 normalized;
	XMStoreFloat3(&normalized, XMVector3Normalize(vector));
	return normalized;
}
} // namespace

void LightSystem::Update(const XMFLOAT3 &eyePosition, const XMFLOAT3 &lookDirection, const EditState &editState)
{
	const XMFLOAT3 directionalDirection = NormalizeOrFallback(editState.DirectionState.DirectionalLights[0], kDefaultDirectionalDirection);
	m_lightingState.DirectionalLights[0].Direction =
	    XMFLOAT4(directionalDirection.x, directionalDirection.y, directionalDirection.z, 0.0f);
	m_lightingState.DirectionalLights[0].Color =
	    editState.EnableState.DirectionalLights[0]
	        ? ScaleLightColor(editState.ColorState.DirectionalLights[0], editState.IntensityState.DirectionalLights[0])
	        : XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	for (std::size_t lightIndex = 0; lightIndex < PointLightCount; ++lightIndex)
	{
		const XMFLOAT3 &position = editState.PositionState.PointLights[lightIndex];
		m_lightingState.PointLights[lightIndex].Position = XMFLOAT4(position.x, position.y, position.z, 1.0f);
		m_lightingState.PointLights[lightIndex].Color =
		    editState.EnableState.PointLights[lightIndex]
		        ? ScaleLightColor(editState.ColorState.PointLights[lightIndex], editState.IntensityState.PointLights[lightIndex])
		        : XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		m_lightingState.PointLights[lightIndex].Params = kPointLightParams;
	}

	const XMFLOAT3 primarySpotDirection = NormalizeOrFallback(lookDirection, XMFLOAT3(0.0f, 0.0f, 1.0f));
	m_lightingState.SpotLights[0].Position = XMFLOAT4(eyePosition.x, eyePosition.y, eyePosition.z, 1.0f);
	m_lightingState.SpotLights[0].Direction =
	    XMFLOAT4(primarySpotDirection.x, primarySpotDirection.y, primarySpotDirection.z, 0.0f);
	m_lightingState.SpotLights[0].Color =
	    editState.EnableState.SpotLights[0]
	        ? ScaleLightColor(editState.ColorState.SpotLights[0], editState.IntensityState.SpotLights[0])
	        : XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_lightingState.SpotLights[0].Params = kPrimarySpotLightParams;

	m_lightingState.SpotLights[1].Position = XMFLOAT4(editState.PositionState.SecondarySpotLight.x, editState.PositionState.SecondarySpotLight.y,
	                                                   editState.PositionState.SecondarySpotLight.z, 1.0f);
	m_lightingState.SpotLights[1].Direction =
	    XMFLOAT4(kSecondarySpotLightDirection.x, kSecondarySpotLightDirection.y, kSecondarySpotLightDirection.z, 0.0f);
	m_lightingState.SpotLights[1].Color =
	    editState.EnableState.SpotLights[1]
	        ? ScaleLightColor(editState.ColorState.SpotLights[1], editState.IntensityState.SpotLights[1])
	        : XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	m_lightingState.SpotLights[1].Params = kSecondarySpotLightParams;
}
