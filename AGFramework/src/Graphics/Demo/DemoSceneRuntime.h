#pragma once

#include "DemoLightEditSession.h"
#include "DemoSceneComposer.h"
#include "DemoShowcaseSession.h"
#include "../RenderSettings.h"
#include "../Scene/SponzaScene.h"

class DirectX12Context;

namespace Demo
{
class DemoSceneRuntime
{
  public:
	void Initialize(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings);
	bool RequiresReload(const RenderSettings::DemoSettings &demoSettings) const;
	void Reload(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings);
	void ApplyFrameState(const RenderSettings::DemoSettings &demoSettings);
	void DisposeUploaders();

	const SponzaScene &GetScene() const
	{
		return m_scene;
	}

	SponzaScene &GetScene()
	{
		return m_scene;
	}

	const DemoShowcaseSession &GetShowcaseSession() const
	{
		return m_showcaseSession;
	}

	DemoShowcaseSession &GetShowcaseSession()
	{
		return m_showcaseSession;
	}

	const DemoLightEditSession &GetLightEditSession() const
	{
		return m_lightEditSession;
	}

	DemoLightEditSession &GetLightEditSession()
	{
		return m_lightEditSession;
	}

  private:
	SponzaScene m_scene;
	DemoSceneComposer m_sceneComposer;
	DemoShowcaseSession m_showcaseSession;
	DemoLightEditSession m_lightEditSession;
	RenderSettings::DemoSettings m_activeDemoSettings;
};
} // namespace Demo
