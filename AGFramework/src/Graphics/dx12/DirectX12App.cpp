#include "DirectX12App.h"
#include "../ObjModelLoader.h"
#include "../../Core/GameTimer.h"
#include <limits>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
	XMVECTOR GetSafeNormalizedDirection(const XMFLOAT3& direction, const XMVECTOR& fallbackDirection = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f))
	{
		const XMVECTOR directionVector = XMLoadFloat3(&direction);
		if (XMVector3NearEqual(directionVector, XMVectorZero(), XMVectorReplicate(0.0001f)))
		{
			return fallbackDirection;
		}

		return XMVector3Normalize(directionVector);
	}

	struct TgaTextureData
	{
		UINT Width = 0;
		UINT Height = 0;
		std::vector<std::uint8_t> Pixels;
	};

	std::wstring ResolveShaderPath(const std::wstring& shaderRelativePath)
	{
		const std::wstring candidates[] =
		{
			shaderRelativePath,
			L"..\\" + shaderRelativePath,
			L"..\\..\\" + shaderRelativePath,
			L"AGFramework\\" + shaderRelativePath
		};

		for (const std::wstring& candidate : candidates)
		{
			const DWORD attributes = GetFileAttributesW(candidate.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return candidate;
			}
		}

		return shaderRelativePath;
	}

	std::wstring ResolveAssetPath(const std::wstring& assetRelativePath)
	{
		const std::wstring candidates[] =
		{
			assetRelativePath,
			L"..\\" + assetRelativePath,
			L"..\\..\\" + assetRelativePath,
			L"AGFramework\\" + assetRelativePath
		};

		for (const std::wstring& candidate : candidates)
		{
			const DWORD attributes = GetFileAttributesW(candidate.c_str());
			if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
			{
				return candidate;
			}
		}

		return assetRelativePath;
	}

	std::uint16_t ReadUInt16LE(const std::uint8_t* bytes)
	{
		return static_cast<std::uint16_t>(bytes[0] | (bytes[1] << 8));
	}

	std::string WStringToString(const std::wstring& wideString)
	{
		if (wideString.empty())
		{
			return std::string();
		}

		const int sizeRequired = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string result(sizeRequired > 0 ? sizeRequired - 1 : 0, '\0');

		if (sizeRequired > 1)
		{
			WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, &result[0], sizeRequired - 1, nullptr, nullptr);
		}

		return result;
	}

	TgaTextureData LoadUncompressedTga(const std::wstring& filename)
	{
		std::ifstream input(filename, std::ios::binary);
		if (!input)
		{
			throw std::runtime_error("Failed to open TGA texture file.");
		}

		std::array<std::uint8_t, 18> header{};
		input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
		if (!input)
		{
			throw std::runtime_error("Failed to read TGA header.");
		}

		const std::uint8_t idLength = header[0];
		const std::uint8_t colorMapType = header[1];
		const std::uint8_t imageType = header[2];
		const std::uint16_t width = ReadUInt16LE(&header[12]);
		const std::uint16_t height = ReadUInt16LE(&header[14]);
		const std::uint8_t bitsPerPixel = header[16];
		const std::uint8_t imageDescriptor = header[17];

		if (colorMapType != 0 || imageType != 2)
		{
			throw std::runtime_error("Only uncompressed true-color TGA textures are supported.");
		}

		if (bitsPerPixel != 24 && bitsPerPixel != 32)
		{
			throw std::runtime_error("Unsupported TGA pixel format.");
		}

		if (idLength > 0)
		{
			input.seekg(idLength, std::ios::cur);
		}

		const UINT srcPixelSize = bitsPerPixel / 8;
		const size_t srcDataSize = static_cast<size_t>(width) * height * srcPixelSize;
		std::vector<std::uint8_t> srcPixels(srcDataSize);
		input.read(reinterpret_cast<char*>(srcPixels.data()), static_cast<std::streamsize>(srcDataSize));
		if (!input)
		{
			throw std::runtime_error("Failed to read TGA pixel data.");
		}

		TgaTextureData textureData;
		textureData.Width = width;
		textureData.Height = height;
		textureData.Pixels.resize(static_cast<size_t>(width) * height * 4);

		const bool topLeftOrigin = (imageDescriptor & 0x20) != 0;

		for (UINT y = 0; y < height; ++y)
		{
			const UINT srcY = topLeftOrigin ? y : (height - 1 - y);
			for (UINT x = 0; x < width; ++x)
			{
				const size_t srcIndex = (static_cast<size_t>(srcY) * width + x) * srcPixelSize;
				const size_t dstIndex = (static_cast<size_t>(y) * width + x) * 4;

				textureData.Pixels[dstIndex + 0] = srcPixels[srcIndex + 2];
				textureData.Pixels[dstIndex + 1] = srcPixels[srcIndex + 1];
				textureData.Pixels[dstIndex + 2] = srcPixels[srcIndex + 0];
				textureData.Pixels[dstIndex + 3] = (srcPixelSize == 4) ? srcPixels[srcIndex + 3] : 255;
			}
		}

		return textureData;
	}
}

DirectX12App::DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd) : m_hAppInst(mhAppInst), m_hMainWnd(mhMainWnd)
{
}

DirectX12App::~DirectX12App()
{
	m_debugOverlay.Shutdown();
}

bool DirectX12App::Initialize()
{
	RECT clientRect{};
	GetClientRect(m_hMainWnd, &clientRect);
	const int clientWidth = clientRect.right - clientRect.left;
	const int clientHeight = clientRect.bottom - clientRect.top;

	m_context.Initialize(
		m_hMainWnd,
		clientWidth,
		clientHeight,
		m4xMsaaState,
		m4xMsaaQuality,
		SwapChainBufferCount,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_D24_UNORM_S8_UINT);

	ApplyResize(clientWidth, clientHeight);

	ThrowIfFailed(m_context.GetCommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(m_context.GetCommandAllocator(), nullptr));

	BuildModelGeometry();
	BuildTextures();
	BuildDescriptorHeaps();
	m_deferredRenderer.Initialize(m_context, m4xMsaaState, m4xMsaaQuality);
	m_debugOverlay.Initialize(
		m_hMainWnd,
		m_context.GetDevice(),
		m_context.GetCommandQueue(),
		m_context.GetBackBufferFormat(),
		SwapChainBufferCount);

	ThrowIfFailed(m_context.GetCommandList()->Close());
	ID3D12CommandList* initCmdsLists[] = { m_context.GetCommandList() };
	m_context.ExecuteCommandLists(_countof(initCmdsLists), initCmdsLists);
	m_context.FlushCommandQueue();

	if (m_sceneGeo)
	{
		m_sceneGeo->DisposeUploaders();
	}
	for (auto& textureEntry : m_textures)
	{
		textureEntry.second->UploadHeap.Reset();
	}

	return true;
}

void DirectX12App::Update(const GameTimer& gt)
{
	UpdateMouseCaptureState();
	UpdateMouseLook();
	UpdateCamera(gt);
	UpdateMainPassCB(gt);
}

void DirectX12App::Draw(const GameTimer& gt)
{
	(void)gt;
	ThrowIfFailed(m_context.GetCommandAllocator()->Reset());
	ThrowIfFailed(m_context.GetCommandList()->Reset(m_context.GetCommandAllocator(), nullptr));

	auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(
		m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToRT);

	m_context.GetCommandList()->RSSetViewports(1, &m_context.GetViewport());
	m_context.GetCommandList()->RSSetScissorRects(1, &m_context.GetScissorRect());

	if (m_deferredRenderer.GetGbufferState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		m_deferredRenderer.TransitionGbuffer(m_context, m_deferredRenderer.GetGbufferState(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}
	m_deferredRenderer.DrawGeometryPass(
		m_context,
		m_srvDescriptorHeap.Get(),
		m_context.GetCbvSrvUavDescriptorSize(),
		*m_sceneGeo,
		m_modelDrawItems);
	m_deferredRenderer.TransitionGbuffer(m_context, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_deferredRenderer.SetGbufferState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	const float clearColor[] = { 0.03f, 0.05f, 0.08f, 1.0f };
	m_context.GetCommandList()->ClearRenderTargetView(m_context.CurrentBackBufferView(), clearColor, 0, nullptr);
	m_deferredRenderer.DrawLightingPass(m_context, m_debugOverlay.GetDebugViewMode());
	m_debugOverlay.Draw(
		m_context.GetCommandList(),
		gt,
		m_eyePos,
		m_lookDirection,
		m_yaw,
		m_pitch,
		m_cameraMoveSpeed,
		m_cameraMouseSensitivity,
		m_materialSystem,
		m_renderSettings,
		m_lightSystem);

	auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		m_context.CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_context.GetCommandList()->ResourceBarrier(1, &transitionToPresent);

	ThrowIfFailed(m_context.GetCommandList()->Close());

	ID3D12CommandList* cmds[] = { m_context.GetCommandList() };
	m_context.ExecuteCommandLists(_countof(cmds), cmds);

	m_context.Present();
	m_context.FlushCommandQueue();
}

void DirectX12App::ApplyResize(int width, int height)
{
	m_context.Resize(width, height);
	m_deferredRenderer.Resize(m_context);

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_proj, P);
}

void DirectX12App::OnWindowResize(int width, int height)
{
	if (width == m_context.GetClientWidth() && height == m_context.GetClientHeight())
		return;

	if (m_context.IsInitialized())
	{
		ApplyResize(width, height);
	}
}

float DirectX12App::AspectRatio() const
{
	return static_cast<float>(m_context.GetClientWidth()) / m_context.GetClientHeight();
}

void DirectX12App::BuildModelGeometry()
{
	const std::wstring modelPath = ResolveAssetPath(L"Assets\\sponza\\sponza.obj");
	std::vector<ObjModelLoader::MeshData> meshes = ObjModelLoader().Load(WStringToString(modelPath));
	if (meshes.empty())
	{
		throw std::runtime_error("No meshes were loaded from the OBJ model.");
	}

	std::vector<GeometryGenerator::Vertex> vertices;
	std::vector<std::uint32_t> indices;
	m_modelDrawItems.clear();

	XMFLOAT3 minPoint(
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)(),
		(std::numeric_limits<float>::max)());
	XMFLOAT3 maxPoint(
		-(std::numeric_limits<float>::max)(),
		-(std::numeric_limits<float>::max)(),
		-(std::numeric_limits<float>::max)());

	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		const ObjModelLoader::MeshData& mesh = meshes[meshIndex];
		const UINT baseVertexLocation = static_cast<UINT>(vertices.size());
		const UINT startIndexLocation = static_cast<UINT>(indices.size());

		vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
		for (std::uint32_t index : mesh.Indices32)
		{
			indices.push_back(index);
		}

		for (const GeometryGenerator::Vertex& vertex : mesh.Vertices)
		{
			minPoint.x = (std::min)(minPoint.x, vertex.Position.x);
			minPoint.y = (std::min)(minPoint.y, vertex.Position.y);
			minPoint.z = (std::min)(minPoint.z, vertex.Position.z);
			maxPoint.x = (std::max)(maxPoint.x, vertex.Position.x);
			maxPoint.y = (std::max)(maxPoint.y, vertex.Position.y);
			maxPoint.z = (std::max)(maxPoint.z, vertex.Position.z);
		}

		const std::string drawName = "mesh_" + std::to_string(meshIndex);
		SubmeshGeometry submesh;
		submesh.IndexCount = static_cast<UINT>(mesh.Indices32.size());
		submesh.StartIndexLocation = startIndexLocation;
		submesh.BaseVertexLocation = baseVertexLocation;

		DeferredRenderer::ModelDrawItem drawItem;
		drawItem.DrawName = drawName;
		drawItem.DiffuseTexturePath = mesh.DiffuseTexturePath;
		drawItem.HasAlphaCutout = mesh.HasAlphaCutout;

		m_modelDrawItems.push_back(std::move(drawItem));
	}

	m_sceneCenter = XMFLOAT3(
		0.5f * (minPoint.x + maxPoint.x),
		0.5f * (minPoint.y + maxPoint.y),
		0.5f * (minPoint.z + maxPoint.z));

	const float extentX = maxPoint.x - minPoint.x;
	const float extentY = maxPoint.y - minPoint.y;
	const float extentZ = maxPoint.z - minPoint.z;
	const float maxExtent = (std::max)(extentX, (std::max)(extentY, extentZ));
	m_sceneScale = maxExtent > 0.0f ? 20.0f / maxExtent : 1.0f;
	const float scaledHeight = extentY * m_sceneScale;
	const float scaledDepth = extentZ * m_sceneScale;
	const float cameraDistance = (std::max)(18.0f, scaledDepth + 12.0f);
	m_eyePos = XMFLOAT3(
		0.0f,
		(std::max)(6.0f, 0.35f * scaledHeight + 4.0f),
		-cameraDistance);
	m_lookDirection = XMFLOAT3(-m_eyePos.x, -m_eyePos.y, -m_eyePos.z);
	const XMVECTOR initialLook = XMVector3Normalize(XMLoadFloat3(&m_lookDirection));
	m_yaw = atan2f(XMVectorGetX(initialLook), XMVectorGetZ(initialLook));
	m_pitch = -asinf(XMVectorGetY(initialLook));

	const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(GeometryGenerator::Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint32_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "sponzaGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		m_context.GetDevice(), m_context.GetCommandList(), vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		m_context.GetDevice(), m_context.GetCommandList(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(GeometryGenerator::Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	for (size_t meshIndex = 0; meshIndex < m_modelDrawItems.size(); ++meshIndex)
	{
		const std::string drawName = m_modelDrawItems[meshIndex].DrawName;
		SubmeshGeometry submesh;
		submesh.IndexCount = static_cast<UINT>(meshes[meshIndex].Indices32.size());
		submesh.StartIndexLocation = static_cast<UINT>(0);
		submesh.BaseVertexLocation = 0;
		geo->DrawArgs[drawName] = submesh;
	}

	UINT runningBaseVertex = 0;
	UINT runningStartIndex = 0;
	for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
	{
		SubmeshGeometry& submesh = geo->DrawArgs[m_modelDrawItems[meshIndex].DrawName];
		submesh.IndexCount = static_cast<UINT>(meshes[meshIndex].Indices32.size());
		submesh.StartIndexLocation = runningStartIndex;
		submesh.BaseVertexLocation = runningBaseVertex;
		runningBaseVertex += static_cast<UINT>(meshes[meshIndex].Vertices.size());
		runningStartIndex += static_cast<UINT>(meshes[meshIndex].Indices32.size());
	}

	m_sceneGeo = std::move(geo);
}

void DirectX12App::CreateTextureResource(Texture& texture, const void* pixelData, UINT width, UINT height)
{
	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		width,
		height,
		1,
		1);

	ThrowIfFailed(m_context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&texture.Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(texture.Resource.Get(), 0, 1);

	ThrowIfFailed(m_context.GetDevice()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture.UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = pixelData;
	subresourceData.RowPitch = static_cast<LONG_PTR>(width * 4);
	subresourceData.SlicePitch = subresourceData.RowPitch * height;

	UpdateSubresources(
		m_context.GetCommandList(),
		texture.Resource.Get(),
		texture.UploadHeap.Get(),
		0,
		0,
		1,
		&subresourceData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
		texture.Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_context.GetCommandList()->ResourceBarrier(1, &transition);
}

void DirectX12App::BuildTextures()
{
	m_textures.clear();
	m_orderedTextures.clear();

	const std::array<std::uint8_t, 4> whitePixel = { 255, 255, 255, 255 };
	std::unordered_map<std::string, UINT> textureIndices;

	for (DeferredRenderer::ModelDrawItem& drawItem : m_modelDrawItems)
	{
		const std::string textureKey = drawItem.DiffuseTexturePath.empty() ? "__default_white__" : drawItem.DiffuseTexturePath;
		const auto existing = textureIndices.find(textureKey);
		if (existing != textureIndices.end())
		{
			drawItem.DiffuseSrvHeapIndex = existing->second;
			continue;
		}

		auto texture = std::make_unique<Texture>();
		texture->Name = textureKey;

		if (drawItem.DiffuseTexturePath.empty())
		{
			texture->Filename = L"default-white";
			CreateTextureResource(*texture, whitePixel.data(), 1, 1);
		}
		else
		{
			const std::wstring texturePath = AnsiToWString(drawItem.DiffuseTexturePath);
			const TgaTextureData textureData = LoadUncompressedTga(texturePath);
			texture->Filename = texturePath;
			CreateTextureResource(*texture, textureData.Pixels.data(), textureData.Width, textureData.Height);
		}

		const UINT textureIndex = static_cast<UINT>(m_orderedTextures.size());
		drawItem.DiffuseSrvHeapIndex = textureIndex;
		textureIndices[textureKey] = textureIndex;
		m_orderedTextures.push_back(texture.get());
		m_textures[textureKey] = std::move(texture);
	}
}

void DirectX12App::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = static_cast<UINT>(m_orderedTextures.size());
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_context.GetDevice()->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	for (Texture* texture : m_orderedTextures)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = texture->Resource->GetDesc().Format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

		m_context.GetDevice()->CreateShaderResourceView(texture->Resource.Get(), &srvDesc, handle);
		handle.Offset(1, m_context.GetCbvSrvUavDescriptorSize());
	}
}

void DirectX12App::UpdateCamera(const GameTimer& gt)
{
	const float sprintMultiplier = 3.0f;
	const float moveSpeed = m_cameraMoveSpeed *
		(d3dUtil::IsKeyDown(VK_SHIFT) ? sprintMultiplier : 1.0f) *
		gt.DeltaTime();

	XMVECTOR eyePosition = XMLoadFloat3(&m_eyePos);
	XMVECTOR lookDirection = GetSafeNormalizedDirection(m_lookDirection);
	const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMVECTOR rightDirection = XMVector3Normalize(XMVector3Cross(worldUp, lookDirection));

	if (d3dUtil::IsKeyDown('W'))
	{
		eyePosition += lookDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('S'))
	{
		eyePosition -= lookDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('A'))
	{
		eyePosition -= rightDirection * moveSpeed;
	}
	if (d3dUtil::IsKeyDown('D'))
	{
		eyePosition += rightDirection * moveSpeed;
	}

	XMStoreFloat3(&m_eyePos, eyePosition);
}

void DirectX12App::UpdateMouseCaptureState()
{
	const bool isWindowFocused = GetForegroundWindow() == m_hMainWnd;
	const bool isRightMouseDown = d3dUtil::IsKeyDown(VK_RBUTTON);
	const bool shouldCaptureMouse = isWindowFocused && isRightMouseDown;

	if (shouldCaptureMouse && !m_isMouseCaptured)
	{
		RECT clientRect{};
		GetClientRect(m_hMainWnd, &clientRect);

		POINT topLeft{ clientRect.left, clientRect.top };
		POINT bottomRight{ clientRect.right, clientRect.bottom };
		ClientToScreen(m_hMainWnd, &topLeft);
		ClientToScreen(m_hMainWnd, &bottomRight);

		RECT clipRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
		ClipCursor(&clipRect);
		ShowCursor(FALSE);

		const int centerX = (clipRect.left + clipRect.right) / 2;
		const int centerY = (clipRect.top + clipRect.bottom) / 2;
		SetCursorPos(centerX, centerY);

		m_isMouseCaptured = true;
	}
	else if (!shouldCaptureMouse && m_isMouseCaptured)
	{
		ClipCursor(nullptr);
		ShowCursor(TRUE);
		m_isMouseCaptured = false;
	}
}

void DirectX12App::UpdateMouseLook()
{
	if (!m_isMouseCaptured)
	{
		return;
	}

	RECT clientRect{};
	GetClientRect(m_hMainWnd, &clientRect);

	POINT centerPoint{
		(clientRect.left + clientRect.right) / 2,
		(clientRect.top + clientRect.bottom) / 2
	};
	ClientToScreen(m_hMainWnd, &centerPoint);

	POINT currentMousePosition{};
	if (!GetCursorPos(&currentMousePosition))
	{
		return;
	}

	const float deltaX = static_cast<float>(currentMousePosition.x - centerPoint.x);
	const float deltaY = static_cast<float>(currentMousePosition.y - centerPoint.y);

	m_yaw += deltaX * m_cameraMouseSensitivity;
	m_pitch += deltaY * m_cameraMouseSensitivity;
	m_pitch = (std::max)(-1.45f, (std::min)(1.45f, m_pitch));

	const float cosPitch = cosf(m_pitch);
	m_lookDirection.x = sinf(m_yaw) * cosPitch;
	m_lookDirection.y = -sinf(m_pitch);
	m_lookDirection.z = cosf(m_yaw) * cosPitch;

	SetCursorPos(centerPoint.x, centerPoint.y);
}

void DirectX12App::UpdateMainPassCB(const GameTimer& gt)
{
	(void)gt;

	XMVECTOR eyePos = XMLoadFloat3(&m_eyePos);
	const XMVECTOR safeLookDirection = GetSafeNormalizedDirection(m_lookDirection);
	XMStoreFloat3(&m_lookDirection, safeLookDirection);
	m_lightSystem.Update(m_eyePos, m_lookDirection, m_sceneCenter);

	DeferredRenderer::FrameData frameData;
	frameData.EyePos = m_eyePos;
	frameData.LookDirection = m_lookDirection;
	frameData.SceneCenter = m_sceneCenter;
	frameData.SceneScale = m_sceneScale;
	frameData.Projection = m_proj;
	frameData.LightingSettings = m_renderSettings.GetLightingSettings();
	frameData.Material = m_materialSystem.GetMaterialState();
	frameData.LightState = m_lightSystem.GetLightingState();

	m_deferredRenderer.UpdateMainPassCB(frameData);
}
