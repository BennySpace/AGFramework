#pragma once

#include "../Math/MathHelper.h"

class MaterialSystem
{
public:
    struct MaterialState
    {
        DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT4 SpecularAlbedo = { 0.85f, 0.85f, 0.85f, 32.0f };
        DirectX::XMFLOAT4 PbrParams = { 0.0f, 0.5f, 1.0f, 1.0f };
    };

    const MaterialState& GetMaterialState() const { return m_materialState; }
    void SetMaterialState(const MaterialState& materialState) { m_materialState = materialState; }

private:
    MaterialState m_materialState;
};
