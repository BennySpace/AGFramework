#include "DemoSceneRuntime.h"

class DirectX12Context;

namespace Demo
{
void DemoSceneRuntime::Initialize(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings)
{
	m_scene.Initialize(context, demoSettings);
	m_sceneComposer.RebuildTrackedPbrGridDrawItems(m_scene.GetDrawItems());
	m_activeDemoSettings = demoSettings;
}

bool DemoSceneRuntime::RequiresReload(const RenderSettings::DemoSettings &demoSettings) const
{
	return m_activeDemoSettings.EnablePbrGrid != demoSettings.EnablePbrGrid;
}

void DemoSceneRuntime::Reload(DirectX12Context &context, const RenderSettings::DemoSettings &demoSettings)
{
	Initialize(context, demoSettings);
}

void DemoSceneRuntime::ApplyFrameState(const RenderSettings::DemoSettings &demoSettings)
{
	if (demoSettings.EnablePbrGrid)
	{
		m_sceneComposer.ApplyPbrGridOffset(m_scene, m_showcaseSession.GetPbrGridOffset());
	}
}

void DemoSceneRuntime::DisposeUploaders()
{
	m_scene.DisposeUploaders();
}
} // namespace Demo
