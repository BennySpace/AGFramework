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
	static RenderSettings::DemoSettings GetDemoSettings(const RenderSettings &renderSettings);
	static void SetDemoSettings(RenderSettings &renderSettings, const RenderSettings::DemoSettings &demoSettings);
	static bool IsPbrGridEnabled(const RenderSettings &renderSettings);
	static DirectX::XMFLOAT3 GetPbrGridOffset(const DemoShowcaseSession &showcaseSession);
	static void SetPbrGridOffset(DemoShowcaseSession &showcaseSession, const DirectX::XMFLOAT3 &offset);
	static DemoLightEditState GetLightEditState(const DemoLightEditSession &lightEditSession);
	static void SetLightEditState(DemoLightEditSession &lightEditSession, const DemoLightEditState &lightEditState);
	static void ApplyRecommendedLightPreset(DemoLightEditSession &lightEditSession);
	static void ResetRecommendedLightPositions(DemoLightEditSession &lightEditSession);
	static void ResetRecommendedPbrGridOffset(DemoShowcaseSession &showcaseSession);
	static void ApplyRecommendedLook(MaterialSystem &materialSystem, RenderSettings &renderSettings, DemoLightEditSession &lightEditSession);
	static bool MatchesRecommendedLook(const MaterialSystem &materialSystem, const RenderSettings &renderSettings,
	                                  const DemoLightEditSession &lightEditSession);
};
} // namespace Demo
