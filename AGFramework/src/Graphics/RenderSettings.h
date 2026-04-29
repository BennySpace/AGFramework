#pragma once

#include "../Math/MathHelper.h"

class RenderSettings
{
public:
    struct LightingSettings
    {
        DirectX::XMFLOAT4 AmbientLight = { 0.15f, 0.15f, 0.2f, 1.0f };
    };

    const LightingSettings& GetLightingSettings() const { return m_lightingSettings; }
    void SetLightingSettings(const LightingSettings& lightingSettings) { m_lightingSettings = lightingSettings; }

private:
    LightingSettings m_lightingSettings;
};
