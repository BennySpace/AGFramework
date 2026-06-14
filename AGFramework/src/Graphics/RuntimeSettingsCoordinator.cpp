#include "RuntimeSettingsCoordinator.h"

#include "dx12/DirectX12Context.h"

namespace
{
bool NearlyEqualFloat(float a, float b, float epsilon = 0.0001f)
{
	return fabsf(a - b) <= epsilon;
}
} // namespace

RenderSettings::ShadowSettings
RuntimeSettingsCoordinator::InitializeShadowSettings(DeferredRenderer &deferredRenderer, const RenderSettings &renderSettings)
{
	const RenderSettings::ShadowSettings activeShadowSettings = renderSettings.GetShadowSettings();
	deferredRenderer.SetShadowSettings(activeShadowSettings);
	return activeShadowSettings;
}

void RuntimeSettingsCoordinator::ApplyFrameState(Demo::DemoSceneRuntime &demoSceneRuntime, const RenderSettings &renderSettings)
{
	demoSceneRuntime.ApplyFrameState(renderSettings.GetDemoSettings());
}

void RuntimeSettingsCoordinator::ReloadSceneIfNeeded(DirectX12Context &context, Demo::DemoSceneRuntime &demoSceneRuntime,
                                                     const RenderSettings &renderSettings,
                                                     const ReloadCallbacks &callbacks)
{
	const RenderSettings::DemoSettings requestedDemoSettings = renderSettings.GetDemoSettings();
	if (!demoSceneRuntime.RequiresReload(requestedDemoSettings))
	{
		return;
	}

	callbacks.BeginReload();
	demoSceneRuntime.Reload(context, requestedDemoSettings);
	callbacks.EndReload();
	demoSceneRuntime.DisposeUploaders();
}

void RuntimeSettingsCoordinator::ReloadShadowSettingsIfNeeded(DirectX12Context &context, DeferredRenderer &deferredRenderer,
                                                              const RenderSettings &renderSettings,
                                                              RenderSettings::ShadowSettings &activeShadowSettings,
                                                              const ReloadCallbacks &callbacks)
{
	const RenderSettings::ShadowSettings requestedShadowSettings = renderSettings.GetShadowSettings();
	if (!RequiresShadowResourceReload(activeShadowSettings, requestedShadowSettings))
	{
		return;
	}

	callbacks.BeginReload();
	deferredRenderer.SetShadowSettings(requestedShadowSettings);
	deferredRenderer.ReloadShadowDependentResources(context);
	callbacks.EndReload();

	activeShadowSettings = requestedShadowSettings;
}

bool RuntimeSettingsCoordinator::RequiresShadowResourceReload(const RenderSettings::ShadowSettings &currentSettings,
                                                             const RenderSettings::ShadowSettings &requestedSettings)
{
	return currentSettings.CascadeCount != requestedSettings.CascadeCount ||
	       currentSettings.ShadowMapSize != requestedSettings.ShadowMapSize ||
	       !NearlyEqualFloat(currentSettings.DepthBias, requestedSettings.DepthBias) ||
	       !NearlyEqualFloat(currentSettings.SlopeScaledDepthBias, requestedSettings.SlopeScaledDepthBias) ||
	       !NearlyEqualFloat(currentSettings.DepthBiasClamp, requestedSettings.DepthBiasClamp);
}
