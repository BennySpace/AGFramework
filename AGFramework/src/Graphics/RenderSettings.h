#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "../Math/MathHelper.h"

class RenderSettings
{
  public:
	static constexpr std::uint32_t MaxShadowCascadeCount = 4;
	static constexpr std::uint32_t MinShadowMapSize = 512;
	static constexpr std::uint32_t MaxShadowMapSize = 4096;
	static constexpr std::uint32_t ShadowMapSizeAlignment = 256;

	struct LightingSettings
	{
		DirectX::XMFLOAT4 AmbientLight = {1.0f, 1.0f, 1.0f, 0.05f};
		DirectX::XMFLOAT4 BackgroundColor = {0.0f, 0.0f, 0.0f, 1.0f};
		DirectX::XMFLOAT4 AmbientFloor = {0.0f, 0.0f, 0.0f, 0.0f};
	};

	struct ImageBasedLightingSettings
	{
		bool ShowSkybox = true;
		float DiffuseStrength = 0.40f;
		float SpecularStrength = 0.40f;
		float SkyboxIntensity = 1.0f;
		float Exposure = 1.0f;
	};

	struct ShadowSettings
	{
		bool EnableDirectionalShadows = true;
		std::uint32_t CascadeCount = 4;
		std::uint32_t ShadowMapSize = 2048;
		float CascadeSplitLambda = 0.65f;
		float MaxShadowDistance = 180.0f;
		float DepthBias = 64.0f;
		float SlopeScaledDepthBias = 1.0f;
		float DepthBiasClamp = 0.0f;
		float PcfRadius = 1.5f;
		float ShadowStrength = 1.0f;
		float ReceiverBiasMin = 0.00005f;
		float ReceiverBiasSlopeScale = 0.00035f;
		float ReceiverBiasTexelFactor = 0.75f;
	};

	struct DemoSettings
	{
		bool EnableDemoControls = false;
		bool EnablePbrGrid = false;
	};

	struct CascadedShadowData
	{
		std::array<float, MaxShadowCascadeCount> SplitDistances = {12.0f, 36.0f, 90.0f, 180.0f};
		std::array<DirectX::XMFLOAT4X4, MaxShadowCascadeCount> LightViewProjMatrices;
		std::array<std::array<DirectX::XMFLOAT3, 8>, MaxShadowCascadeCount> FrustumCornersWorldSpace = {};
		std::array<DirectX::XMFLOAT4, MaxShadowCascadeCount> CascadeScaleOffsets = {};
		DirectX::XMFLOAT4 ShadowMapMetrics = {2048.0f, 2048.0f, 1.0f / 2048.0f, 1.0f / 2048.0f};

		CascadedShadowData()
		{
			for (DirectX::XMFLOAT4X4 &matrix : LightViewProjMatrices)
			{
				matrix = MathHelper::Identity4x4();
			}
		}
	};

	const LightingSettings &GetLightingSettings() const
	{
		return m_lightingSettings;
	}
	void SetLightingSettings(const LightingSettings &lightingSettings)
	{
		m_lightingSettings = lightingSettings;
	}
	const ImageBasedLightingSettings &GetImageBasedLightingSettings() const
	{
		return m_imageBasedLightingSettings;
	}
	void SetImageBasedLightingSettings(const ImageBasedLightingSettings &imageBasedLightingSettings)
	{
		m_imageBasedLightingSettings = imageBasedLightingSettings;
	}
	const ShadowSettings &GetShadowSettings() const
	{
		return m_shadowSettings;
	}
	void SetShadowSettings(const ShadowSettings &shadowSettings)
	{
		ShadowSettings normalizedSettings = shadowSettings;
		normalizedSettings.CascadeCount =
		    (std::clamp)(normalizedSettings.CascadeCount, 1u, MaxShadowCascadeCount);
		normalizedSettings.ShadowMapSize =
		    (std::clamp)(normalizedSettings.ShadowMapSize, MinShadowMapSize, MaxShadowMapSize);
		normalizedSettings.ShadowMapSize =
		    ((normalizedSettings.ShadowMapSize + ShadowMapSizeAlignment - 1) / ShadowMapSizeAlignment) * ShadowMapSizeAlignment;
		m_shadowSettings = normalizedSettings;
	}
	const DemoSettings &GetDemoSettings() const
	{
		return m_demoSettings;
	}
	void SetDemoSettings(const DemoSettings &demoSettings)
	{
		m_demoSettings = demoSettings;
	}

 private:
	LightingSettings m_lightingSettings;
	ImageBasedLightingSettings m_imageBasedLightingSettings;
	ShadowSettings m_shadowSettings;
	DemoSettings m_demoSettings;
};
