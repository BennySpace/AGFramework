#pragma once

#include <d3d12.h>
#include <wrl.h>

// Identifies how a shader interprets texels, so uploads retain the correct color space.
enum class TextureUsage
{
	Color,
	Normal,
	Material,
	Opacity,
};

struct Texture
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
