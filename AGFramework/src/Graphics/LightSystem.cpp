#include "LightSystem.h"

using namespace DirectX;

void LightSystem::Update(const XMFLOAT3& eyePosition, const XMFLOAT3& lookDirection)
{
    m_lightingState.DirectionalLights[0].Direction = XMFLOAT4(0.45f, -0.82f, 0.35f, 0.0f);
    m_lightingState.DirectionalLights[0].Color =
        m_lightEnableState.DirectionalLights[0] ?
        XMFLOAT4(0.72f, 0.74f, 0.70f, 1.0f) :
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    const std::array<XMFLOAT3, PointLightCount> pointPositions =
    {
        XMFLOAT3(-6.0f, 4.0f, -6.0f),
        XMFLOAT3(6.0f, 4.0f, -6.0f),
        XMFLOAT3(-6.0f, 4.5f, 0.0f),
        XMFLOAT3(6.0f, 4.5f, 0.0f),
        XMFLOAT3(-5.0f, 4.0f, 6.0f),
        XMFLOAT3(5.0f, 4.0f, 6.0f)
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
        const XMFLOAT3& position = pointPositions[lightIndex];
        m_lightingState.PointLights[lightIndex].Position = XMFLOAT4(
            position.x,
            position.y,
            position.z,
            1.0f);
        m_lightingState.PointLights[lightIndex].Color =
            m_lightEnableState.PointLights[lightIndex] ?
            pointColors[lightIndex] :
            XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_lightingState.PointLights[lightIndex].Params = XMFLOAT4(12.0f, 1.35f, 0.0f, 1.0f);
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
    m_lightingState.SpotLights[0].Color =
        m_lightEnableState.SpotLights[0] ?
        XMFLOAT4(0.38f, 0.48f, 1.0f, 1.0f) :
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    m_lightingState.SpotLights[0].Params = XMFLOAT4(48.0f, 0.96f, 0.88f, 3.25f);

    m_lightingState.SpotLights[1].Position = XMFLOAT4(
        0.0f,
        8.0f,
        -6.0f,
        1.0f);
    m_lightingState.SpotLights[1].Direction = XMFLOAT4(0.0f, -0.45f, 1.0f, 0.0f);
    m_lightingState.SpotLights[1].Color =
        m_lightEnableState.SpotLights[1] ?
        XMFLOAT4(1.0f, 0.92f, 0.70f, 1.0f) :
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    m_lightingState.SpotLights[1].Params = XMFLOAT4(24.0f, 0.95f, 0.82f, 2.2f);
}
