#include "DirectX12App.h"
#include "../../Core/GameTimer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace
{
	struct TgaTextureData
	{
		UINT Width = 0;
		UINT Height = 0;
		DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
	RECT r{};
	GetClientRect(mhMainWnd, &r);
	m_clientWidth = r.right - r.left;
	m_clientHeight = r.bottom - r.top;
}

DirectX12App::~DirectX12App()
{
	if (m_objectCB != nullptr && m_mappedObjectCB != nullptr)
	{
		m_objectCB->Unmap(0, nullptr);
		m_mappedObjectCB = nullptr;
	}

	if (m_device) FlushCommandQueue();
}

bool DirectX12App::Initialize()
{
#if defined(DEBUG) || defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			debugController->EnableDebugLayer();
	}
#endif

	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
	if (FAILED(hr))
	{
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
		ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	}

	ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	CreateCommandObjects();
	CreateSwapChain();
	CreateRtvAndDsvDescriptorHeaps();

	OnResize(); // Initial setup

	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	BuildShadersAndInputLayout();
	BuildBoxGeometry();
	BuildTexture();
	BuildDescriptorHeaps();
	BuildConstantBuffer();
	BuildRootSignature();
	BuildPSO();

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* initCmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(initCmdsLists), initCmdsLists);
	FlushCommandQueue();

	if (m_boxGeo)
	{
		m_boxGeo->DisposeUploaders();
	}
	if (m_diffuseTexture)
	{
		m_diffuseTexture->UploadHeap.Reset();
	}

	return true;
}

void DirectX12App::Update(const GameTimer& gt)
{
	UpdateMainPassCB(gt);
}

void DirectX12App::Draw(const GameTimer& gt)
{
	ThrowIfFailed(m_commandAllocator->Reset());
	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()));

	auto transitionToRT = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_commandList->ResourceBarrier(1, &transitionToRT);

	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);

	const float clearColor[] = { 0.2f, 0.4f, 0.8f, 1.0f }; // Nice blue
	m_commandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);
	m_commandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	const D3D12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = CurrentBackBufferView();
	const D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = DepthStencilView();
	m_commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	ID3D12DescriptorHeap* descriptorHeaps[] = { m_srvDescriptorHeap.Get() };
	m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	m_commandList->SetGraphicsRootDescriptorTable(1, m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const D3D12_VERTEX_BUFFER_VIEW vertexBufferView = m_boxGeo->VertexBufferView();
	const D3D12_INDEX_BUFFER_VIEW indexBufferView = m_boxGeo->IndexBufferView();
	m_commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
	m_commandList->IASetIndexBuffer(&indexBufferView);
	m_commandList->SetGraphicsRootConstantBufferView(0, m_objectCB->GetGPUVirtualAddress());
	const auto& boxSubmesh = m_boxGeo->DrawArgs["box"];
	m_commandList->DrawIndexedInstanced(boxSubmesh.IndexCount, 1, boxSubmesh.StartIndexLocation, boxSubmesh.BaseVertexLocation, 0);

	auto transitionToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_commandList->ResourceBarrier(1, &transitionToPresent);

	ThrowIfFailed(m_commandList->Close());

	ID3D12CommandList* cmds[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmds), cmds);

	ThrowIfFailed(m_swapChain->Present(0, 0));
	m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

	FlushCommandQueue();
}

void DirectX12App::OnResize()
{
	assert(m_device);
	assert(m_swapChain);
	assert(m_commandAllocator);

	FlushCommandQueue();

	ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	for (int i = 0; i < SwapChainBufferCount; ++i)
		m_renderTargets[i].Reset();
	m_depthStencilBuffer.Reset();

	ThrowIfFailed(m_swapChain->ResizeBuffers(
		SwapChainBufferCount, m_clientWidth, m_clientHeight,
		m_backBufferFormat, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	m_currBackBuffer = 0;

	// Recreate RTVs
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHeapHandle);
		rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
	}

	// Recreate Depth Stencil
	D3D12_RESOURCE_DESC depthStencilDesc = {};
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Width = m_clientWidth;
	depthStencilDesc.Height = m_clientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear{};
	optClear.Format = m_depthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optClear,
		IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = m_depthStencilFormat;
	m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

	ThrowIfFailed(m_commandList->Close());
	ID3D12CommandList* cmds[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmds), cmds);
	FlushCommandQueue();

	m_viewport = { 0.0f, 0.0f, (float)m_clientWidth, (float)m_clientHeight, 0.0f, 1.0f };
	m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };

	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * XM_PI, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&m_proj, P);
}

void DirectX12App::OnWindowResize(int width, int height)
{
	if (width == m_clientWidth && height == m_clientHeight)
		return;

	m_clientWidth = width;
	m_clientHeight = height;

	if (m_device != nullptr)
		OnResize();
}

float DirectX12App::AspectRatio()const
{
	return static_cast<float>(m_clientWidth) / m_clientHeight;
}

void DirectX12App::CreateSwapChain()
{
	m_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = m_clientWidth;
	sd.BufferDesc.Height = m_clientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = m_backBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = m_hMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ThrowIfFailed(m_dxgiFactory->CreateSwapChain(
		m_commandQueue.Get(),
		&sd,
		m_swapChain.GetAddressOf()));
}

void DirectX12App::FlushCommandQueue()
{
	m_fenceValue++;

	ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));

	if (m_fence->GetCompletedValue() < m_fenceValue)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValue, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

ID3D12Resource* DirectX12App::CurrentBackBuffer()const
{
	return m_renderTargets[m_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12App::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
		m_currBackBuffer,
		m_rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12App::DepthStencilView()const
{
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void DirectX12App::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
}

void DirectX12App::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

	ThrowIfFailed(m_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(m_commandAllocator.GetAddressOf())));

	ThrowIfFailed(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_commandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(m_commandList.GetAddressOf())));

	m_commandList->Close();
}

void DirectX12App::LogAdapters()
{
	UINT i = 0;
	IDXGIAdapter* adapter = nullptr;
	std::vector<IDXGIAdapter*> adapterList;
	while (m_dxgiFactory->EnumAdapters(i, &adapter) !=
		DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC desc;
		adapter->GetDesc(&desc);
		std::wstring text = L"***Adapter: ";
		text += desc.Description;
		text += L"\n";
		OutputDebugString(text.c_str());
		adapterList.push_back(adapter);
		++i;
	}
	for (size_t i = 0; i < adapterList.size(); ++i)
	{
		LogAdapterOutputs(adapterList[i]);
		ReleaseCom(adapterList[i]);
	}
}

void DirectX12App::LogAdapterOutputs(IDXGIAdapter* adapter)
{
	UINT i = 0;
	IDXGIOutput* output = nullptr;
	while (adapter->EnumOutputs(i, &output) !=
		DXGI_ERROR_NOT_FOUND)
	{
		DXGI_OUTPUT_DESC desc;
		output->GetDesc(&desc);
		std::wstring text = L"***Output: ";
		text += desc.DeviceName;
		text += L"\n";
		OutputDebugString(text.c_str());
		LogOutputDisplayModes(output,
			DXGI_FORMAT_B8G8R8A8_UNORM);
		ReleaseCom(output);
		++i;
	}
}

void DirectX12App::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
	UINT count = 0;
	UINT flags = 0;
	output->GetDisplayModeList(format, flags, &count,
		nullptr);
	std::vector<DXGI_MODE_DESC> modeList(count);
	output->GetDisplayModeList(format, flags, &count,
		&modeList[0]);
	for (auto& x : modeList)
	{
		UINT n = x.RefreshRate.Numerator;
		UINT d = x.RefreshRate.Denominator;
		std::wstring text =
			L"Width = " + std::to_wstring(x.Width) + L" " +
			L"Height = " + std::to_wstring(x.Height) + L" "
			+
			L"Refresh = " + std::to_wstring(n) + L"/" +
			std::to_wstring(d) +
			L"\n";
		::OutputDebugString(text.c_str());
	}
}

void DirectX12App::BuildShadersAndInputLayout()
{
	m_shaders["standardVS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\Phong.hlsl"), nullptr, "VS", "vs_5_1");
	m_shaders["opaquePS"] = d3dUtil::CompileShader(
		ResolveShaderPath(L"shaders\\Phong.hlsl"), nullptr, "PS", "ps_5_1");

	m_inputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, Position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(GeometryGenerator::Vertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void DirectX12App::BuildBoxGeometry()
{
	auto box = GeometryGenerator().CreateBox(2.0f, 2.0f, 2.0f, 0);
	const std::vector<std::uint16_t>& indices = box.GetIndices16();

	const UINT vbByteSize = static_cast<UINT>(box.Vertices.size() * sizeof(GeometryGenerator::Vertex));
	const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), box.Vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		m_device.Get(), m_commandList.Get(), box.Vertices.data(), vbByteSize, geo->VertexBufferUploader);
	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		m_device.Get(), m_commandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(GeometryGenerator::Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = static_cast<UINT>(indices.size());
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;
	m_boxGeo = std::move(geo);
}

void DirectX12App::BuildTexture()
{
	const std::wstring texturePath = ResolveAssetPath(L"Assets\\sponza\\textures\\spnza_bricks_a_diff.tga");
	const TgaTextureData textureData = LoadUncompressedTga(texturePath);

	m_diffuseTexture = std::make_unique<Texture>();
	m_diffuseTexture->Name = "sponzaBricksDiffuse";
	m_diffuseTexture->Filename = texturePath;

	const auto textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
		textureData.Format,
		textureData.Width,
		textureData.Height,
		1,
		1);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&textureDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_diffuseTexture->Resource)));

	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_diffuseTexture->Resource.Get(), 0, 1);

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_diffuseTexture->UploadHeap)));

	D3D12_SUBRESOURCE_DATA subresourceData = {};
	subresourceData.pData = textureData.Pixels.data();
	subresourceData.RowPitch = static_cast<LONG_PTR>(textureData.Width * 4);
	subresourceData.SlicePitch = subresourceData.RowPitch * textureData.Height;

	UpdateSubresources(
		m_commandList.Get(),
		m_diffuseTexture->Resource.Get(),
		m_diffuseTexture->UploadHeap.Get(),
		0,
		0,
		1,
		&subresourceData);

	auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
		m_diffuseTexture->Resource.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	m_commandList->ResourceBarrier(1, &transition);
}

void DirectX12App::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_diffuseTexture->Resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	m_device->CreateShaderResourceView(
		m_diffuseTexture->Resource.Get(),
		&srvDesc,
		m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

void DirectX12App::BuildConstantBuffer()
{
	m_objectCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	ThrowIfFailed(m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(m_objectCBByteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_objectCB)));

	ThrowIfFailed(m_objectCB->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedObjectCB)));
}

void DirectX12App::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC linearWrapSampler(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		2,
		slotRootParameter,
		1,
		&linearWrapSampler,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(),
		errorBlob.GetAddressOf()));

	ThrowIfFailed(m_device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

void DirectX12App::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
	psoDesc.pRootSignature = m_rootSignature.Get();
	psoDesc.VS =
	{
		reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()),
		m_shaders["standardVS"]->GetBufferSize()
	};
	psoDesc.PS =
	{
		reinterpret_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer()),
		m_shaders["opaquePS"]->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = m_backBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = m_depthStencilFormat;

	ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
}

void DirectX12App::UpdateMainPassCB(const GameTimer& gt)
{
	m_theta += 0.8f * gt.DeltaTime();

	XMMATRIX world = XMMatrixRotationY(m_theta) * XMMatrixRotationX(0.35f * m_theta);
	XMVECTOR eyePos = XMLoadFloat3(&m_eyePos);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(eyePos, target, up);
	XMMATRIX proj = XMLoadFloat4x4(&m_proj);
	XMMATRIX worldInvTranspose = MathHelper::InverseTranspose(world);
	XMMATRIX worldViewProj = world * view * proj;

	XMStoreFloat4x4(&m_world, XMMatrixTranspose(world));
	XMStoreFloat4x4(&m_view, XMMatrixTranspose(view));

	ObjectConstants objConstants;
	XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
	XMStoreFloat4x4(&objConstants.WorldInvTranspose, XMMatrixTranspose(worldInvTranspose));
	XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
	objConstants.EyePosW = m_eyePos;

	memcpy(m_mappedObjectCB, &objConstants, sizeof(objConstants));
}
