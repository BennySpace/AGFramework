#pragma once

#include "d3dUtil.h"

#if defined(DEBUG) || defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

class GameTimer;

class DirectX12App
{
public:
	DirectX12App(HINSTANCE mhAppInst, HWND mhMainWnd);
	virtual ~DirectX12App();

	virtual bool Initialize();
	virtual void Update(const GameTimer& gt);
	virtual void Draw(const GameTimer& gt);
	virtual void OnResize();

	void OnWindowResize(int width, int height);

	float AspectRatio() const;

protected:
	virtual void CreateRtvAndDsvDescriptorHeaps();
	void CreateCommandObjects();
	void CreateSwapChain();
	void FlushCommandQueue();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	void BuildShadersAndInputLayout();
	void BuildBoxGeometry();
	void BuildConstantBuffer();
	void BuildRootSignature();
	void BuildPSO();
	void UpdateMainPassCB(const GameTimer& gt);

private:
	void LogAdapters();
	void LogAdapterOutputs(IDXGIAdapter* adapter);
	void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 WorldInvTranspose = MathHelper::Identity4x4();
		DirectX::XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
		DirectX::XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
		float Pad0 = 0.0f;
		DirectX::XMFLOAT4 AmbientLight = { 0.15f, 0.15f, 0.2f, 1.0f };
		DirectX::XMFLOAT4 LightDir = { 0.577f, -0.577f, 0.577f, 0.0f };
		DirectX::XMFLOAT4 LightColor = { 0.85f, 0.85f, 0.8f, 1.0f };
		DirectX::XMFLOAT4 DiffuseAlbedo = { 0.78f, 0.24f, 0.18f, 1.0f };
		DirectX::XMFLOAT4 SpecularAlbedo = { 0.85f, 0.85f, 0.85f, 32.0f };
	};

protected:
	HINSTANCE m_hAppInst = nullptr;
	HWND      m_hMainWnd = nullptr;

	bool      m4xMsaaState = false;
	UINT      m4xMsaaQuality = 0;

	static const int SwapChainBufferCount = 2;

	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;

	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[SwapChainBufferCount];
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;

	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	UINT m_rtvDescriptorSize = 0;
	UINT m_dsvDescriptorSize = 0;
	UINT m_cbvSrvUavDescriptorSize = 0;

	int m_currBackBuffer = 0;
	int m_clientWidth = 1920;
	int m_clientHeight = 1080;

	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_objectCB;

	std::unique_ptr<MeshGeometry> m_boxGeo;
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3DBlob>> m_shaders;
	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;

	UINT8* m_mappedObjectCB = nullptr;
	UINT m_objectCBByteSize = 0;

	DirectX::XMFLOAT4X4 m_world = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_view = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 m_proj = MathHelper::Identity4x4();
	DirectX::XMFLOAT3 m_eyePos = { 0.0f, 2.0f, -6.0f };
	float m_theta = 0.0f;
};
