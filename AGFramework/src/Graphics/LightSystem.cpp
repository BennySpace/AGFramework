#include "LightSystem.h"

#include <algorithm>

using namespace DirectX;

void LightSystem::Update(const XMFLOAT3& eyePosition, const XMFLOAT3& lookDirection, const XMFLOAT3& sceneCenter)
{
    m_lightingState.DirectionalLights[0].Direction = XMFLOAT4(0.45f, -0.82f, 0.35f, 0.0f);
    m_lightingState.DirectionalLights[0].Color = XMFLOAT4(0.72f, 0.74f, 0.70f, 1.0f);

    const std::array<XMFLOAT3, PointLightCount> pointOffsets =
    {
        XMFLOAT3(-12.0f, 5.0f, -12.0f),
        XMFLOAT3(12.0f, 5.0f, -10.0f),
        XMFLOAT3(-10.0f, 6.5f, 0.0f),
        XMFLOAT3(10.0f, 6.0f, 2.0f),
        XMFLOAT3(-8.0f, 5.5f, 12.0f),
        XMFLOAT3(8.0f, 5.0f, 10.0f)
    };
    const std::array<XMFLOAT4, PointLightCount> pointColors =
    {
        XMFLOAT4(1.0f, 0.45f, 0.30f, 1.0f),
        XMFLOAT4(1.0f, 0.70f, 0.35f, 1.0f),
        XMFLOAT4(0.35f, 0.85f, 1.0f, 1.0f),
        XMFLOAT4(0.50f, 1.0f, 0.55f, 1.0f),
        XMFLOAT4(0.95f, 0.35f, 0.95f, 1.0f),
        XMFLOAT4(1.0f, 0.95f, 0.55f, 1.0f)
    };

    for (std::size_t lightIndex = 0; lightIndex < PointLightCount; ++lightIndex)
    {
        const XMFLOAT3& offset = pointOffsets[lightIndex];
        m_lightingState.PointLights[lightIndex].Position = XMFLOAT4(
            sceneCenter.x + offset.x,
            sceneCenter.y + offset.y,
            sceneCenter.z + offset.z,
            1.0f);
        m_lightingState.PointLights[lightIndex].Color = pointColors[lightIndex];
        m_lightingState.PointLights[lightIndex].Params = XMFLOAT4(20.0f + static_cast<float>(lightIndex), 2.0f, 0.0f, 1.0f);
    }

    const XMVECTOR spotDirectionVector = XMVector3Normalize(XMLoadFloat3(&lookDirection));
    XMFLOAT3 spotDirection;
    XMStoreFloat3(&spotDirection, spotDirectionVector);

    m_lightingState.SpotLights[0].Position = XMFLOAT4(eyePosition.x, eyePosition.y, eyePosition.z, 1.0f);
    m_lightingState.SpotLights[0].Direction = XMFLOAT4(
        spotDirection.x,
        spotDirection.y,
        spotDirection.z,
        0.0f);
    m_lightingState.SpotLights[0].Color = XMFLOAT4(0.38f, 0.48f, 1.0f, 1.0f);
    m_lightingState.SpotLights[0].Params = XMFLOAT4(48.0f, 0.96f, 0.88f, 3.25f);

    m_lightingState.SpotLights[1].Position = XMFLOAT4(
        sceneCenter.x,
        sceneCenter.y + (std::max)(12.0f, eyePosition.y + 3.0f),
        sceneCenter.z - 16.0f,
        1.0f);
    m_lightingState.SpotLights[1].Direction = XMFLOAT4(0.0f, -0.75f, 0.65f, 0.0f);
    m_lightingState.SpotLights[1].Color = XMFLOAT4(1.0f, 0.92f, 0.70f, 1.0f);
    m_lightingState.SpotLights[1].Params = XMFLOAT4(52.0f, 0.93f, 0.82f, 2.8f);
}
