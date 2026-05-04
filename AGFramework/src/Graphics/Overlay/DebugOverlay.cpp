#include "DebugOverlay.h"

#include "../../Core/GameTimer.h"
#include "../LightSystem.h"
#include "../MaterialSystem.h"
#include "../RenderSettings.h"
#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/backends/imgui_impl_dx12.h"
#include "../../../external/imgui/backends/imgui_impl_win32.h"
#include <string>

using namespace DirectX;

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
	if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const char* viewModeLabels[] = { "Final", "Albedo", "Normal", "Position" };
		int debugViewMode = static_cast<int>(m_debugViewMode);
		if (ImGui::Combo("Debug view", &debugViewMode, viewModeLabels, IM_ARRAYSIZE(viewModeLabels)))
		{
			m_debugViewMode = static_cast<DebugViewMode>(debugViewMode);
		}
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
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
		ImGui::SliderFloat("Mouse sensitivity", &cameraMouseSensitivity, 0.0005f, 0.02f, "%.4f");
	}

	if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
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

	if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen))
	{
		RenderSettings::LightingSettings lightingSettings = renderSettings.GetLightingSettings();
		LightSystem::LightEnableState lightEnableState = lightSystem.GetLightEnableState();
		if (ImGui::ColorEdit3("Ambient", &lightingSettings.AmbientLight.x))
		{
			renderSettings.SetLightingSettings(lightingSettings);
		}
		if (ImGui::SliderFloat("Ambient intensity", &lightingSettings.AmbientLight.w, 0.0f, 2.0f))
		{
			renderSettings.SetLightingSettings(lightingSettings);
		}

		if (ImGui::TreeNodeEx("Directional", ImGuiTreeNodeFlags_DefaultOpen))
		{
			bool enableDirectionalLight = lightEnableState.DirectionalLights[0];
			if (ImGui::Checkbox("Directional 0", &enableDirectionalLight))
			{
				lightEnableState.DirectionalLights[0] = enableDirectionalLight;
				lightSystem.SetLightEnableState(lightEnableState);
			}
			ImGui::TreePop();
		}

		if (ImGui::TreeNodeEx("Point", ImGuiTreeNodeFlags_DefaultOpen))
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

		if (ImGui::TreeNodeEx("Spot", ImGuiTreeNodeFlags_DefaultOpen))
		{
			for (int lightIndex = 0; lightIndex < static_cast<int>(LightSystem::SpotLightCount); ++lightIndex)
			{
				bool isEnabled = lightEnableState.SpotLights[lightIndex];
				std::string label = "Spot " + std::to_string(lightIndex);
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

	ImGui::Render();

	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}
