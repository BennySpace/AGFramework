#pragma once

#include "d3dUtil.h"

class DirectX12Context
{
  public:
	DirectX12Context() = default;
	~DirectX12Context();

	void Initialize(HWND windowHandle, int clientWidth, int clientHeight, bool enable4xMsaa, UINT msaaQuality, UINT swapChainBufferCount,
	                DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthStencilFormat);
	void Resize(int clientWidth, int clientHeight);
	void FlushCommandQueue();
	void ExecuteCommandLists(UINT commandListCount, ID3D12CommandList *const *commandLists) const;
	void Present(UINT syncInterval = 0, UINT flags = 0);

	ID3D12Device *GetDevice() const
	{
		return m_device.Get();
	}
	IDXGIFactory4 *GetFactory() const
	{
		return m_dxgiFactory.Get();
	}
	ID3D12CommandAllocator *GetCommandAllocator() const
	{
		return m_commandAllocator.Get();
	}
	ID3D12CommandQueue *GetCommandQueue() const
	{
		return m_commandQueue.Get();
	}
	ID3D12GraphicsCommandList *GetCommandList() const
	{
		return m_commandList.Get();
	}

	ID3D12Resource *CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;

	const D3D12_VIEWPORT &GetViewport() const
	{
		return m_viewport;
	}
	const D3D12_RECT &GetScissorRect() const
	{
		return m_scissorRect;
	}

	UINT GetRtvDescriptorSize() const
	{
		return m_rtvDescriptorSize;
	}
	UINT GetDsvDescriptorSize() const
	{
		return m_dsvDescriptorSize;
	}
	UINT GetCbvSrvUavDescriptorSize() const
	{
		return m_cbvSrvUavDescriptorSize;
	}

	int GetClientWidth() const
	{
		return m_clientWidth;
	}
	int GetClientHeight() const
	{
		return m_clientHeight;
	}
	DXGI_FORMAT GetBackBufferFormat() const
	{
		return m_backBufferFormat;
	}
	DXGI_FORMAT GetDepthStencilFormat() const
	{
		return m_depthStencilFormat;
	}

	bool IsInitialized() const
	{
		return m_device != nullptr;
	}

  private:
	void CreateCommandObjects();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDevice();

	HWND m_windowHandle = nullptr;
	bool m4xMsaaState = false;
	UINT m4xMsaaQuality = 0;
	UINT m_swapChainBufferCount = 0;
	int m_clientWidth = 0;
	int m_clientHeight = 0;
	DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
	Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTargets;
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
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue = 0;
};
