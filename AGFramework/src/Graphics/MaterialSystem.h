#pragma once

#include "../Math/MathHelper.h"

class MaterialSystem
{
  public:
	struct MaterialState
	{
		DirectX::XMFLOAT4 DiffuseAlbedo = {1.0f, 1.0f, 1.0f, 1.0f};
		// x = metallic, y = roughness, z = ambient occlusion, w = IBL intensity
		DirectX::XMFLOAT4 PbrParams = {0.0f, 0.5f, 1.0f, 1.0f};
	};

	const MaterialState &GetMaterialState() const
	{
		return m_materialState;
	}
	void SetMaterialState(const MaterialState &materialState)
	{
		m_materialState = materialState;
	}

  private:
	MaterialState m_materialState;
};
