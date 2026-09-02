#pragma once

#include <d3d12.h>
#include <wrl.h>

struct Texture
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
