#include "DebugOverlay.h"

#include "../../Core/GameTimer.h"
#include "../Demo/DemoLightEditSession.h"
#include "../Demo/DemoLightingController.h"
#include "../Demo/DemoShowcaseSession.h"
#include "../LightSystem.h"
#include "../MaterialSystem.h"
#include "../RenderSettings.h"
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_dx12.h"
#include "../../../external/imgui/backends/imgui_impl_win32.h"
#include <algorithm>
#include <cmath>
#include <string>

using namespace DirectX;

namespace
{
bool ProjectWorldToScreen(const XMFLOAT3 &worldPosition, const XMMATRIX &viewProj, const ImVec2 &displaySize, ImVec2 &screenPosition)
{
	const XMVECTOR position = XMVectorSet(worldPosition.x, worldPosition.y, worldPosition.z, 1.0f);
	XMVECTOR clip = XMVector4Transform(position, viewProj);
	const float w = XMVectorGetW(clip);
	if (w <= 0.001f)
	{
		return false;
	}

	clip = XMVectorScale(clip, 1.0f / w);
	const float ndcX = XMVectorGetX(clip);
	const float ndcY = XMVectorGetY(clip);
	const float ndcZ = XMVectorGetZ(clip);
	if (ndcX < -1.2f || ndcX > 1.2f || ndcY < -1.2f || ndcY > 1.2f || ndcZ < 0.0f || ndcZ > 1.0f)
	{
		return false;
	}

	screenPosition.x = (ndcX * 0.5f + 0.5f) * displaySize.x;
	screenPosition.y = (-ndcY * 0.5f + 0.5f) * displaySize.y;
	return true;
}

XMMATRIX BuildViewProjection(const XMFLOAT3 &eyePosition, const XMFLOAT3 &lookDirection, const ImVec2 &displaySize)
{
	const XMVECTOR eye = XMLoadFloat3(&eyePosition);
	XMVECTOR look = XMLoadFloat3(&lookDirection);
	if (XMVector3NearEqual(look, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		look = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	}
	else
	{
		look = XMVector3Normalize(look);
	}

	const XMVECTOR target = eye + look;
	const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMMATRIX view = XMMatrixLookAtLH(eye, target, up);
	const float aspectRatio = displaySize.y > 0.0f ? displaySize.x / displaySize.y : 1.0f;
	const XMMATRIX projection = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspectRatio, 1.0f, 1000.0f);
	return view * projection;
}

XMFLOAT3 ExtractPosition(const XMFLOAT4 &value)
{
	return XMFLOAT3(value.x, value.y, value.z);
}

XMFLOAT3 ExtractDirection(const XMFLOAT4 &value)
{
	return XMFLOAT3(value.x, value.y, value.z);
}

XMFLOAT4 WithAlpha(const XMFLOAT4 &color, float alpha)
{
	return XMFLOAT4(color.x, color.y, color.z, alpha);
}

ImU32 ToImColor(const XMFLOAT4 &color, bool isEnabled, float enabledAlpha = 1.0f, float disabledAlpha = 0.35f)
{
	return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, isEnabled ? enabledAlpha : disabledAlpha));
}

void DrawProjectedSegment(ImDrawList *drawList, const XMFLOAT3 &a, const XMFLOAT3 &b, const XMMATRIX &viewProj, const ImVec2 &displaySize,
                          ImU32 color, float thickness)
{
	ImVec2 screenA;
	ImVec2 screenB;
	if (ProjectWorldToScreen(a, viewProj, displaySize, screenA) && ProjectWorldToScreen(b, viewProj, displaySize, screenB))
	{
		drawList->AddLine(screenA, screenB, color, thickness);
	}
}

void DrawWorldCircle(ImDrawList *drawList, const XMFLOAT3 &center, float radius, const XMVECTOR &axisA, const XMVECTOR &axisB,
                     const XMMATRIX &viewProj, const ImVec2 &displaySize, ImU32 color, float thickness)
{
	static constexpr int SegmentCount = 48;
	if (radius <= 0.0f)
	{
		return;
	}

	XMFLOAT3 previousPoint;
	for (int segmentIndex = 0; segmentIndex <= SegmentCount; ++segmentIndex)
	{
		const float angle = XM_2PI * static_cast<float>(segmentIndex) / static_cast<float>(SegmentCount);
		const XMVECTOR offset = axisA * (cosf(angle) * radius) + axisB * (sinf(angle) * radius);
		const XMVECTOR point = XMLoadFloat3(&center) + offset;
		XMFLOAT3 currentPoint;
		XMStoreFloat3(&currentPoint, point);

		if (segmentIndex > 0)
		{
			DrawProjectedSegment(drawList, previousPoint, currentPoint, viewProj, displaySize, color, thickness);
		}
		previousPoint = currentPoint;
	}
}

void DrawPointLightBounds(ImDrawList *drawList, const LightSystem::PointLightData &light, bool isEnabled, const XMMATRIX &viewProj,
                          const ImVec2 &displaySize)
{
	const XMFLOAT3 center = ExtractPosition(light.Position);
	const float radius = light.Params.x;
	const ImU32 color = ToImColor(WithAlpha(light.Color, 1.0f), isEnabled, 0.42f, 0.16f);
	const float thickness = isEnabled ? 1.6f : 1.0f;

	DrawWorldCircle(drawList, center, radius, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), viewProj,
	                displaySize, color, thickness);
	DrawWorldCircle(drawList, center, radius, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), viewProj,
	                displaySize, color, thickness);
	DrawWorldCircle(drawList, center, radius, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), viewProj,
	                displaySize, color, thickness);
}

void DrawSpotLightBounds(ImDrawList *drawList, const LightSystem::SpotLightData &light, bool isEnabled, const XMMATRIX &viewProj,
                         const ImVec2 &displaySize)
{
	static constexpr int SegmentCount = 32;
	const XMFLOAT3 position = ExtractPosition(light.Position);
	const XMFLOAT3 directionF = ExtractDirection(light.Direction);
	XMVECTOR direction = XMLoadFloat3(&directionF);
	if (XMVector3NearEqual(direction, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return;
	}
	direction = XMVector3Normalize(direction);

	const float range = (std::max)(0.0f, light.Params.x);
	const float outerConeCos = (std::max)(-0.99f, (std::min)(0.99f, light.Params.z));
	const float baseRadius = range * tanf(acosf(outerConeCos));
	const XMVECTOR apex = XMLoadFloat3(&position);
	const XMVECTOR baseCenter = apex + direction * range;
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	const XMVECTOR fallbackUp = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	const XMVECTOR helperUp = fabsf(XMVectorGetX(XMVector3Dot(direction, worldUp))) > 0.95f ? fallbackUp : worldUp;
	const XMVECTOR axisA = XMVector3Normalize(XMVector3Cross(helperUp, direction));
	const XMVECTOR axisB = XMVector3Normalize(XMVector3Cross(direction, axisA));
	const ImU32 color = ToImColor(WithAlpha(light.Color, 1.0f), isEnabled, 0.48f, 0.16f);
	const float thickness = isEnabled ? 1.6f : 1.0f;

	XMFLOAT3 baseCenterF;
	XMStoreFloat3(&baseCenterF, baseCenter);
	DrawWorldCircle(drawList, baseCenterF, baseRadius, axisA, axisB, viewProj, displaySize, color, thickness);

	for (int segmentIndex = 0; segmentIndex < SegmentCount; segmentIndex += SegmentCount / 4)
	{
		const float angle = XM_2PI * static_cast<float>(segmentIndex) / static_cast<float>(SegmentCount);
		const XMVECTOR edge = baseCenter + axisA * (cosf(angle) * baseRadius) + axisB * (sinf(angle) * baseRadius);
		XMFLOAT3 edgeF;
		XMStoreFloat3(&edgeF, edge);
		DrawProjectedSegment(drawList, position, edgeF, viewProj, displaySize, color, thickness);
	}
}

void DrawPointLightMarker(ImDrawList *drawList, const LightSystem::PointLightData &light, bool isEnabled, float markerScale,
                          const XMMATRIX &viewProj, const ImVec2 &displaySize, int lightIndex)
{
	ImVec2 screenPosition;
	if (!ProjectWorldToScreen(ExtractPosition(light.Position), viewProj, displaySize, screenPosition))
	{
		return;
	}

	const ImU32 color = ToImColor(WithAlpha(light.Color, 1.0f), isEnabled);
	const float radius = (std::max)(4.0f, 6.0f * markerScale);
	drawList->AddCircleFilled(screenPosition, radius, color);
	drawList->AddCircle(screenPosition, radius + 1.5f, IM_COL32(255, 255, 255, 220), 0, 2.0f);

	const std::string label = "P" + std::to_string(lightIndex);
	drawList->AddText(ImVec2(screenPosition.x + radius + 4.0f, screenPosition.y - radius), IM_COL32(255, 255, 255, 220), label.c_str());
}

void DrawSpotLightMarker(ImDrawList *drawList, const LightSystem::SpotLightData &light, bool isEnabled, float markerScale,
                         const XMMATRIX &viewProj, const ImVec2 &displaySize, int lightIndex)
{
	const XMFLOAT3 position = ExtractPosition(light.Position);
	XMFLOAT3 direction = ExtractDirection(light.Direction);
	XMVECTOR directionVector = XMLoadFloat3(&direction);
	if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return;
	}
	directionVector = XMVector3Normalize(directionVector);
	XMStoreFloat3(&direction, directionVector);

	const float range = light.Params.x;
	const float visualLength = (std::min)(range * 0.35f, 10.0f);
	const XMFLOAT3 endPoint(position.x + direction.x * visualLength, position.y + direction.y * visualLength,
	                        position.z + direction.z * visualLength);

	ImVec2 screenPosition;
	ImVec2 screenEnd;
	if (!ProjectWorldToScreen(position, viewProj, displaySize, screenPosition))
	{
		return;
	}

	const ImU32 color = ToImColor(WithAlpha(light.Color, 1.0f), isEnabled);
	const float radius = (std::max)(4.0f, 5.0f * markerScale);
	drawList->AddCircleFilled(screenPosition, radius, color);
	drawList->AddCircle(screenPosition, radius + 1.0f, IM_COL32(255, 255, 255, 220), 0, 2.0f);

	if (ProjectWorldToScreen(endPoint, viewProj, displaySize, screenEnd))
	{
		drawList->AddLine(screenPosition, screenEnd, color, (std::max)(2.0f, 2.0f * markerScale));
		drawList->AddCircleFilled(screenEnd, 2.5f, color);
	}

	const std::string label = "S" + std::to_string(lightIndex);
	drawList->AddText(ImVec2(screenPosition.x + radius + 4.0f, screenPosition.y - radius), IM_COL32(255, 255, 255, 220), label.c_str());
}

void DrawDirectionalLightMarker(ImDrawList *drawList, const LightSystem::DirectionalLightData &light, bool isEnabled, float markerScale,
                                const XMMATRIX &viewProj, const ImVec2 &displaySize)
{
	XMFLOAT3 direction = ExtractDirection(light.Direction);
	XMVECTOR directionVector = XMLoadFloat3(&direction);
	if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
	{
		return;
	}
	directionVector = XMVector3Normalize(directionVector);
	XMStoreFloat3(&direction, directionVector);

	const float length = 6.0f;
	const XMFLOAT3 startPoint(-direction.x * length, -direction.y * length, -direction.z * length);
	const XMFLOAT3 endPoint(direction.x * length, direction.y * length, direction.z * length);

	ImVec2 screenStart;
	ImVec2 screenEnd;
	if (!ProjectWorldToScreen(startPoint, viewProj, displaySize, screenStart) ||
	    !ProjectWorldToScreen(endPoint, viewProj, displaySize, screenEnd))
	{
		return;
	}

	const ImU32 color = ToImColor(WithAlpha(light.Color, 1.0f), isEnabled);
	drawList->AddLine(screenStart, screenEnd, color, (std::max)(2.5f, 2.5f * markerScale));
	drawList->AddCircleFilled(screenEnd, (std::max)(4.0f, 4.0f * markerScale), color);
	drawList->AddText(ImVec2(screenEnd.x + 6.0f, screenEnd.y - 10.0f), IM_COL32(255, 255, 255, 220), "D0");
}
} // namespace

void DebugOverlay::Initialize(HWND windowHandle, ID3D12Device *device, ID3D12CommandQueue *commandQueue, DXGI_FORMAT backBufferFormat,
                              UINT framesInFlight)
{
	if (m_isInitialized)
	{
		return;
	}

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplWin32_Init(windowHandle);
	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = device;
	initInfo.CommandQueue = commandQueue;
	initInfo.NumFramesInFlight = framesInFlight;
	initInfo.RTVFormat = backBufferFormat;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.SrvDescriptorHeap = m_srvHeap.Get();
	initInfo.LegacySingleSrvCpuDescriptor = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
	initInfo.LegacySingleSrvGpuDescriptor = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
	ImGui_ImplDX12_Init(&initInfo);

	m_isInitialized = true;
}

void DebugOverlay::Shutdown()
{
	if (!m_isInitialized)
	{
		return;
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	m_isInitialized = false;
}

void DebugOverlay::Draw(ID3D12GraphicsCommandList *commandList, const FrameContext &frameContext)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO &io = ImGui::GetIO();
	UpdateSidebarLayout(io.DisplaySize.x, io.MousePos.x, ImGui::IsMouseDown(ImGuiMouseButton_Left));
	RenderSettings::DemoSettings demoSettings = frameContext.Render->GetDemoSettings();
	DrawSidebarWindow(frameContext, demoSettings);
	ImDrawList *overlayDrawList = ImGui::GetForegroundDrawList();
	DrawSidebarSplitter(io.DisplaySize.y, overlayDrawList);
	DrawLightGizmos(frameContext.Camera, *frameContext.Light, demoSettings, *frameContext.LightEdit, io.DisplaySize, overlayDrawList);

	ImGui::Render();

	ID3D12DescriptorHeap *descriptorHeaps[] = {m_srvHeap.Get()};
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void DebugOverlay::UpdateSidebarLayout(float displayWidth, float mouseX, bool isMouseDown)
{
	const float minSidebarWidth = 280.0f;
	const float maxSidebarWidth = (std::max)(minSidebarWidth, displayWidth * 0.6f);
	if (m_isResizingSidebar)
	{
		if (isMouseDown)
		{
			m_sidebarWidth = (std::max)(minSidebarWidth, (std::min)(maxSidebarWidth, mouseX));
		}
		else
		{
			m_isResizingSidebar = false;
		}
	}

	m_sidebarWidth = (std::max)(minSidebarWidth, (std::min)(maxSidebarWidth, m_sidebarWidth));
}

void DebugOverlay::DrawSidebarWindow(const FrameContext &frameContext, RenderSettings::DemoSettings &demoSettings)
{
	const GameTimer &gameTimer = *frameContext.Timer;
	MaterialSystem &materialSystem = *frameContext.Material;
	RenderSettings &renderSettings = *frameContext.Render;
	Demo::DemoShowcaseSession &showcaseSession = *frameContext.Showcase;
	Demo::DemoLightEditSession &lightEditSession = *frameContext.LightEdit;

	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(m_sidebarWidth, io.DisplaySize.y), ImGuiCond_Always);
	const ImGuiWindowFlags debugWindowFlags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Debug", nullptr, debugWindowFlags);
	ImGui::Text("Renderer: DirectX 12 + Dear ImGui");
	ImGui::Separator();
	ImGui::Text("FPS: %.1f", gameTimer.DeltaTime() > 0.0 ? (1.0 / gameTimer.DeltaTime()) : 0.0);
	ImGui::Text("Frame time: %.3f ms", gameTimer.DeltaTime() * 1000.0);

	const bool matchesRecommendedLook = Demo::DemoLightingController::MatchesRecommendedLook(materialSystem, renderSettings, lightEditSession);
	const ImVec4 lookStatusColor = matchesRecommendedLook ? ImVec4(0.55f, 0.88f, 0.62f, 1.0f) : ImVec4(0.95f, 0.78f, 0.42f, 1.0f);
	ImGui::TextColored(lookStatusColor, "%s", matchesRecommendedLook ? "Look: Recommended" : "Look: Custom");

	DrawDemoSection(renderSettings, demoSettings, showcaseSession);
	DrawViewSection();
	DrawAdvancedSection(frameContext.Camera, materialSystem, demoSettings);
	DrawLightingSection(materialSystem, renderSettings, demoSettings, lightEditSession);
	ImGui::End();
}

void DebugOverlay::DrawDemoSection(RenderSettings &renderSettings, RenderSettings::DemoSettings &demoSettings,
                                   Demo::DemoShowcaseSession &showcaseSession)
{
	if (!ImGui::CollapsingHeader("Demo"))
	{
		return;
	}

	if (ImGui::Checkbox("Enable demo controls", &demoSettings.EnableDemoControls))
	{
		renderSettings.SetDemoSettings(demoSettings);
	}
	if (ImGui::Checkbox("Enable PBR grid", &demoSettings.EnablePbrGrid))
	{
		renderSettings.SetDemoSettings(demoSettings);
	}
	if (demoSettings.EnablePbrGrid && ImGui::TreeNode("PBR grid"))
	{
		XMFLOAT3 pbrGridOffset = showcaseSession.GetPbrGridOffset();
		if (ImGui::DragFloat3("Offset##PbrGrid", &pbrGridOffset.x, 0.1f))
		{
			showcaseSession.SetPbrGridOffset(pbrGridOffset);
		}
		if (ImGui::Button("Reset PBR grid"))
		{
			Demo::DemoLightingController::ResetRecommendedPbrGridOffset(showcaseSession);
		}
		ImGui::TextUnformatted("Changes apply on the next frame.");
		ImGui::TreePop();
	}
}

void DebugOverlay::DrawViewSection()
{
	if (!ImGui::CollapsingHeader("View"))
	{
		return;
	}

	const char *viewModeLabels[] = {"Final",           "Albedo",          "Normal",           "Shadow cascade", "Shadow factor",
	                                "Metallic",        "Roughness",       "Ambient occlusion", "Direct lighting", "Ambient lighting"};
	const DebugViewMode debugViewModes[] = {DebugViewMode::Final,          DebugViewMode::Albedo,      DebugViewMode::Normal,
	                                        DebugViewMode::ShadowCascade,  DebugViewMode::ShadowFactor, DebugViewMode::Metallic,
	                                        DebugViewMode::Roughness,      DebugViewMode::AmbientOcclusion,
	                                        DebugViewMode::DirectLighting, DebugViewMode::AmbientLighting};
	int selectedViewIndex = 0;
	for (int viewIndex = 0; viewIndex < static_cast<int>(IM_ARRAYSIZE(debugViewModes)); ++viewIndex)
	{
		if (m_debugViewMode == debugViewModes[viewIndex])
		{
			selectedViewIndex = viewIndex;
			break;
		}
	}

	if (ImGui::Combo("Debug view", &selectedViewIndex, viewModeLabels, IM_ARRAYSIZE(viewModeLabels)))
	{
		m_debugViewMode = debugViewModes[selectedViewIndex];
	}
}

void DebugOverlay::DrawAdvancedSection(const CameraState &cameraState, MaterialSystem &materialSystem,
                                       const RenderSettings::DemoSettings &demoSettings)
{
	if (!demoSettings.EnableDemoControls || !ImGui::CollapsingHeader("Advanced"))
	{
		return;
	}

	XMFLOAT3 &eyePosition = *cameraState.EyePosition;
	XMFLOAT3 &lookDirection = *cameraState.LookDirection;
	float &yaw = *cameraState.Yaw;
	float &pitch = *cameraState.Pitch;
	float &cameraMoveSpeed = *cameraState.MoveSpeed;
	float &cameraMouseSensitivity = *cameraState.MouseSensitivity;

	if (ImGui::TreeNode("Camera"))
	{
		ImGui::DragFloat3("Position", &eyePosition.x, 0.1f);
		ImGui::DragFloat3("Look direction", &lookDirection.x, 0.01f);
		if (ImGui::Button("Reset look forward"))
		{
			lookDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);
			yaw = 0.0f;
			pitch = 0.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset camera"))
		{
			eyePosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
			lookDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);
			yaw = 0.0f;
			pitch = 0.0f;
		}
		ImGui::SliderFloat("Move speed", &cameraMoveSpeed, 1.0f, 50.0f);
		const float minMouseSensitivity = 0.0005f;
		const float maxMouseSensitivity = 0.02f;
		float sensitivityPercent = 1.0f;
		if (maxMouseSensitivity > minMouseSensitivity)
		{
			sensitivityPercent = 1.0f + ((cameraMouseSensitivity - minMouseSensitivity) / (maxMouseSensitivity - minMouseSensitivity)) * 99.0f;
		}
		sensitivityPercent = (std::max)(1.0f, (std::min)(100.0f, sensitivityPercent));
		if (ImGui::SliderFloat("Mouse sensitivity", &sensitivityPercent, 1.0f, 100.0f, "%.0f"))
		{
			const float normalizedValue = (sensitivityPercent - 1.0f) / 99.0f;
			cameraMouseSensitivity = minMouseSensitivity + normalizedValue * (maxMouseSensitivity - minMouseSensitivity);
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Material"))
	{
		MaterialSystem::MaterialState materialState = materialSystem.GetMaterialState();
		if (ImGui::Button("Apply recommended material"))
		{
			materialState = Demo::DemoLightingController::BuildRecommendedMaterialState();
			materialSystem.SetMaterialState(materialState);
			materialState = materialSystem.GetMaterialState();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset material"))
		{
			materialState = Demo::DemoLightingController::BuildRecommendedMaterialState();
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::ColorEdit3("Base color", &materialState.DiffuseAlbedo.x))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Opacity", &materialState.DiffuseAlbedo.w, 0.0f, 1.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Metallic", &materialState.PbrParams.x, 0.0f, 1.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Roughness", &materialState.PbrParams.y, 0.04f, 1.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Ambient occlusion", &materialState.PbrParams.z, 0.0f, 1.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("IBL intensity", &materialState.PbrParams.w, 0.0f, 2.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		ImGui::TreePop();
	}
}

void DebugOverlay::DrawLightingSection(MaterialSystem &materialSystem, RenderSettings &renderSettings,
                                       const RenderSettings::DemoSettings &demoSettings, Demo::DemoLightEditSession &lightEditSession)
{
	if (!ImGui::CollapsingHeader("Lighting"))
	{
		return;
	}

	RenderSettings::LightingSettings lightingSettings = renderSettings.GetLightingSettings();
	RenderSettings::ImageBasedLightingSettings imageBasedLightingSettings = renderSettings.GetImageBasedLightingSettings();
	RenderSettings::ShadowSettings shadowSettings = renderSettings.GetShadowSettings();
	Demo::DemoLightEditState lightEditState = lightEditSession.GetState();
	if (ImGui::Button("Apply recommended look"))
	{
		Demo::DemoLightingController::ApplyRecommendedLook(materialSystem, renderSettings, lightEditSession);
		lightingSettings = renderSettings.GetLightingSettings();
		imageBasedLightingSettings = renderSettings.GetImageBasedLightingSettings();
		shadowSettings = renderSettings.GetShadowSettings();
		lightEditState = lightEditSession.GetState();
	}
	if (ImGui::ColorEdit3("Ambient", &lightingSettings.AmbientLight.x))
	{
		renderSettings.SetLightingSettings(lightingSettings);
	}
	if (ImGui::SliderFloat("Ambient intensity", &lightingSettings.AmbientLight.w, 0.0f, 2.0f))
	{
		renderSettings.SetLightingSettings(lightingSettings);
	}
	if (ImGui::ColorEdit3("Background", &lightingSettings.BackgroundColor.x))
	{
		renderSettings.SetLightingSettings(lightingSettings);
	}
	if (ImGui::ColorEdit3("Ambient floor", &lightingSettings.AmbientFloor.x))
	{
		renderSettings.SetLightingSettings(lightingSettings);
	}
	if (ImGui::SliderFloat("Ambient floor intensity", &lightingSettings.AmbientFloor.w, 0.0f, 3.0f))
	{
		renderSettings.SetLightingSettings(lightingSettings);
	}

	ImGui::SeparatorText("IBL");
	if (ImGui::Checkbox("Show skybox", &imageBasedLightingSettings.ShowSkybox))
	{
		renderSettings.SetImageBasedLightingSettings(imageBasedLightingSettings);
	}
	if (ImGui::SliderFloat("Diffuse IBL", &imageBasedLightingSettings.DiffuseStrength, 0.0f, 2.0f, "%.2f"))
	{
		renderSettings.SetImageBasedLightingSettings(imageBasedLightingSettings);
	}
	if (ImGui::SliderFloat("Specular IBL", &imageBasedLightingSettings.SpecularStrength, 0.0f, 2.0f, "%.2f"))
	{
		renderSettings.SetImageBasedLightingSettings(imageBasedLightingSettings);
	}
	if (ImGui::SliderFloat("Skybox intensity", &imageBasedLightingSettings.SkyboxIntensity, 0.0f, 5.0f, "%.2f"))
	{
		renderSettings.SetImageBasedLightingSettings(imageBasedLightingSettings);
	}
	if (ImGui::SliderFloat("Exposure", &imageBasedLightingSettings.Exposure, 0.25f, 5.0f, "%.2f"))
	{
		renderSettings.SetImageBasedLightingSettings(imageBasedLightingSettings);
	}

	if (!demoSettings.EnableDemoControls)
	{
		ImGui::SeparatorText("Shadows");
		if (ImGui::Checkbox("Enable directional shadows", &shadowSettings.EnableDirectionalShadows))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		ImGui::TextUnformatted("Advanced shadow and light controls are available in Demo mode.");
		return;
	}

	if (ImGui::TreeNode("Shadows"))
	{
		ImGui::TextUnformatted("Current scope: directional light only.");
		if (ImGui::Checkbox("Enable directional shadows", &shadowSettings.EnableDirectionalShadows))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		int cascadeCount = static_cast<int>(shadowSettings.CascadeCount);
		if (ImGui::SliderInt("Cascade count", &cascadeCount, 1, 4))
		{
			shadowSettings.CascadeCount = static_cast<std::uint32_t>(cascadeCount);
			renderSettings.SetShadowSettings(shadowSettings);
		}
		int shadowMapSize = static_cast<int>(shadowSettings.ShadowMapSize);
		if (ImGui::SliderInt("Shadow map size", &shadowMapSize, 512, 4096))
		{
			shadowMapSize = (std::max)(512, shadowMapSize);
			shadowMapSize = ((shadowMapSize + 255) / 256) * 256;
			shadowSettings.ShadowMapSize = static_cast<std::uint32_t>(shadowMapSize);
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Cascade split lambda", &shadowSettings.CascadeSplitLambda, 0.0f, 1.0f, "%.2f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Max shadow distance", &shadowSettings.MaxShadowDistance, 25.0f, 500.0f, "%.1f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Depth bias", &shadowSettings.DepthBias, 0.0f, 10000.0f, "%.0f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Slope bias", &shadowSettings.SlopeScaledDepthBias, 0.0f, 8.0f, "%.2f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Bias clamp", &shadowSettings.DepthBiasClamp, 0.0f, 10.0f, "%.3f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("PCF kernel radius", &shadowSettings.PcfRadius, 0.0f, 3.0f, "%.2f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Shadow strength", &shadowSettings.ShadowStrength, 0.0f, 1.0f, "%.2f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Receiver bias min", &shadowSettings.ReceiverBiasMin, 0.00001f, 0.001f, "%.5f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Receiver bias slope", &shadowSettings.ReceiverBiasSlopeScale, 0.0f, 0.005f, "%.5f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		if (ImGui::SliderFloat("Receiver bias texel", &shadowSettings.ReceiverBiasTexelFactor, 0.0f, 4.0f, "%.2f"))
		{
			renderSettings.SetShadowSettings(shadowSettings);
		}
		ImGui::TreePop();
	}

	if (!ImGui::TreeNode("Lights"))
	{
		return;
	}

	ImGui::Checkbox("Show light markers", &m_showLightMarkers);
	ImGui::Checkbox("Show light bounds", &m_showLightBounds);
	ImGui::SliderFloat("Marker scale", &m_lightMarkerScale, 0.5f, 2.5f, "%.2f");
	if (ImGui::Button("Reset light positions"))
	{
		Demo::DemoLightingController::ResetRecommendedLightPositions(lightEditSession);
		lightEditState = lightEditSession.GetState();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset all lights"))
	{
		Demo::DemoLightingController::ApplyRecommendedLightPreset(lightEditSession);
		lightEditState = lightEditSession.GetState();
	}

	if (ImGui::TreeNode("Directional"))
	{
		bool enableDirectionalLight = lightEditState.EnableState.DirectionalLights[0];
		if (ImGui::Checkbox("Directional 0", &enableDirectionalLight))
		{
			lightEditState.EnableState.DirectionalLights[0] = enableDirectionalLight;
			lightEditSession.SetState(lightEditState);
		}
		if (ImGui::ColorEdit3("Directional 0 color", &lightEditState.ColorState.DirectionalLights[0].x))
		{
			lightEditSession.SetState(lightEditState);
		}
		if (ImGui::SliderFloat("Directional 0 intensity", &lightEditState.IntensityState.DirectionalLights[0], 0.0f, 12.0f, "%.2f"))
		{
			lightEditSession.SetState(lightEditState);
		}
		if (ImGui::DragFloat3("Directional 0 direction", &lightEditState.DirectionState.DirectionalLights[0].x, 0.01f, -1.0f, 1.0f))
		{
			lightEditSession.SetState(lightEditState);
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Point"))
	{
		for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::PointLightCount); ++lightIndex)
		{
			bool isEnabled = lightEditState.EnableState.PointLights[lightIndex];
			std::string label = "Point " + std::to_string(lightIndex);
			if (ImGui::Checkbox(label.c_str(), &isEnabled))
			{
				lightEditState.EnableState.PointLights[lightIndex] = isEnabled;
				lightEditSession.SetState(lightEditState);
			}
			ImGui::SameLine();
			std::string colorLabel = "Color##Point" + std::to_string(lightIndex);
			if (ImGui::ColorEdit3(colorLabel.c_str(), &lightEditState.ColorState.PointLights[lightIndex].x))
			{
				lightEditSession.SetState(lightEditState);
			}
			std::string intensityLabel = "Intensity##Point" + std::to_string(lightIndex);
			if (ImGui::SliderFloat(intensityLabel.c_str(), &lightEditState.IntensityState.PointLights[lightIndex], 0.0f, 20.0f, "%.2f"))
			{
				lightEditSession.SetState(lightEditState);
			}
			std::string positionLabel = "Position##Point" + std::to_string(lightIndex);
			if (ImGui::DragFloat3(positionLabel.c_str(), &lightEditState.PositionState.PointLights[lightIndex].x, 0.1f))
			{
				lightEditSession.SetState(lightEditState);
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Spot"))
	{
		ImGui::TextUnformatted("Spot 0 follows the camera.");
		for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::SpotLightCount); ++lightIndex)
		{
			bool isEnabled = lightEditState.EnableState.SpotLights[lightIndex];
			std::string label = "Spot " + std::to_string(lightIndex);
			if (ImGui::Checkbox(label.c_str(), &isEnabled))
			{
				lightEditState.EnableState.SpotLights[lightIndex] = isEnabled;
				lightEditSession.SetState(lightEditState);
			}
			ImGui::SameLine();
			std::string colorLabel = "Color##Spot" + std::to_string(lightIndex);
			if (ImGui::ColorEdit3(colorLabel.c_str(), &lightEditState.ColorState.SpotLights[lightIndex].x))
			{
				lightEditSession.SetState(lightEditState);
			}
			std::string intensityLabel = "Intensity##Spot" + std::to_string(lightIndex);
			if (ImGui::SliderFloat(intensityLabel.c_str(), &lightEditState.IntensityState.SpotLights[lightIndex], 0.0f, 24.0f, "%.2f"))
			{
				lightEditSession.SetState(lightEditState);
			}
			if (lightIndex == 1 && ImGui::DragFloat3("Position##Spot1", &lightEditState.PositionState.SecondarySpotLight.x, 0.1f))
			{
				lightEditSession.SetState(lightEditState);
			}
		}
		ImGui::TreePop();
	}

	ImGui::TreePop();
}

void DebugOverlay::DrawSidebarSplitter(float displayHeight, ImDrawList *overlayDrawList)
{
	const float splitterThickness = 8.0f;
	const ImVec2 splitterMin(m_sidebarWidth - splitterThickness * 0.5f, 0.0f);
	const ImVec2 splitterMax(m_sidebarWidth + splitterThickness * 0.5f, displayHeight);
	const bool splitterHovered = ImGui::IsMouseHoveringRect(splitterMin, splitterMax, false);
	if (!m_isResizingSidebar && splitterHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		m_isResizingSidebar = true;
	}
	if (splitterHovered || m_isResizingSidebar)
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}

	const ImU32 splitterColor =
	    ImGui::ColorConvertFloat4ToU32(splitterHovered || m_isResizingSidebar ? ImVec4(0.38f, 0.60f, 0.92f, 1.0f) : ImVec4(0.22f, 0.27f, 0.34f, 1.0f));
	overlayDrawList->AddLine(ImVec2(m_sidebarWidth, 0.0f), ImVec2(m_sidebarWidth, displayHeight), splitterColor, 2.0f);
}

void DebugOverlay::DrawLightGizmos(const CameraState &cameraState, const LightSystem &lightSystem,
                                   const RenderSettings::DemoSettings &demoSettings, Demo::DemoLightEditSession &lightEditSession,
                                   const ImVec2 &displaySize, ImDrawList *overlayDrawList)
{
	if (!demoSettings.EnableDemoControls || (!m_showLightMarkers && !m_showLightBounds))
	{
		return;
	}

	const XMMATRIX viewProj = BuildViewProjection(*cameraState.EyePosition, *cameraState.LookDirection, displaySize);
	const LightSystem::LightingState &lightingState = lightSystem.GetLightingState();
	const Demo::DemoLightEditState lightEditState = lightEditSession.GetState();

	LightSystem::DirectionalLightData directionalLight = lightingState.DirectionalLights[0];
	directionalLight.Color = lightEditState.ColorState.DirectionalLights[0];
	if (m_showLightMarkers)
	{
		DrawDirectionalLightMarker(overlayDrawList, directionalLight, lightEditState.EnableState.DirectionalLights[0], m_lightMarkerScale,
		                           viewProj, displaySize);
	}

	for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::PointLightCount); ++lightIndex)
	{
		LightSystem::PointLightData pointLight = lightingState.PointLights[lightIndex];
		pointLight.Color = lightEditState.ColorState.PointLights[lightIndex];
		if (m_showLightBounds)
		{
			DrawPointLightBounds(overlayDrawList, pointLight, lightEditState.EnableState.PointLights[lightIndex], viewProj, displaySize);
		}
		if (m_showLightMarkers)
		{
			DrawPointLightMarker(overlayDrawList, pointLight, lightEditState.EnableState.PointLights[lightIndex], m_lightMarkerScale, viewProj,
			                     displaySize, lightIndex);
		}
	}

	for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::SpotLightCount); ++lightIndex)
	{
		LightSystem::SpotLightData spotLight = lightingState.SpotLights[lightIndex];
		spotLight.Color = lightEditState.ColorState.SpotLights[lightIndex];
		if (m_showLightBounds)
		{
			DrawSpotLightBounds(overlayDrawList, spotLight, lightEditState.EnableState.SpotLights[lightIndex], viewProj, displaySize);
		}
		if (m_showLightMarkers)
		{
			DrawSpotLightMarker(overlayDrawList, spotLight, lightEditState.EnableState.SpotLights[lightIndex], m_lightMarkerScale, viewProj,
			                    displaySize, lightIndex);
		}
	}
}
