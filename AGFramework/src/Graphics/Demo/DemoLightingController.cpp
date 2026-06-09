#include "DemoLightingController.h"

#include <array>
#include <cmath>

using namespace DirectX;

namespace
{
bool NearlyEqual(float a, float b, float epsilon = 0.001f)
{
	return fabsf(a - b) <= epsilon;
}

bool NearlyEqual(const XMFLOAT4 &a, const XMFLOAT4 &b, float epsilon = 0.001f)
{
	return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) && NearlyEqual(a.z, b.z, epsilon) &&
	       NearlyEqual(a.w, b.w, epsilon);
}

bool NearlyEqual(const XMFLOAT3 &a, const XMFLOAT3 &b, float epsilon = 0.001f)
{
	return NearlyEqual(a.x, b.x, epsilon) && NearlyEqual(a.y, b.y, epsilon) && NearlyEqual(a.z, b.z, epsilon);
}

template <typename T, size_t N>
bool NearlyEqualArray(const std::array<T, N> &a, const std::array<T, N> &b)
{
	for (size_t index = 0; index < N; ++index)
	{
		if (!NearlyEqual(a[index], b[index]))
		{
			return false;
		}
	}

	return true;
}
} // namespace

namespace Demo
{
MaterialSystem::MaterialState DemoLightingController::BuildRecommendedMaterialState()
{
	MaterialSystem::MaterialState materialState;
	materialState.DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	materialState.PbrParams = XMFLOAT4(0.0f, 0.52f, 1.0f, 0.95f);
	return materialState;
}

RenderSettings::LightingSettings DemoLightingController::BuildRecommendedLightingSettings()
{
	RenderSettings::LightingSettings lightingSettings;
	lightingSettings.AmbientLight = XMFLOAT4(0.92f, 0.95f, 1.0f, 0.03f);
	lightingSettings.BackgroundColor = XMFLOAT4(0.018f, 0.024f, 0.032f, 1.0f);
	lightingSettings.AmbientFloor = XMFLOAT4(0.03f, 0.028f, 0.024f, 0.12f);
	return lightingSettings;
}

RenderSettings::ImageBasedLightingSettings DemoLightingController::BuildRecommendedImageBasedLightingSettings()
{
	RenderSettings::ImageBasedLightingSettings imageBasedLightingSettings;
	imageBasedLightingSettings.ShowSkybox = true;
	imageBasedLightingSettings.DiffuseStrength = 0.52f;
	imageBasedLightingSettings.SpecularStrength = 0.68f;
	imageBasedLightingSettings.SkyboxIntensity = 0.85f;
	imageBasedLightingSettings.Exposure = 1.05f;
	return imageBasedLightingSettings;
}

RenderSettings::ShadowSettings DemoLightingController::BuildRecommendedShadowSettings()
{
	RenderSettings::ShadowSettings shadowSettings;
	shadowSettings.EnableDirectionalShadows = true;
	shadowSettings.CascadeCount = 4;
	shadowSettings.ShadowMapSize = 2048;
	shadowSettings.CascadeSplitLambda = 0.72f;
	shadowSettings.MaxShadowDistance = 150.0f;
	shadowSettings.DepthBias = 96.0f;
	shadowSettings.SlopeScaledDepthBias = 1.85f;
	shadowSettings.DepthBiasClamp = 0.0f;
	shadowSettings.PcfRadius = 1.75f;
	shadowSettings.ShadowStrength = 0.92f;
	shadowSettings.ReceiverBiasMin = 0.00008f;
	shadowSettings.ReceiverBiasSlopeScale = 0.00045f;
	shadowSettings.ReceiverBiasTexelFactor = 0.85f;
	return shadowSettings;
}

RecommendedLightPreset DemoLightingController::BuildRecommendedLightPreset()
{
	RecommendedLightPreset preset;
	preset.ColorState.DirectionalLights[0] = XMFLOAT4(1.0f, 0.965f, 0.90f, 1.0f);
	preset.IntensityState.DirectionalLights[0] = 1.35f;
	preset.DirectionState.DirectionalLights[0] = XMFLOAT3(-0.36f, -0.82f, 0.44f);
	return preset;
}

void DemoLightingController::ApplyRecommendedLightPreset(DemoLightEditSession &lightEditSession)
{
	const RecommendedLightPreset preset = BuildRecommendedLightPreset();
	DemoLightEditState lightEditState;
	lightEditState.EnableState = preset.EnableState;
	lightEditState.ColorState = preset.ColorState;
	lightEditState.IntensityState = preset.IntensityState;
	lightEditState.DirectionState = preset.DirectionState;
	lightEditState.PositionState = preset.PositionState;
	lightEditSession.SetState(lightEditState);
}

void DemoLightingController::ResetRecommendedLightPositions(DemoLightEditSession &lightEditSession)
{
	DemoLightEditState lightEditState = lightEditSession.GetState();
	const RecommendedLightPreset preset = BuildRecommendedLightPreset();
	lightEditState.DirectionState = preset.DirectionState;
	lightEditState.PositionState = preset.PositionState;
	lightEditSession.SetState(lightEditState);
}

void DemoLightingController::ResetRecommendedPbrGridOffset(DemoShowcaseSession &showcaseSession)
{
	showcaseSession.SetPbrGridOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));
}

void DemoLightingController::ApplyRecommendedLook(MaterialSystem &materialSystem, RenderSettings &renderSettings,
                                                  DemoLightEditSession &lightEditSession)
{
	materialSystem.SetMaterialState(BuildRecommendedMaterialState());
	renderSettings.SetLightingSettings(BuildRecommendedLightingSettings());
	renderSettings.SetImageBasedLightingSettings(BuildRecommendedImageBasedLightingSettings());
	renderSettings.SetShadowSettings(BuildRecommendedShadowSettings());
	ApplyRecommendedLightPreset(lightEditSession);
}

bool DemoLightingController::MatchesRecommendedLook(const MaterialSystem &materialSystem, const RenderSettings &renderSettings,
                                                    const DemoLightEditSession &lightEditSession)
{
	const MaterialSystem::MaterialState &materialState = materialSystem.GetMaterialState();
	const RenderSettings::LightingSettings &lightingSettings = renderSettings.GetLightingSettings();
	const RenderSettings::ImageBasedLightingSettings &imageBasedLightingSettings = renderSettings.GetImageBasedLightingSettings();
	const RenderSettings::ShadowSettings &shadowSettings = renderSettings.GetShadowSettings();
	const DemoLightEditState &lightEditState = lightEditSession.GetState();

	const MaterialSystem::MaterialState recommendedMaterialState = BuildRecommendedMaterialState();
	const RenderSettings::LightingSettings recommendedLightingSettings = BuildRecommendedLightingSettings();
	const RenderSettings::ImageBasedLightingSettings recommendedImageBasedLightingSettings = BuildRecommendedImageBasedLightingSettings();
	const RenderSettings::ShadowSettings recommendedShadowSettings = BuildRecommendedShadowSettings();
	const RecommendedLightPreset recommendedLightPreset = BuildRecommendedLightPreset();

	return NearlyEqual(materialState.DiffuseAlbedo, recommendedMaterialState.DiffuseAlbedo) &&
	       NearlyEqual(materialState.PbrParams, recommendedMaterialState.PbrParams) &&
	       NearlyEqual(lightingSettings.AmbientLight, recommendedLightingSettings.AmbientLight) &&
	       NearlyEqual(lightingSettings.BackgroundColor, recommendedLightingSettings.BackgroundColor) &&
	       NearlyEqual(lightingSettings.AmbientFloor, recommendedLightingSettings.AmbientFloor) &&
	       imageBasedLightingSettings.ShowSkybox == recommendedImageBasedLightingSettings.ShowSkybox &&
	       NearlyEqual(imageBasedLightingSettings.DiffuseStrength, recommendedImageBasedLightingSettings.DiffuseStrength) &&
	       NearlyEqual(imageBasedLightingSettings.SpecularStrength, recommendedImageBasedLightingSettings.SpecularStrength) &&
	       NearlyEqual(imageBasedLightingSettings.SkyboxIntensity, recommendedImageBasedLightingSettings.SkyboxIntensity) &&
	       NearlyEqual(imageBasedLightingSettings.Exposure, recommendedImageBasedLightingSettings.Exposure) &&
	       shadowSettings.EnableDirectionalShadows == recommendedShadowSettings.EnableDirectionalShadows &&
	       shadowSettings.CascadeCount == recommendedShadowSettings.CascadeCount &&
	       shadowSettings.ShadowMapSize == recommendedShadowSettings.ShadowMapSize &&
	       NearlyEqual(shadowSettings.CascadeSplitLambda, recommendedShadowSettings.CascadeSplitLambda) &&
	       NearlyEqual(shadowSettings.MaxShadowDistance, recommendedShadowSettings.MaxShadowDistance) &&
	       NearlyEqual(shadowSettings.DepthBias, recommendedShadowSettings.DepthBias) &&
	       NearlyEqual(shadowSettings.SlopeScaledDepthBias, recommendedShadowSettings.SlopeScaledDepthBias) &&
	       NearlyEqual(shadowSettings.DepthBiasClamp, recommendedShadowSettings.DepthBiasClamp) &&
	       NearlyEqual(shadowSettings.PcfRadius, recommendedShadowSettings.PcfRadius) &&
	       NearlyEqual(shadowSettings.ShadowStrength, recommendedShadowSettings.ShadowStrength) &&
	       NearlyEqual(shadowSettings.ReceiverBiasMin, recommendedShadowSettings.ReceiverBiasMin) &&
	       NearlyEqual(shadowSettings.ReceiverBiasSlopeScale, recommendedShadowSettings.ReceiverBiasSlopeScale) &&
	       NearlyEqual(shadowSettings.ReceiverBiasTexelFactor, recommendedShadowSettings.ReceiverBiasTexelFactor) &&
	       lightEditState.EnableState.DirectionalLights == recommendedLightPreset.EnableState.DirectionalLights &&
	       lightEditState.EnableState.PointLights == recommendedLightPreset.EnableState.PointLights &&
	       lightEditState.EnableState.SpotLights == recommendedLightPreset.EnableState.SpotLights &&
	       NearlyEqualArray(lightEditState.ColorState.DirectionalLights, recommendedLightPreset.ColorState.DirectionalLights) &&
	       NearlyEqualArray(lightEditState.ColorState.PointLights, recommendedLightPreset.ColorState.PointLights) &&
	       NearlyEqualArray(lightEditState.ColorState.SpotLights, recommendedLightPreset.ColorState.SpotLights) &&
	       NearlyEqualArray(lightEditState.IntensityState.DirectionalLights, recommendedLightPreset.IntensityState.DirectionalLights) &&
	       NearlyEqualArray(lightEditState.IntensityState.PointLights, recommendedLightPreset.IntensityState.PointLights) &&
	       NearlyEqualArray(lightEditState.IntensityState.SpotLights, recommendedLightPreset.IntensityState.SpotLights) &&
	       NearlyEqualArray(lightEditState.DirectionState.DirectionalLights, recommendedLightPreset.DirectionState.DirectionalLights) &&
	       NearlyEqualArray(lightEditState.PositionState.PointLights, recommendedLightPreset.PositionState.PointLights) &&
	       NearlyEqual(lightEditState.PositionState.SecondarySpotLight, recommendedLightPreset.PositionState.SecondarySpotLight);
}
} // namespace Demo
