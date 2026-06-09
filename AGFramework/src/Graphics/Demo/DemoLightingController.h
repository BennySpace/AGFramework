#pragma once

#include "DemoLightEditSession.h"
#include "DemoShowcaseSession.h"
#include "../LightSystem.h"
#include "../MaterialSystem.h"
#include "../RenderSettings.h"

namespace Demo
{
using DemoLightEditState = DemoLightEditSession::State;

struct RecommendedLightPreset
{
	LightSystem::LightEnableState EnableState;
	LightSystem::LightColorState ColorState;
	LightSystem::LightIntensityState IntensityState;
	LightSystem::LightDirectionState DirectionState;
	LightSystem::LightPositionState PositionState;
};

class DemoLightingController
{
  public:
	static MaterialSystem::MaterialState BuildRecommendedMaterialState();
	static RenderSettings::LightingSettings BuildRecommendedLightingSettings();
	static RenderSettings::ImageBasedLightingSettings BuildRecommendedImageBasedLightingSettings();
	static RenderSettings::ShadowSettings BuildRecommendedShadowSettings();
	static RecommendedLightPreset BuildRecommendedLightPreset();
	static void ApplyRecommendedLightPreset(DemoLightEditSession &lightEditSession);
	static void ResetRecommendedLightPositions(DemoLightEditSession &lightEditSession);
	static void ResetRecommendedPbrGridOffset(DemoShowcaseSession &showcaseSession);
	static void ApplyRecommendedLook(MaterialSystem &materialSystem, RenderSettings &renderSettings, DemoLightEditSession &lightEditSession);
	static bool MatchesRecommendedLook(const MaterialSystem &materialSystem, const RenderSettings &renderSettings,
	                                  const DemoLightEditSession &lightEditSession);
};
} // namespace Demo
