#pragma once

#include "DeferredRenderer.h"
#include "Demo/DemoSceneRuntime.h"
#include "RenderSettings.h"

#include <functional>

class DirectX12Context;

class RuntimeSettingsCoordinator
{
  public:
	struct ReloadCallbacks
	{
		std::function<void()> BeginReload;
		std::function<void()> EndReload;
	};

	static RenderSettings::ShadowSettings InitializeShadowSettings(DeferredRenderer &deferredRenderer,
	                                                              const RenderSettings &renderSettings);
	static void ApplyFrameState(Demo::DemoSceneRuntime &demoSceneRuntime, const RenderSettings &renderSettings);
	static void ReloadSceneIfNeeded(DirectX12Context &context, Demo::DemoSceneRuntime &demoSceneRuntime,
	                                const RenderSettings &renderSettings, const ReloadCallbacks &callbacks);
	static void ReloadShadowSettingsIfNeeded(DirectX12Context &context, DeferredRenderer &deferredRenderer,
	                                         const RenderSettings &renderSettings,
	                                         RenderSettings::ShadowSettings &activeShadowSettings,
	                                         const ReloadCallbacks &callbacks);

  private:
	static bool RequiresShadowResourceReload(const RenderSettings::ShadowSettings &currentSettings,
	                                         const RenderSettings::ShadowSettings &requestedSettings);
};
