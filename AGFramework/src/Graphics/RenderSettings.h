#pragma once

#include <cstdint>

#include "../Math/MathHelper.h"

class RenderSettings
{
public:
    struct LightingSettings
    {
        DirectX::XMFLOAT4 AmbientLight = { 0.15f, 0.15f, 0.2f, 1.0f };
    };

    struct ShadowSettings
    {
        bool EnableDirectionalShadows = true;
        std::uint32_t CascadeCount = 4;
        std::uint32_t ShadowMapSize = 2048;
        float CascadeSplitLambda = 0.65f;
        float MaxShadowDistance = 180.0f;
    };

    const LightingSettings& GetLightingSettings() const { return m_lightingSettings; }
    void SetLightingSettings(const LightingSettings& lightingSettings) { m_lightingSettings = lightingSettings; }
    const ShadowSettings& GetShadowSettings() const { return m_shadowSettings; }
    void SetShadowSettings(const ShadowSettings& shadowSettings) { m_shadowSettings = shadowSettings; }

private:
    LightingSettings m_lightingSettings;
    ShadowSettings m_shadowSettings;
};
