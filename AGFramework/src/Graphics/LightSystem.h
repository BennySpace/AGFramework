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
		DirectX::XMFLOAT4 Direction = {};
		DirectX::XMFLOAT4 Color = {};
	};

	struct PointLightData
	{
		DirectX::XMFLOAT4 Position = {};
		DirectX::XMFLOAT4 Color = {};
		DirectX::XMFLOAT4 Params = {};
	};

	struct SpotLightData
	{
		DirectX::XMFLOAT4 Position = {};
		DirectX::XMFLOAT4 Direction = {};
		DirectX::XMFLOAT4 Color = {};
		DirectX::XMFLOAT4 Params = {};
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

	struct LightColorState
	{
		std::array<DirectX::XMFLOAT4, DirectionalLightCount> DirectionalLights = {DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f)};
		std::array<DirectX::XMFLOAT4, PointLightCount> PointLights = {
		    DirectX::XMFLOAT4(1.0f, 0.45f, 0.30f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.70f, 0.35f, 1.0f),
		    DirectX::XMFLOAT4(0.35f, 0.85f, 1.0f, 1.0f), DirectX::XMFLOAT4(0.50f, 1.0f, 0.55f, 1.0f),
		    DirectX::XMFLOAT4(0.95f, 0.35f, 0.95f, 1.0f), DirectX::XMFLOAT4(1.0f, 0.95f, 0.55f, 1.0f)};
		std::array<DirectX::XMFLOAT4, SpotLightCount> SpotLights = {DirectX::XMFLOAT4(0.38f, 0.48f, 1.0f, 1.0f),
		                                                            DirectX::XMFLOAT4(1.0f, 0.92f, 0.70f, 1.0f)};
	};

	struct LightIntensityState
	{
		std::array<float, DirectionalLightCount> DirectionalLights = {0.5f};
		std::array<float, PointLightCount> PointLights = {5.0f, 5.0f, 5.0f, 5.0f, 5.0f, 5.0f};
		std::array<float, SpotLightCount> SpotLights = {8.0f, 6.0f};
	};

	struct LightDirectionState
	{
		std::array<DirectX::XMFLOAT3, DirectionalLightCount> DirectionalLights = {DirectX::XMFLOAT3(-0.577f, -0.577f, 0.577f)};
	};

	struct LightPositionState
	{
		std::array<DirectX::XMFLOAT3, PointLightCount> PointLights = {DirectX::XMFLOAT3(-6.0f, 4.0f, -6.0f),
		                                                              DirectX::XMFLOAT3(6.0f, 4.0f, -6.0f),
		                                                              DirectX::XMFLOAT3(-6.0f, 4.5f, 0.0f),
		                                                              DirectX::XMFLOAT3(6.0f, 4.5f, 0.0f),
		                                                              DirectX::XMFLOAT3(-5.0f, 4.0f, 6.0f),
		                                                              DirectX::XMFLOAT3(5.0f, 4.0f, 6.0f)};
		DirectX::XMFLOAT3 SecondarySpotLight = {0.0f, 8.0f, -6.0f};
	};

	struct EditState
	{
		LightEnableState EnableState;
		LightColorState ColorState;
		LightIntensityState IntensityState;
		LightDirectionState DirectionState;
		LightPositionState PositionState;
	};

	void Update(const DirectX::XMFLOAT3 &eyePosition, const DirectX::XMFLOAT3 &lookDirection, const EditState &editState);
	const LightingState &GetLightingState() const
	{
		return m_lightingState;
	}
	const DirectionalLightData &GetShadowCastingDirectionalLight() const
	{
		return m_lightingState.DirectionalLights[ShadowCastingDirectionalLightIndex];
	}

  private:
	LightingState m_lightingState;
};
