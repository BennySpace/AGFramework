#pragma once

#include <array>

#include "../Math/MathHelper.h"

class LightSystem
{
  public:
	static constexpr std::size_t DirectionalLightCount = 1;
	static constexpr std::size_t PointLightCount = 6;
	static constexpr std::size_t SpotLightCount = 2;
	static constexpr std::size_t ShadowCastingDirectionalLightIndex = 0;

	struct DirectionalLightData
	{
		DirectX::XMFLOAT4 Direction = {0.577f, -0.577f, 0.577f, 0.0f};
		DirectX::XMFLOAT4 Color = {2.1f, 2.0f, 1.9f, 1.0f};
	};

	struct PointLightData
	{
		DirectX::XMFLOAT4 Position = {8.0f, 6.0f, -4.0f, 1.0f};
		DirectX::XMFLOAT4 Color = {1.0f, 0.55f, 0.30f, 1.0f};
		DirectX::XMFLOAT4 Params = {24.0f, 2.2f, 0.0f, 1.0f};
	};

	struct SpotLightData
	{
		DirectX::XMFLOAT4 Position = {0.0f, 10.0f, -18.0f, 1.0f};
		DirectX::XMFLOAT4 Direction = {0.0f, -0.35f, 1.0f, 0.0f};
		DirectX::XMFLOAT4 Color = {0.35f, 0.45f, 1.0f, 1.0f};
		DirectX::XMFLOAT4 Params = {42.0f, 0.94f, 0.82f, 3.0f};
	};

	struct LightingState
	{
		std::array<DirectionalLightData, DirectionalLightCount> DirectionalLights;
		std::array<PointLightData, PointLightCount> PointLights;
		std::array<SpotLightData, SpotLightCount> SpotLights;
	};

	struct LightEnableState
	{
		std::array<bool, DirectionalLightCount> DirectionalLights = {true};
		std::array<bool, PointLightCount> PointLights = {false, false, false, false, false, false};
		std::array<bool, SpotLightCount> SpotLights = {false, false};
	};

	void Update(const DirectX::XMFLOAT3 &eyePosition, const DirectX::XMFLOAT3 &lookDirection);
	const LightingState &GetLightingState() const
	{
		return m_lightingState;
	}
	const DirectionalLightData &GetShadowCastingDirectionalLight() const
	{
		return m_lightingState.DirectionalLights[ShadowCastingDirectionalLightIndex];
	}
	const LightEnableState &GetLightEnableState() const
	{
		return m_lightEnableState;
	}
	void SetLightEnableState(const LightEnableState &lightEnableState)
	{
		m_lightEnableState = lightEnableState;
	}

  private:
	LightingState m_lightingState;
	LightEnableState m_lightEnableState;
};
