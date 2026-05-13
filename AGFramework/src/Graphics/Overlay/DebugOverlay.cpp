#include "DebugOverlay.h"

#include "../../Core/GameTimer.h"
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
	bool ProjectWorldToScreen(
		const XMFLOAT3& worldPosition,
		const XMMATRIX& viewProj,
		const ImVec2& displaySize,
		ImVec2& screenPosition)
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

	XMMATRIX BuildViewProjection(const XMFLOAT3& eyePosition, const XMFLOAT3& lookDirection, const ImVec2& displaySize)
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

	XMFLOAT3 ExtractPosition(const XMFLOAT4& value)
	{
		return XMFLOAT3(value.x, value.y, value.z);
	}

	XMFLOAT3 ExtractDirection(const XMFLOAT4& value)
	{
		return XMFLOAT3(value.x, value.y, value.z);
	}

	void DrawPointLightMarker(
		ImDrawList* drawList,
		const LightSystem::PointLightData& light,
		bool isEnabled,
		float markerScale,
		const XMMATRIX& viewProj,
		const ImVec2& displaySize,
		int lightIndex)
	{
		ImVec2 screenPosition;
		if (!ProjectWorldToScreen(ExtractPosition(light.Position), viewProj, displaySize, screenPosition))
		{
			return;
		}

		const ImU32 color = ImGui::ColorConvertFloat4ToU32(
			ImVec4(light.Color.x, light.Color.y, light.Color.z, isEnabled ? 1.0f : 0.35f));
		const float radius = (std::max)(4.0f, 6.0f * markerScale);
		drawList->AddCircleFilled(screenPosition, radius, color);
		drawList->AddCircle(screenPosition, radius + 1.5f, IM_COL32(255, 255, 255, 220), 0, 2.0f);

		const std::string label = "P" + std::to_string(lightIndex);
		drawList->AddText(ImVec2(screenPosition.x + radius + 4.0f, screenPosition.y - radius), IM_COL32(255, 255, 255, 220), label.c_str());
	}

	void DrawSpotLightMarker(
		ImDrawList* drawList,
		const LightSystem::SpotLightData& light,
		bool isEnabled,
		float markerScale,
		const XMMATRIX& viewProj,
		const ImVec2& displaySize,
		int lightIndex)
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
		const XMFLOAT3 endPoint(
			position.x + direction.x * visualLength,
			position.y + direction.y * visualLength,
			position.z + direction.z * visualLength);

		ImVec2 screenPosition;
		ImVec2 screenEnd;
		if (!ProjectWorldToScreen(position, viewProj, displaySize, screenPosition))
		{
			return;
		}

		const ImU32 color = ImGui::ColorConvertFloat4ToU32(
			ImVec4(light.Color.x, light.Color.y, light.Color.z, isEnabled ? 1.0f : 0.35f));
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

	void DrawDirectionalLightMarker(
		ImDrawList* drawList,
		const LightSystem::DirectionalLightData& light,
		bool isEnabled,
		float markerScale,
		const XMMATRIX& viewProj,
		const ImVec2& displaySize)
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
		const XMFLOAT3 startPoint(
			-direction.x * length,
			-direction.y * length,
			-direction.z * length);
		const XMFLOAT3 endPoint(
			direction.x * length,
			direction.y * length,
			direction.z * length);

		ImVec2 screenStart;
		ImVec2 screenEnd;
		if (!ProjectWorldToScreen(startPoint, viewProj, displaySize, screenStart) ||
			!ProjectWorldToScreen(endPoint, viewProj, displaySize, screenEnd))
		{
			return;
		}

		const ImU32 color = ImGui::ColorConvertFloat4ToU32(
			ImVec4(light.Color.x, light.Color.y, light.Color.z, isEnabled ? 1.0f : 0.35f));
		drawList->AddLine(screenStart, screenEnd, color, (std::max)(2.5f, 2.5f * markerScale));
		drawList->AddCircleFilled(screenEnd, (std::max)(4.0f, 4.0f * markerScale), color);
		drawList->AddText(ImVec2(screenEnd.x + 6.0f, screenEnd.y - 10.0f), IM_COL32(255, 255, 255, 220), "D0");
	}
}

void DebugOverlay::Initialize(
	HWND windowHandle,
	ID3D12Device* device,
	ID3D12CommandQueue* commandQueue,
	DXGI_FORMAT backBufferFormat,
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

	ImGuiIO& io = ImGui::GetIO();
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

void DebugOverlay::Draw(
	ID3D12GraphicsCommandList* commandList,
	const GameTimer& gameTimer,
	XMFLOAT3& eyePosition,
	XMFLOAT3& lookDirection,
	float& yaw,
	float& pitch,
	float& cameraMoveSpeed,
	float& cameraMouseSensitivity,
	MaterialSystem& materialSystem,
	RenderSettings& renderSettings,
	LightSystem& lightSystem)
{
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Debug");
	ImGui::Text("Renderer: DirectX 12 + Dear ImGui");
	ImGui::Separator();
	ImGui::Text("FPS: %.1f", gameTimer.DeltaTime() > 0.0 ? (1.0 / gameTimer.DeltaTime()) : 0.0);
	ImGui::Text("Frame time: %.3f ms", gameTimer.DeltaTime() * 1000.0);
	if (ImGui::CollapsingHeader("View"))
	{
		const char* viewModeLabels[] =
		{
			"Final",
			"Albedo",
			"Normal",
			"Position",
			"Shadow cascade",
			"Shadow factor",
			"Spot shadow factor",
			"Spot shadow frustum",
			"Directional shadow map",
			"Spot shadow map",
			"Directional shadow frustum"
		};
		int debugViewMode = static_cast<int>(m_debugViewMode);
		if (ImGui::Combo("Debug view", &debugViewMode, viewModeLabels, IM_ARRAYSIZE(viewModeLabels)))
		{
			m_debugViewMode = static_cast<DebugViewMode>(debugViewMode);
		}
		ImGui::SliderInt("Shadow debug cascade", &m_shadowDebugCascadeIndex, 0, 3);
	}

	if (ImGui::CollapsingHeader("Camera"))
	{
		ImGui::DragFloat3("Position", &eyePosition.x, 0.1f);
		ImGui::DragFloat3("Look direction", &lookDirection.x, 0.01f);
		ImGui::TextUnformatted("Use actions below to realign the camera.");
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
			sensitivityPercent =
				1.0f + ((cameraMouseSensitivity - minMouseSensitivity) / (maxMouseSensitivity - minMouseSensitivity)) * 99.0f;
		}
		sensitivityPercent = (std::max)(1.0f, (std::min)(100.0f, sensitivityPercent));
		if (ImGui::SliderFloat("Mouse sensitivity", &sensitivityPercent, 1.0f, 100.0f, "%.0f"))
		{
			const float normalizedValue = (sensitivityPercent - 1.0f) / 99.0f;
			cameraMouseSensitivity = minMouseSensitivity + normalizedValue * (maxMouseSensitivity - minMouseSensitivity);
		}
	}

	if (ImGui::CollapsingHeader("Material"))
	{
		MaterialSystem::MaterialState materialState = materialSystem.GetMaterialState();
		if (ImGui::ColorEdit3("Diffuse", &materialState.DiffuseAlbedo.x))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Opacity", &materialState.DiffuseAlbedo.w, 0.0f, 1.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::ColorEdit3("Specular", &materialState.SpecularAlbedo.x))
		{
			materialSystem.SetMaterialState(materialState);
		}
		if (ImGui::SliderFloat("Shininess", &materialState.SpecularAlbedo.w, 1.0f, 128.0f))
		{
			materialSystem.SetMaterialState(materialState);
		}
	}

	if (ImGui::CollapsingHeader("Lighting"))
	{
		RenderSettings::LightingSettings lightingSettings = renderSettings.GetLightingSettings();
		RenderSettings::ShadowSettings shadowSettings = renderSettings.GetShadowSettings();
		RenderSettings::SpotShadowSettings spotShadowSettings = renderSettings.GetSpotShadowSettings();
		LightSystem::LightEnableState lightEnableState = lightSystem.GetLightEnableState();
		if (ImGui::ColorEdit3("Ambient", &lightingSettings.AmbientLight.x))
		{
			renderSettings.SetLightingSettings(lightingSettings);
		}
		if (ImGui::SliderFloat("Ambient intensity", &lightingSettings.AmbientLight.w, 0.0f, 2.0f))
		{
			renderSettings.SetLightingSettings(lightingSettings);
		}

		ImGui::SeparatorText("Shadows");
		ImGui::TextUnformatted("Current scope: directional light only");
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
		if (ImGui::SliderFloat("PCF radius", &shadowSettings.PcfRadius, 0.0f, 4.0f, "%.2f"))
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

		ImGui::SeparatorText("Spot shadows");
		ImGui::TextUnformatted("Current scope: only Spot 0 casts shadows");
		if (ImGui::Checkbox("Enable spot shadows", &spotShadowSettings.EnableSpotShadows))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		int spotShadowMapSize = static_cast<int>(spotShadowSettings.ShadowMapSize);
		if (ImGui::SliderInt("Spot shadow map size", &spotShadowMapSize, 512, 4096))
		{
			spotShadowMapSize = (std::max)(512, spotShadowMapSize);
			spotShadowMapSize = ((spotShadowMapSize + 255) / 256) * 256;
			spotShadowSettings.ShadowMapSize = static_cast<std::uint32_t>(spotShadowMapSize);
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot depth bias", &spotShadowSettings.DepthBias, 0.0f, 10000.0f, "%.0f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot slope bias", &spotShadowSettings.SlopeScaledDepthBias, 0.0f, 8.0f, "%.2f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot bias clamp", &spotShadowSettings.DepthBiasClamp, 0.0f, 10.0f, "%.3f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot PCF radius", &spotShadowSettings.PcfRadius, 0.0f, 4.0f, "%.2f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot shadow strength", &spotShadowSettings.ShadowStrength, 0.0f, 1.0f, "%.2f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot receiver bias min", &spotShadowSettings.ReceiverBiasMin, 0.00001f, 0.001f, "%.5f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot receiver bias slope", &spotShadowSettings.ReceiverBiasSlopeScale, 0.0f, 0.005f, "%.5f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}
		if (ImGui::SliderFloat("Spot receiver bias texel", &spotShadowSettings.ReceiverBiasTexelFactor, 0.0f, 4.0f, "%.2f"))
		{
			renderSettings.SetSpotShadowSettings(spotShadowSettings);
		}

		ImGui::SeparatorText("Lights");
		ImGui::Checkbox("Show light markers", &m_showLightMarkers);
		ImGui::SliderFloat("Marker scale", &m_lightMarkerScale, 0.5f, 2.5f, "%.2f");

		if (ImGui::TreeNode("Directional"))
		{
			bool enableDirectionalLight = lightEnableState.DirectionalLights[0];
			if (ImGui::Checkbox("Directional 0", &enableDirectionalLight))
			{
				lightEnableState.DirectionalLights[0] = enableDirectionalLight;
				lightSystem.SetLightEnableState(lightEnableState);
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Point"))
		{
			for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::PointLightCount); ++lightIndex)
			{
				bool isEnabled = lightEnableState.PointLights[lightIndex];
				std::string label = "Point " + std::to_string(lightIndex);
				if (ImGui::Checkbox(label.c_str(), &isEnabled))
				{
					lightEnableState.PointLights[lightIndex] = isEnabled;
					lightSystem.SetLightEnableState(lightEnableState);
				}
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNode("Spot"))
		{
			for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::SpotLightCount); ++lightIndex)
			{
				bool isEnabled = lightEnableState.SpotLights[lightIndex];
				std::string label = "Spot " + std::to_string(lightIndex);
				if (lightIndex == static_cast<int>(LightSystem::ShadowCastingSpotLightIndex))
				{
					label += " (casts shadows)";
				}
				else
				{
					label += " (no shadows)";
				}
				if (ImGui::Checkbox(label.c_str(), &isEnabled))
				{
					lightEnableState.SpotLights[lightIndex] = isEnabled;
					lightSystem.SetLightEnableState(lightEnableState);
				}
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();

	if (m_showLightMarkers)
	{
		const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
		const XMMATRIX viewProj = BuildViewProjection(eyePosition, lookDirection, displaySize);
		const LightSystem::LightingState& lightingState = lightSystem.GetLightingState();
		const LightSystem::LightEnableState& lightEnableState = lightSystem.GetLightEnableState();
		ImDrawList* drawList = ImGui::GetForegroundDrawList();

		DrawDirectionalLightMarker(
			drawList,
			lightingState.DirectionalLights[0],
			lightEnableState.DirectionalLights[0],
			m_lightMarkerScale,
			viewProj,
			displaySize);

		for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::PointLightCount); ++lightIndex)
		{
			DrawPointLightMarker(
				drawList,
				lightingState.PointLights[lightIndex],
				lightEnableState.PointLights[lightIndex],
				m_lightMarkerScale,
				viewProj,
				displaySize,
				lightIndex);
		}

		for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::SpotLightCount); ++lightIndex)
		{
			DrawSpotLightMarker(
				drawList,
				lightingState.SpotLights[lightIndex],
				lightEnableState.SpotLights[lightIndex],
				m_lightMarkerScale,
				viewProj,
				displaySize,
				lightIndex);
		}
	}

	ImGui::Render();

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
